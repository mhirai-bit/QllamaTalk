#include "VoiceRecognitionEngine.h"
#include "common.h"   // vad_simple, whisper
#include "whisper.h"

#include <QDebug>
#include <QThread>
#include <cstring>
#include <chrono>

VoiceRecognitionEngine::VoiceRecognitionEngine(QObject *parent)
    : QObject(parent)
{
}

VoiceRecognitionEngine::~VoiceRecognitionEngine()
{
    stop();
    if (m_ctx) {
        whisper_free(m_ctx);
        m_ctx = nullptr;
    }
}

bool VoiceRecognitionEngine::initWhisper(const VoiceRecParams &params)
{
    m_params = params;
    whisper_context_params cparams = whisper_context_default_params();
    cparams.use_gpu    = m_params.use_gpu;
    cparams.flash_attn = m_params.flash_attn;

    m_ctx = whisper_init_from_file_with_params(m_params.model.c_str(), cparams);
    if (!m_ctx) {
        qWarning() << "[VoiceRecognitionEngine] Failed to init whisper from"
                   << QString::fromStdString(m_params.model);
        return false;
    }
    qDebug() << "[VoiceRecognitionEngine] Whisper inited. Model:"
             << QString::fromStdString(m_params.model);
    return true;
}

void VoiceRecognitionEngine::addAudio(const std::vector<float> & pcms)
{
    // 単純に追加
    m_capturedAudio.insert(m_capturedAudio.end(), pcms.begin(), pcms.end());
    qDebug() << "[VoiceRecognitionEngine] addAudio() => added"
             << pcms.size() << "samples. total buffer size =" << m_capturedAudio.size();
}

void VoiceRecognitionEngine::start()
{
    if (m_running) {
        qWarning() << "[VoiceRecognitionEngine] Already running";
        return;
    }
    if (!m_ctx) {
        qWarning() << "[VoiceRecognitionEngine] Please initWhisper() first";
        return;
    }
    m_running = true;
    m_t_last = std::chrono::steady_clock::now();

    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, &VoiceRecognitionEngine::processVadCheck);
    m_timer->start(200); // 200msごとにVADチェック

    qDebug() << "[VoiceRecognitionEngine] start() done.";
}

void VoiceRecognitionEngine::stop()
{
    if (!m_running) return;
    m_running = false;

    if (m_timer) {
        m_timer->stop();
        m_timer->deleteLater();
        m_timer = nullptr;
    }
    qDebug() << "[VoiceRecognitionEngine] stop() done.";
}

void VoiceRecognitionEngine::processVadCheck()
{
    if (!m_running) return;
    qDebug() << "[VoiceRecognitionEngine] processVadCheck. buffer size =" << m_capturedAudio.size();

    if (m_capturedAudio.size() < 16000 * 2) {
        // 2秒分ないとVADチェックできない (例)
        return;
    }
    auto t_now = std::chrono::steady_clock::now();
    auto t_diff = std::chrono::duration_cast<std::chrono::milliseconds>(t_now - m_t_last).count();
    qDebug() << "[VoiceRecognitionEngine] t_diff =" << t_diff << "ms since last check";
    if (t_diff < 2000) {
        return;
    }
    m_t_last = t_now;

    // 直近2秒分コピー
    const int n_get = 16000 * 2;
    if (int(m_capturedAudio.size()) < n_get) return;

    std::vector<float> pcmf32_new(n_get);
    memcpy(pcmf32_new.data(), &m_capturedAudio[m_capturedAudio.size() - n_get], n_get * sizeof(float));

    // 簡易VAD
    if (!vad_simple(pcmf32_new, COMMON_SAMPLE_RATE, 1000,
                    m_params.vad_thold, m_params.freq_thold, false)) {
        qDebug() << "[VoiceRecognitionEngine] VAD => no speech detected.";
        // 無音
        return;
    }

    // 音声アリ → length_ms分を取り出して認識
    const int n_len = (COMMON_SAMPLE_RATE * m_params.length_ms) / 1000;
    if (int(m_capturedAudio.size()) < n_len) {
        // データが不十分
        qDebug() << "[VoiceRecognitionEngine] Not enough data for inference yet.";
        return;
    }

    std::vector<float> pcmf32(n_len);
    memcpy(pcmf32.data(), &m_capturedAudio[m_capturedAudio.size() - n_len], n_len*sizeof(float));

    qDebug() << "[VoiceRecognitionEngine] VAD => speech detected. Running whisper...";
    runWhisper(pcmf32);
}

void VoiceRecognitionEngine::runWhisper(const std::vector<float> & pcmf32)
{
    if (!m_ctx) return;

    whisper_full_params wparams = whisper_full_default_params(WHISPER_SAMPLING_GREEDY);
    wparams.print_progress   = false;
    wparams.print_special    = false;
    wparams.print_realtime   = false;
    wparams.print_timestamps = false;
    wparams.translate        = false;
    wparams.single_segment   = true;
    wparams.language         = m_params.language.c_str();
    wparams.n_threads        = 4; // 例

    int ret = whisper_full(m_ctx, wparams, pcmf32.data(), pcmf32.size());
    if (ret != 0) {
        qWarning() << "[VoiceRecognitionEngine] whisper_full failed with code:" << ret;
        return;
    }
    // テキストをまとめる
    QString result;
    int n_segments = whisper_full_n_segments(m_ctx);
    for (int i = 0; i < n_segments; i++) {
        const char* seg_txt = whisper_full_get_segment_text(m_ctx, i);
        result += QString::fromUtf8(seg_txt);
    }

    // デバッグ出力
    qDebug() << "[VoiceRecognitionEngine] recognized text:" << result;

    // シグナルで外部に伝える
    emit textRecognized(result);
}

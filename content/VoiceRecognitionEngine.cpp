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
    m_whisper_params = params;
    whisper_context_params cparams = whisper_context_default_params();
    cparams.use_gpu    = m_whisper_params.use_gpu;
    cparams.flash_attn = m_whisper_params.flash_attn;

    m_ctx = whisper_init_from_file_with_params(m_whisper_params.model.c_str(), cparams);
    if (!m_ctx) {
        qWarning() << "[VoiceRecognitionEngine] Failed to init whisper from"
                   << QString::fromStdString(m_whisper_params.model);
        return false;
    }
    qDebug() << "[VoiceRecognitionEngine] Whisper inited. Model:"
             << QString::fromStdString(m_whisper_params.model);
    return true;
}

void VoiceRecognitionEngine::addAudio(const std::vector<float> & pcms)
{
    // 単純に追加
    m_capturedAudio.insert(m_capturedAudio.end(), pcms.begin(), pcms.end());
    // qDebug() << "[VoiceRecognitionEngine] addAudio() => added"
    //          << pcms.size() << "samples. total buffer size =" << m_capturedAudio.size();
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

    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, &VoiceRecognitionEngine::processVadCheck);
    m_timer->start(2000); // 2000msごとにVADチェック

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

void VoiceRecognitionEngine::setLanguage(const QString &language) {
    std::string langStd = language.toStdString();
    // チェック
    int langId = whisper_lang_id(langStd.c_str());
    if (langId < 0) {
        qWarning() << "Invalid whisper language code:" << language;
        return; // or fallback
    }

    m_whisper_params.language = langStd; // 有効なのでセット
}

void VoiceRecognitionEngine::processVadCheck()
{
    if (!m_running) return;
    // qDebug() << "[VoiceRecognitionEngine] processVadCheck. buffer size =" << m_capturedAudio.size();

    if (m_capturedAudio.size() < COMMON_SAMPLE_RATE * 2) {
        // 2秒分ないとVADチェックできない (例)
        return;
    }

    // 直近2秒分コピー
    const int sample_counts_for_VAD_check = COMMON_SAMPLE_RATE * 2;
    if (int(m_capturedAudio.size()) < sample_counts_for_VAD_check) return;

    std::vector<float> audio_for_inference_new(sample_counts_for_VAD_check);
    memcpy(audio_for_inference_new.data(), &m_capturedAudio[m_capturedAudio.size() - sample_counts_for_VAD_check], sample_counts_for_VAD_check * sizeof(float));

    // 簡易VAD
    if (!vad_simple(audio_for_inference_new, COMMON_SAMPLE_RATE, 1000,
                    m_whisper_params.vad_thold, m_whisper_params.freq_thold, false)) {
        qDebug() << "[VoiceRecognitionEngine] VAD => no speech detected.";
        // 無音
        return;
    }

    // 音声アリ → length_ms分を取り出して認識
    const int samples_count_for_inference = (COMMON_SAMPLE_RATE * m_whisper_params.length_for_inference_ms) / 1000;
    if (int(m_capturedAudio.size()) < samples_count_for_inference) {
        // データが不十分
        qDebug() << "[VoiceRecognitionEngine] Not enough data for inference yet.";
        return;
    }

    std::vector<float> audio_for_inference(samples_count_for_inference);
    memcpy(audio_for_inference.data(), &m_capturedAudio[m_capturedAudio.size() - samples_count_for_inference], samples_count_for_inference*sizeof(float));

    // qDebug() << "[VoiceRecognitionEngine] VAD => speech detected. Running whisper...";
    runWhisper(audio_for_inference);
}

void VoiceRecognitionEngine::runWhisper(const std::vector<float> & audio_for_inference)
{
    if (!m_ctx) return;

    whisper_full_params wparams = whisper_full_default_params(WHISPER_SAMPLING_GREEDY);
    wparams.print_progress   = false;
    wparams.print_special    = false;
    wparams.print_realtime   = false;
    wparams.print_timestamps = false;
    wparams.translate        = false;
    wparams.single_segment   = true;
    wparams.language         = m_whisper_params.language.c_str();
    wparams.n_threads        = 4; // 例

    int ret = whisper_full(m_ctx, wparams, audio_for_inference.data(), audio_for_inference.size());
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
    // qDebug() << "[VoiceRecognitionEngine] recognized text:" << result;

    // シグナルで外部に伝える
    emit textRecognized(result);
}

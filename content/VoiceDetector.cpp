#include "VoiceDetector.h"

#include <QAudioFormat>
#include <QDebug>
#include <QThread>
#include <algorithm>
#include <cstring> // memcpy

VoiceDetector::VoiceDetector(int len_ms, QObject *parent)
    : QObject(parent)
    , m_len_ms(len_ms)
{
    m_running.store(false);
}

VoiceDetector::~VoiceDetector()
{
    // 終了時にオーディオソースを停止して破棄
    if (m_audioSource) {
        m_audioSource->stop();
        delete m_audioSource;
        m_audioSource = nullptr;
    }
}

bool VoiceDetector::init(int sampleRate, int channelCount)
{
    if (m_initialized) {
        qWarning() << "[VoiceDetector] already initialized";
        return false;
    }

    m_sample_rate = sampleRate;

    // リングバッファのサイズを確保 (サンプルレート * len_ms / 1000)
    size_t bufferSize = static_cast<size_t>(m_sample_rate) * m_len_ms / 1000 * channelCount;
    m_audio.resize(bufferSize, 0.0f);
    m_audio_pos = 0;
    m_audio_len = 0;

    // QAudioFormat 設定
    QAudioFormat format;
    format.setSampleRate(m_sample_rate);
    format.setChannelCount(channelCount);

    // Qt6以降なら setSampleFormat(QAudioFormat::Float) が使える
    // ただし環境によってはFloat未対応で fallback する可能性あり
    format.setSampleFormat(QAudioFormat::Float);

    // QAudioSourceの作成
    m_audioSource = new QAudioSource(format, this);
    if (!m_audioSource) {
        qWarning() << "[VoiceDetector] Failed to create QAudioSource";
        return false;
    }

    // QAudioSource のデバイス(読み取り用)を取得
    m_audioDevice = m_audioSource->start();
    if (!m_audioDevice) {
        qWarning() << "[VoiceDetector] Failed to start QAudioSource";
        delete m_audioSource;
        m_audioSource = nullptr;
        return false;
    }

    // readyRead() シグナルに接続
    connect(m_audioDevice, &QIODevice::readyRead,
            this, &VoiceDetector::onDataAvailable);

    m_initialized = true;
    qDebug() << "[VoiceDetector] init done. sampleRate=" << m_sample_rate
             << ", channelCount=" << channelCount
             << ", bufferSize=" << bufferSize;
    return true;
}

bool VoiceDetector::resume()
{
    if (!m_initialized) {
        qWarning() << "[VoiceDetector] not initialized";
        return false;
    }
    if (m_running.load()) {
        qWarning() << "[VoiceDetector] already running";
        return false;
    }

    // QAudioSourceを再開
    m_audioSource->resume();
    m_running.store(true);

    qDebug() << "[VoiceDetector] resume capturing";
    return true;
}

bool VoiceDetector::pause()
{
    if (!m_initialized) {
        qWarning() << "[VoiceDetector] not initialized";
        return false;
    }
    if (!m_running.load()) {
        qWarning() << "[VoiceDetector] already paused";
        return false;
    }

    // 一時停止
    m_audioSource->suspend();
    m_running.store(false);

    qDebug() << "[VoiceDetector] pause capturing";
    return true;
}

bool VoiceDetector::clear()
{
    if (!m_initialized) {
        qWarning() << "[VoiceDetector] not initialized";
        return false;
    }
    if (!m_running.load()) {
        qWarning() << "[VoiceDetector] not running, cannot clear buffer";
        return false;
    }

    QMutexLocker locker(&m_mutex);
    m_audio_pos = 0;
    m_audio_len = 0;
    std::fill(m_audio.begin(), m_audio.end(), 0.0f);
    qDebug() << "[VoiceDetector] buffer cleared";
    return true;
}

void VoiceDetector::onDataAvailable()
{
    if (!m_running.load()) {
        // 動作していないならバッファを読み捨て
        m_audioDevice->readAll();
        return;
    }

    const qint64 bytesAvail = m_audioDevice->bytesAvailable();
    if (bytesAvail <= 0) {
        return;
    }

    // まとめて読み込む
    QByteArray ba = m_audioDevice->readAll();
    qint64 len = ba.size();
    if (len == 0) {
        return;
    }

    // Floatサンプルとして解釈
    size_t n_samples = static_cast<size_t>(len / sizeof(float));
    const float *samples = reinterpret_cast<const float*>(ba.constData());

    // ここで "新たに読み取った chunk" をシグナルで emit したい場合:
    {
        std::vector<float> chunkVec(n_samples);
        for (size_t i = 0; i < n_samples; i++) {
            chunkVec[i] = samples[i];
        }
        emit audioAvailable(chunkVec);
    }

    // --- リングバッファへ保存 ---
    QMutexLocker locker(&m_mutex);

    if (n_samples > m_audio.size()) {
        // 入りきらない場合は最後の分だけ
        size_t discard = n_samples - m_audio.size();
        samples += discard;
        n_samples = m_audio.size();
    }

    if (m_audio_pos + n_samples > m_audio.size()) {
        const size_t n0 = m_audio.size() - m_audio_pos;
        std::memcpy(&m_audio[m_audio_pos], samples, n0 * sizeof(float));
        std::memcpy(&m_audio[0], samples + n0, (n_samples - n0) * sizeof(float));
        m_audio_pos = (m_audio_pos + n_samples) % m_audio.size();
        m_audio_len = m_audio.size(); // バッファ満杯
    } else {
        std::memcpy(&m_audio[m_audio_pos], samples, n_samples * sizeof(float));
        m_audio_pos = (m_audio_pos + n_samples) % m_audio.size();
        m_audio_len = std::min(m_audio_len + n_samples, m_audio.size());
    }
}

void VoiceDetector::get(int ms, std::vector<float> & result)
{
    if (!m_initialized) {
        qWarning() << "[VoiceDetector] not initialized";
        return;
    }
    if (!m_running.load()) {
        qWarning() << "[VoiceDetector] not running. cannot get()";
        return;
    }

    QMutexLocker locker(&m_mutex);

    result.clear();
    if (ms <= 0) {
        ms = m_len_ms;
    }

    size_t n_samples = static_cast<size_t>(
        double(m_sample_rate) * ms / 1000.0
        );
    if (n_samples > m_audio_len) {
        n_samples = m_audio_len;
    }

    result.resize(n_samples);

    // リングバッファの末尾から n_samples 分をコピー
    int s0 = static_cast<int>(m_audio_pos) - static_cast<int>(n_samples);
    if (s0 < 0) {
        s0 += m_audio.size();
    }

    if (static_cast<size_t>(s0) + n_samples > m_audio.size()) {
        size_t n0 = m_audio.size() - s0;
        std::memcpy(result.data(), &m_audio[s0], n0 * sizeof(float));
        std::memcpy(result.data() + n0, &m_audio[0], (n_samples - n0) * sizeof(float));
    } else {
        std::memcpy(result.data(), &m_audio[s0], n_samples * sizeof(float));
    }
}

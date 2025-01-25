#include "VoiceDetector.h"

#include <QCoreApplication>
#include <QMediaDevices>
#include <QAudioFormat>
#include <QDebug>
#include <QThread>
#include <algorithm>
#include <cstring> // memcpy

//-------------------------
// Pullモード用のカスタム QIODevice 実装
//-------------------------
class VoicePullIODevice : public QIODevice
{
public:
    explicit VoicePullIODevice(VoiceDetector *detector, QObject *parent = nullptr)
        : QIODevice(parent)
        , m_detector(detector)
    {
    }

    bool open(OpenMode mode) override {
        // Pullモード時は writeOnly であればOK
        return QIODevice::open(mode);
    }

    // QIODevice interface
    qint64 readData(char * /*data*/, qint64 /*maxSize*/) override {
        // pullモードでは読み取りはしないので、常に0を返す
        return 0;
    }
    qint64 writeData(const char *data, qint64 len) override {
        // ここでオーディオデータを受け取る
        // もし VoiceDetector が停止中ならすぐ返す
        if (!m_detector->m_running.load()) {
            return len; // 受け取るだけ受け取って破棄
        }

        // データを float として解釈
        //   preferredFormat() が Int16 などの場合は自前で変換が必要 (省略)
        //   ここでは Float前提で進める例に
        size_t n_samples = static_cast<size_t>(len / sizeof(float));
        const float *samples = reinterpret_cast<const float*>(data);

        // 1) シグナル発行用のベクタを確保
        std::vector<float> chunkVec(n_samples);
        for (size_t i = 0; i < n_samples; i++) {
            chunkVec[i] = samples[i];
        }

        // 2) シグナル送信
        emit m_detector->audioAvailable(chunkVec);

        // 3) リングバッファに格納
        QMutexLocker locker(&m_detector->m_mutex);

        if (n_samples > m_detector->m_audio.size()) {
            // 入りきらない場合は最後の分だけ
            size_t discard = n_samples - m_detector->m_audio.size();
            samples += discard;
            n_samples = m_detector->m_audio.size();
        }

        size_t pos = m_detector->m_audio_pos;
        size_t bufSize = m_detector->m_audio.size();

        if (pos + n_samples > bufSize) {
            const size_t n0 = bufSize - pos;
            std::memcpy(&m_detector->m_audio[pos], samples, n0 * sizeof(float));
            std::memcpy(&m_detector->m_audio[0], samples + n0, (n_samples - n0) * sizeof(float));
            m_detector->m_audio_pos = (pos + n_samples) % bufSize;
            m_detector->m_audio_len = bufSize; // バッファ満杯
        } else {
            std::memcpy(&m_detector->m_audio[pos], samples, n_samples * sizeof(float));
            m_detector->m_audio_pos = (pos + n_samples) % bufSize;
            m_detector->m_audio_len = std::min(m_detector->m_audio_len + n_samples, bufSize);
        }

        return len;
    }

private:
    VoiceDetector *m_detector = nullptr;
};

//-------------------------
// VoiceDetector 実装
//-------------------------

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
    // pull用のIODeviceも削除
    if (m_pullDevice) {
        delete m_pullDevice;
        m_pullDevice = nullptr;
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
    const size_t bufferSize = static_cast<size_t>(m_sample_rate)
                              * m_len_ms / 1000
                              * channelCount;
    m_audio.resize(bufferSize, 0.0f);
    m_audio_pos = 0;
    m_audio_len = 0;

    // 1) デバイス取得
    QAudioDevice device = QMediaDevices::defaultAudioInput();
    if (!device.isNull()) {
        qDebug() << "[VoiceDetector] default audio input device:"
                 << device.description();
    }

    // 2) QAudioFormat: サンプルフォーマットに Float を優先
    QAudioFormat format;
    format.setSampleRate(m_sample_rate);
    format.setChannelCount(channelCount);
    format.setSampleFormat(QAudioFormat::Float);

    // 3) Float形式がサポートされるかチェック
    if (!device.isFormatSupported(format)) {
        // サポートされない → エラーを出力し、プログラム終了
        qCritical() << "[VoiceDetector] Error: Float format is NOT supported by the device!"
                    << "sampleRate=" << m_sample_rate
                    << "ch=" << channelCount;
        QCoreApplication::exit(1);
        return false; // (到達しないかもしれませんが念のため)
    }

    // 4) QAudioSourceの生成（pullモード前提）
    m_audioSource = new QAudioSource(device, format, this);
    if (!m_audioSource) {
        qWarning() << "[VoiceDetector] Failed to create QAudioSource";
        return false;
    }

    // 5) pullモード用の QIODevice を作成 (例: VoicePullIODevice)
    m_pullDevice = new VoicePullIODevice(this, this);
    if (!m_pullDevice->open(QIODevice::WriteOnly)) {
        qWarning() << "[VoiceDetector] Failed to open pullDevice";
        delete m_pullDevice;
        m_pullDevice = nullptr;
        delete m_audioSource;
        m_audioSource = nullptr;
        return false;
    }

    // 6) QAudioSource を start() し、pullDevice に書き込みさせる
    m_audioSource->start(m_pullDevice);

    // debug: stateChanged を監視
    connect(m_audioSource, &QAudioSource::stateChanged,
            this, [this](QAudio::State newState) {
                qDebug() << "[VoiceDetector] stateChanged ->" << newState
                         << "error=" << m_audioSource->error();
            });

    m_initialized = true;
    qDebug() << "[VoiceDetector] init done. bufferSize =" << bufferSize
             << "m_audioSource state =" << m_audioSource->state();
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

    qDebug() << "[VoiceDetector] resume capturing. state =" << m_audioSource->state();
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

    qDebug() << "[VoiceDetector] pause capturing. state =" << m_audioSource->state();
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

#include "VoiceDetector.h"

#include <QMediaDevices>
#include <QAudioFormat>
#include <QDebug>
#include <QThread>
#include <cstring> // memcpy

//-------------------------
// Pullモード用のカスタム QIODevice 実装
//-------------------------
class VoicePullIODevice : public QIODevice
{
public:
    explicit VoicePullIODevice(VoiceDetector *detector, QObject *parent = nullptr)
        : QIODevice(parent)
        , m_voice_detector(detector)
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
    qint64 writeData(const char *data, qint64 data_len_in_bytes) override {
        // もし VoiceDetector が停止中ならすぐ返す
        if (!m_voice_detector->m_running) {
            return data_len_in_bytes; // 受け取るだけ受け取って破棄
        }

        // AudioSource が実際に使用しているフォーマットを取得
        QAudioFormat actualFormat = m_voice_detector->m_audioSource->format();
        QAudioFormat::SampleFormat sf = actualFormat.sampleFormat();

        std::vector<float> chunkVec;  // ここに float 値として格納する

        switch (sf) {
        case QAudioFormat::Float: {
            // 既存のとおり float として解釈
            size_t sample_counts = static_cast<size_t>(data_len_in_bytes / sizeof(float));
            chunkVec.resize(sample_counts);

            const float *audio_samples = reinterpret_cast<const float*>(data);
            for (size_t i = 0; i < sample_counts; i++) {
                chunkVec[i] = audio_samples[i];
            }
            break;
        }
        case QAudioFormat::Int16: {
            // int16_t として解釈 → float に変換
            size_t sample_counts = static_cast<size_t>(data_len_in_bytes / sizeof(qint16));
            chunkVec.resize(sample_counts);

            const qint16 *audio_samples = reinterpret_cast<const qint16*>(data);
            for (size_t i = 0; i < sample_counts; i++) {
                // 32768.0f で割ることで [-1.0f, +1.0f] 程度の浮動小数に変換
                chunkVec[i] = static_cast<float>(audio_samples[i]) / 32768.0f;
            }
            break;
        }
        case QAudioFormat::Int32: {
            // int32_t → float
            size_t sample_counts = static_cast<size_t>(data_len_in_bytes / sizeof(qint32));
            chunkVec.resize(sample_counts);

            const qint32 *audio_samples = reinterpret_cast<const qint32*>(data);
            // float への正規化の仕方は int32 の場合スケールが 2^31 (約2.147e9)
            constexpr float kMaxInt32Inv = 1.0f / 2147483648.0f;
            for (size_t i = 0; i < sample_counts; i++) {
                chunkVec[i] = static_cast<float>(audio_samples[i]) * kMaxInt32Inv;
            }
            break;
        }
        // もし他のフォーマット (UInt8など) が来る場合は追加
        default:
            // このフォーマットには対応していない or 実装していないので無視
            qWarning() << "[VoicePullIODevice] Unsupported sampleFormat" << sf;
            return data_len_in_bytes;
        }

        // 変換後 chunkVec を送信
        emit m_voice_detector->audioAvailable(chunkVec);

        return data_len_in_bytes;
    }


private:
    VoiceDetector *m_voice_detector = nullptr;
};

//-------------------------
// VoiceDetector 実装
//-------------------------

VoiceDetector::VoiceDetector(int len_ms, QObject *parent)
    : QObject(parent)
    , m_len_ms(len_ms)
{
    m_running = false;
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

    // 1) デバイス取得
    QAudioDevice device = QMediaDevices::defaultAudioInput();
    if (!device.isNull()) {
        qDebug() << "[VoiceDetector] default audio input device:"
                 << device.description();
    }

    // 2) まずは「Float + ユーザー指定のsampleRate/channelCount」で試みる
    QAudioFormat formatWanted;
    formatWanted.setSampleRate(m_sample_rate);
    formatWanted.setChannelCount(channelCount);
    formatWanted.setSampleFormat(QAudioFormat::Float);

    // 3) サポートチェック
    if (!device.isFormatSupported(formatWanted)) {
        // Floatがサポートされない場合 → デバイスの preferredFormat にフォールバック
        qDebug() << "[VoiceDetector] Float format is NOT supported. Fallback to device's preferred format.";

        QAudioFormat preferred = device.preferredFormat();
        // 必要に応じてチャンネル数やサンプルレートを自分で上書きするか、
        // デバイス推奨のまま使うかは要検討 (ここでは sampleRate, channelCount を上書き例に)
        preferred.setSampleRate(m_sample_rate);
        preferred.setChannelCount(channelCount);

        // それでもサポートされない場合は仕方がないので device.preferredFormat() をまるごと受け入れる
        if (!device.isFormatSupported(preferred)) {
            qWarning() << "[VoiceDetector] Even the adjusted preferred format is not fully supported.";
            qWarning() << "[VoiceDetector] preferredFormat.sampleFormat=" << preferred.sampleFormat()
                       << " sampleRate=" << preferred.sampleRate()
                       << " channelCount=" << preferred.channelCount();
            qFatal("Cannot proceed.");
        }

        formatWanted = preferred;
    }

    // 4) QAudioSource生成
    m_audioSource = new QAudioSource(device, formatWanted, this);
    if (!m_audioSource) {
        qWarning() << "[VoiceDetector] Failed to create QAudioSource";
        return false;
    }

    // 5) pullモード用の QIODevice を作成
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
    qDebug() << "[VoiceDetector] init done. m_audioSource state =" << m_audioSource->state()
             << " format.sampleFormat=" << m_audioSource->format().sampleFormat()
             << " sampleRate=" << m_audioSource->format().sampleRate()
             << " channelCount=" << m_audioSource->format().channelCount();
    return true;
}



bool VoiceDetector::resume()
{
    if (!m_initialized) {
        qWarning() << "[VoiceDetector] not initialized";
        return false;
    }
    if (m_running) {
        qWarning() << "[VoiceDetector] already running";
        return false;
    }

    // QAudioSourceを再開
    m_audioSource->resume();
    m_running = true;

    qDebug() << "[VoiceDetector] resume capturing. state =" << m_audioSource->state();
    return true;
}

bool VoiceDetector::pause()
{
    if (!m_initialized) {
        qWarning() << "[VoiceDetector] not initialized";
        return false;
    }
    if (!m_running) {
        qWarning() << "[VoiceDetector] already paused";
        return false;
    }

    // 一時停止
    m_audioSource->suspend();
    m_running = false;

    qDebug() << "[VoiceDetector] pause capturing. state =" << m_audioSource->state();
    return true;
}

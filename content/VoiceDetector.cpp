#include "VoiceDetector.h"

#include <QCoreApplication>
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
        qDebug() << "writeData() thread =" << QThread::currentThread();
        // ここでオーディオデータを受け取る
        // もし VoiceDetector が停止中ならすぐ返す
        if (!m_voice_detector->m_running) {
            return data_len_in_bytes; // 受け取るだけ受け取って破棄
        }

        // データを float として解釈
        //   preferredFormat() が Int16 などの場合は自前で変換が必要 (省略)
        //   ここでは Float前提で進める例に
        size_t sample_counts = static_cast<size_t>(data_len_in_bytes / sizeof(float));
        const float *audio_samples = reinterpret_cast<const float*>(data);

        // 1) シグナル発行用のベクタを確保
        std::vector<float> chunkVec(sample_counts);
        for (size_t i = 0; i < sample_counts; i++) {
            chunkVec[i] = audio_samples[i];
        }

        // 2) シグナル送信
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
    qDebug() << "[VoiceDetector] init done. m_audioSource state =" << m_audioSource->state();
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

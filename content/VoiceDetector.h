#ifndef VOICEDETECTOR_H
#define VOICEDETECTOR_H

#include <QObject>
#include <QAudioSource>
#include <QByteArray>
#include <QAudioFormat>
#include <QIODevice>
#include <QMutex>
#include <atomic>
#include <vector>

/*
 * VoiceDetector:
 *   - QAudioSourceを使ってマイク入力を取得
 *   - リングバッファに一定長のサンプルを保持 (m_len_ms分)
 *   - 新規に読み込んだサンプルは audioAvailable(std::vector<float>) シグナルでも通知
 *
 * 使用例:
 *   VoiceDetector * detector = new VoiceDetector(10000); // 10秒分バッファ
 *   detector->init(16000, 1);  // サンプリングレート16kHz, mono
 *   detector->resume();        // 録音開始
 *   ...
 *   // シグナル: void audioAvailable(std::vector<float> chunk)
 *   //  これを接続して、音声エンジンに chunk を渡すなど
 */

class VoiceDetector : public QObject
{
    Q_OBJECT
public:
    explicit VoiceDetector(int len_ms, QObject *parent = nullptr);
    ~VoiceDetector();

    // 初期化： オーディオソースのデバイスを開き、フォーマットを設定
    bool init(int sampleRate, int channelCount = 1);

    bool resume();
    bool pause();
    bool clear();

    // リングバッファから一定時間(ms)のサンプルを取得して返す
    void get(int ms, std::vector<float> & result);

signals:
    // 新たに読み取ったサンプルを通知
    // chunk.size() はマイクから今回 read() できたサンプル数
    void audioAvailable(const std::vector<float> & chunk);

private slots:
    // QIODevice::readyReadがシグナルされたときに呼び出し
    void onDataAvailable();

private:
    // リングバッファ関連
    std::vector<float> m_audio;    // バッファ
    size_t             m_audio_pos = 0;
    size_t             m_audio_len = 0;

    QMutex m_mutex;               // 排他制御
    std::atomic_bool m_running;   // 実行中フラグ

    int  m_len_ms      = 0;       // リングバッファで保持したい長さ[ms]
    int  m_sample_rate = 0;

    // Qt Multimedia
    QAudioSource  *m_audioSource  = nullptr;
    QIODevice     *m_audioDevice  = nullptr; // audioSource->start() で得られるIOデバイス

    bool m_initialized = false;
};

#endif // VOICEDETECTOR_H

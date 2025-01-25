#ifndef VOICEDETECTOR_H
#define VOICEDETECTOR_H

#include <QObject>
#include <QAudioSource>
#include <QByteArray>
#include <QAudioFormat>
#include <QIODevice>
#include <atomic>
#include <vector>

/*
 * VoiceDetector (pull mode):
 *   - 独自の QIODevice を用いて pull モードでマイク入力を取得
 *   - リングバッファに一定長のサンプルを保持 (m_len_ms分)
 *   - 新規に読み取ったサンプルは audioAvailable(...) シグナルでも通知
 *
 * 使用例:
 *   VoiceDetector * detector = new VoiceDetector(10000); // 10秒分バッファ
 *   detector->init(16000, 1);  // サンプリングレート16kHz, mono
 *   detector->resume();        // 録音開始
 *   ...
 *   // シグナル: void audioAvailable(std::vector<float> chunk)
 *   //  これを接続して、音声エンジンに chunk を渡すなど
 */

// 前方宣言
class VoicePullIODevice;

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

signals:
    // pull デバイスから読み取ったサンプルを通知
    // chunk.size() は今回受け取ったサンプル数
    void audioAvailable(const std::vector<float> & chunk);

private:
    bool m_running;   // 実行中フラグ

    int  m_len_ms      = 0;       // リングバッファで保持したい長さ[ms]
    int  m_sample_rate = 0;

    // Qt Multimedia
    QAudioSource   *m_audioSource   = nullptr;
    VoicePullIODevice *m_pullDevice = nullptr; // pullモード用のカスタムQIODevice

    bool m_initialized = false;

    friend class VoicePullIODevice; // pullデバイスからリングバッファ操作できるように
};

#endif // VOICEDETECTOR_H

#ifndef VOICERECOGNITIONENGINE_H
#define VOICERECOGNITIONENGINE_H

#include <QObject>
#include <QTimer>
#include <QLocale>
#include <vector>
#include <string>
#include "OperationPhase.h"

struct whisper_context;

// 簡易パラメータ
struct VoiceRecParams {
    int   length_for_inference_ms  = 10000;  // 10秒
    float vad_thold  = 0.6f;
    float freq_thold = 100.0f;

    bool  use_gpu    = true;
    bool  flash_attn = false;
    std::string language = "en";
    std::string model    = WHISPER_MODEL_NAME;
};

class VoiceRecognitionEngine : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QLocale detectedVoiceLocale
                   READ detectedVoiceLocale
                       WRITE setDetectedVoiceLocale
                           NOTIFY detectedVoiceLocaleChanged
                               FINAL)

public:
    explicit VoiceRecognitionEngine(QObject *parent = nullptr);
    ~VoiceRecognitionEngine();

    // whisper初期化
    bool initWhisper(const VoiceRecParams &params);

    // 録音データを受け取る
    //  ここは外部(VoiceDetector等)から呼ばれる
    Q_INVOKABLE void addAudio(const std::vector<float> & pcmf32);

    // ループ開始/停止 (VAD判定など)
    Q_INVOKABLE void start();
    Q_INVOKABLE void stop();

    bool isRunning() const { return m_running; }

    void setLanguage(const QString & language);

    QLocale detectedVoiceLocale() const;
    void setDetectedVoiceLocale(const QLocale &newDetectedVoiceLocale);

signals:
    // 音声が認識されテキストが確定したらemit
    void textRecognized(const QString & text);

    void detectedVoiceLocaleChanged(const QLocale&);
    void changeOperationPhaseTo(OperationPhase newPhase);
private slots:
    void processVadCheck();

private:
    void runWhisper(const std::vector<float> & pcmf32);

    struct whisper_context * m_ctx = nullptr;
    VoiceRecParams  m_whisper_params;

    // 音声保存用
    std::vector<float> m_capturedAudio;

    QTimer * m_timer = nullptr;
    bool     m_running = false;

    QLocale m_detectedVoiceLocale;
};

#endif // VOICERECOGNITIONENGINE_H

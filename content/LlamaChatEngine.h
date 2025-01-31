#ifndef LLAMACHATENGINE_H
#define LLAMACHATENGINE_H

#include <optional>
#include <QQmlEngine>
#include <QRemoteObjectNode>
#include <QThread>
#include <QMetaObject>
#include <QObject>
#include "ChatMessageModel.h"
#include "LlamaResponseGenerator.h"
#include "rep_LlamaResponseGenerator_replica.h"
#include "RemoteResponseGeneratorCompositor.h"
#include "VoiceDetector.h"
#include "VoiceRecognitionEngine.h"
#include "OperationPhase.h"
#include "llama.h"

/*
  LlamaChatEngine:
    - Provides a local or remote inference interface
    - Manages chat messages and engine initialization
    - Exposes QML properties and methods

  LlamaChatEngineクラス:
    - ローカルまたはリモート推論のインターフェースを提供
    - チャットメッセージとエンジン初期化を管理
    - QML向けにプロパティやメソッドを公開
*/
class LlamaChatEngine : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    //--------------------------------------------------------------------------
    // QML Properties (QMLプロパティ)
    //--------------------------------------------------------------------------
    Q_PROPERTY(ChatMessageModel* messages READ messages CONSTANT)
    Q_PROPERTY(QString userInput READ userInput WRITE setUserInput RESET resetUserInput NOTIFY userInputChanged FINAL)
    Q_PROPERTY(EngineMode currentEngineMode READ currentEngineMode NOTIFY currentEngineModeChanged FINAL)
    Q_PROPERTY(QString ipAddress READ ipAddress WRITE setIpAddress NOTIFY ipAddressChanged FINAL)
    Q_PROPERTY(int portNumber READ portNumber WRITE setPortNumber NOTIFY portNumberChanged FINAL)
    Q_PROPERTY(bool localInitialized READ localInitialized WRITE setLocalInitialized NOTIFY localInitializedChanged FINAL)
    Q_PROPERTY(bool remoteInitialized READ remoteInitialized WRITE setRemoteInitialized NOTIFY remoteInitializedChanged FINAL)
    Q_PROPERTY(bool remoteAiInError READ remoteAiInError NOTIFY remoteAiInErrorChanged FINAL)
    Q_PROPERTY(bool localAiInError READ localAiInError NOTIFY localAiInErrorChanged FINAL)
    Q_PROPERTY(bool inProgress READ inProgress NOTIFY inProgressChanged FINAL)
    Q_PROPERTY(double modelDownloadProgress READ modelDownloadProgress NOTIFY modelDownloadProgressChanged FINAL)
    Q_PROPERTY(bool modelDownloadInProgress READ modelDownloadInProgress NOTIFY modelDownloadInProgressChanged FINAL)
    Q_PROPERTY(QLocale detectedVoiceLocale READ detectedVoiceLocale NOTIFY detectedVoiceLocaleChanged FINAL)
    Q_PROPERTY(OperationPhase operationPhase READ operationPhase WRITE setOperationPhase NOTIFY operationPhaseChanged FINAL)
    Q_PROPERTY(double whisperModelDownloadProgress READ whisperModelDownloadProgress NOTIFY whisperModelDownloadProgressChanged FINAL)
    Q_PROPERTY(bool whisperModelDownloadInProgress READ whisperModelDownloadInProgress NOTIFY whisperModelDownloadInProgressChanged FINAL)

public:
    //--------------------------------------------------------------------------
    // EngineMode Enum (エンジンモード列挙: ローカル/リモート/未初期化)
    //--------------------------------------------------------------------------
    enum EngineMode {
        Mode_Local,
        Mode_Remote,
        Mode_Uninitialized
    };
    Q_ENUM(EngineMode)

    //--------------------------------------------------------------------------
    // OperationPhase Enum (オペレーションフェーズ列挙: 現在実行中の処理フェーズを示す)
    //--------------------------------------------------------------------------
    Q_ENUM(OperationPhase)

    //--------------------------------------------------------------------------
    // Constructor / Destructor
    // コンストラクタ / デストラクタ
    //--------------------------------------------------------------------------
    explicit LlamaChatEngine(QObject* parent = nullptr);
    ~LlamaChatEngine() override;

    //--------------------------------------------------------------------------
    // QML-Invokable Methods (QMLから呼び出せるpublicメソッド)
    //--------------------------------------------------------------------------
    Q_INVOKABLE void switchEngineMode(EngineMode mode);
    Q_INVOKABLE void pauseVoiceDetection();
    Q_INVOKABLE void resumeVoiceDetection();
    Q_INVOKABLE void setVoiceRecognitionLanguage(const QString &language);
    Q_INVOKABLE void initiateVoiceRecognition();
    Q_INVOKABLE void stopVoiceRecognition();

    //--------------------------------------------------------------------------
    // QML-Exposed Getters / Setters (QMLに公開されるゲッター/セッター)
    //--------------------------------------------------------------------------
    ChatMessageModel* messages();

    QString userInput() const;
    Q_INVOKABLE void setUserInput(const QString &newUserInput);
    void resetUserInput();

    EngineMode currentEngineMode() const;

    QString ipAddress() const;
    void setIpAddress(const QString &newIpAddress);

    int portNumber() const;
    void setPortNumber(int newPortNumber);

    bool localInitialized() const;
    void setLocalInitialized(bool newLocalInitialized);

    bool remoteInitialized() const;
    void setRemoteInitialized(bool newRemoteInitialized);

    bool remoteAiInError() const;
    void setRemoteAiInError(bool newRemoteAiInError);

    bool localAiInError() const;
    void setLocalAiInError(bool newLocalAiInError);

    bool inProgress() const;
    void setInProgress(bool newInProgress);

    double modelDownloadProgress() const;
    void setModelDownloadProgress(double newModelDownloadProgress);

    bool modelDownloadInProgress() const;
    void setModelDownloadInProgress(bool newModelDownloadInProgress);

    QLocale detectedVoiceLocale() const;

    void setDetectedVoiceLocale(const QLocale &newDetectedVoiceLocale);

    OperationPhase operationPhase() const;
    void setOperationPhase(OperationPhase newOperationPhase);

    double whisperModelDownloadProgress() const;
    void setWhisperModelDownloadProgress(double newWhisperModelDownloadProgress);

    bool whisperModelDownloadInProgress() const;
    void setWhisperModelDownloadInProgress(bool newWhisperModelDownloadInProgress);

signals:
    //--------------------------------------------------------------------------
    // Signals (シグナル)
    //--------------------------------------------------------------------------
    void userInputChanged();
    void currentEngineModeChanged();
    void ipAddressChanged();
    void portNumberChanged();
    void localInitializedChanged();
    void remoteInitializedChanged();
    void remoteAiInErrorChanged();
    void localAiInErrorChanged();
    void inProgressChanged();
    void modelDownloadProgressChanged();
    void modelDownloadInProgressChanged();
    void detectedVoiceLocaleChanged();
    void operationPhaseChanged();
    void requestGeneration(const QList<LlamaChatMessage>& messages);
    void generationFinishedToQML(const QString& finalResponse);
    void inferenceErrorToQML(const QString &errorMessage);
    void modelDownloadFinished(bool success);
    void whisperModelDownloadFinished(bool success);
    void whisperModelDownloadProgressChanged();
    void whisperModelDownloadInProgressChanged();

private slots:
    //--------------------------------------------------------------------------
    // Internal Slots (内部で呼ばれるSlots)
    //--------------------------------------------------------------------------
    void onEngineInitFinished();
    void initAfterDownload(bool success);
    void reinitLocalEngine();
    void handleRecognizedText(const QString &text);
    void handleNewUserInput();
    void onPartialResponse(const QString &textSoFar);
    void onGenerationFinished(const QString &finalResponse);
    void onInferenceError(const QString &errorMessage);

private:
    //--------------------------------------------------------------------------
    // Private Helper Methods (プライベートヘルパーメソッド)
    //--------------------------------------------------------------------------
    void doEngineInit();
    void doImmediateEngineSwitch(EngineMode newMode);

    void configureRemoteSignalSlots();
    void configureLocalSignalSlots();
    void configureRemoteObjects();
    void updateRemoteInitializationStatus();
    bool initializeModelPathForAndroid();
    void downloadModelIfNeededAsync();
    void setCurrentEngineMode(EngineMode newCurrentEngineMode);

    void initVoiceRecognition();
    void startVoiceRecognition();

    //--------------------------------------------------------------------------
    // Constants (定数)
    //--------------------------------------------------------------------------
    static constexpr int mNGl  {99};
    static constexpr int mNCtx {2048};

    // Default LLaMA model path (defined via CMake)
    // CMakeで定義されたLLaMAモデルパス
    static const std::string mModelPath;

    // modelをランタイムでダウンロードする際の進捗
    double mModelDownloadProgress {0.0};
    bool   mModelDownloadInProgress {false};
    double mWhisperModelDownloadProgress {0.0};
    bool   mWhisperModelDownloadInProgress {false};

    std::string mWhisperModelPath;
    bool mWhisperModelReady { false };
    void downloadWhisperModelIfNeededAsync();
    void onWhisperDownloadFinished(bool success);

    //--------------------------------------------------------------------------
    // LLaMA Model / Context (LLaMAモデル/コンテキスト)
    //--------------------------------------------------------------------------
    llama_model_params mModelParams;
    llama_model*       mModel          {nullptr};
    llama_context_params mCtxParams;
    llama_context*       mCtx          {nullptr};

    //--------------------------------------------------------------------------
    // Engines: local or remote (ローカル/リモートエンジン)
    //--------------------------------------------------------------------------
    LlamaResponseGenerator*        mLocalGenerator  {nullptr};
    RemoteResponseGeneratorCompositor mRemoteGenerator;

    //--------------------------------------------------------------------------
    // Connection Info (接続情報 IP/ポート)
    //--------------------------------------------------------------------------
    QString mIpAddress;
    int     mPortNumber {0};

    //--------------------------------------------------------------------------
    // Engine States (エンジン状態管理)
    //--------------------------------------------------------------------------
    std::optional<EngineMode> mPendingEngineSwitchMode;
    EngineMode                mCurrentEngineMode {Mode_Uninitialized};
    QThread*                  mLocalWorkerThread {nullptr}; // Local inference thread
    bool                      mInProgress        {false};   // Generation / Inference flag
    int                       mCurrentAssistantIndex {-1};  // Index of current assistant in the chat model

    //--------------------------------------------------------------------------
    // Chat Data (チャットデータ関連)
    //--------------------------------------------------------------------------
    QString          mUserInput;
    ChatMessageModel mMessages;

    //--------------------------------------------------------------------------
    // Initialization status (初期化状態)
    //--------------------------------------------------------------------------
    bool mLocalInitialized  {false};
    bool mRemoteInitialized {false};

    //--------------------------------------------------------------------------
    // Error flags (エラーフラグ)
    //--------------------------------------------------------------------------
    bool mRemoteAiInError {false};
    bool mLocalAiInError  {false};

    //--------------------------------------------------------------------------
    // Connection Storage (接続情報を保持)
    //--------------------------------------------------------------------------
    // (common)
    std::optional<QMetaObject::Connection> mHandleNewUserInputConnection;

    // remote
    std::optional<QMetaObject::Connection> mRemoteInitializedConnection;
    std::optional<QMetaObject::Connection> mRemoteRequestGenerationConnection;
    std::optional<QMetaObject::Connection> mRemotePartialResponseConnection;
    std::optional<QMetaObject::Connection> mRemoteGenerationFinishedConnection;
    std::optional<QMetaObject::Connection> mRemoteGenerationFinishedToQMLConnection;
    std::optional<QMetaObject::Connection> mRemoteGenerationErrorConnection;
    std::optional<QMetaObject::Connection> mRemoteGenerationErrorToQmlConnection;

    // local
    std::optional<QMetaObject::Connection> mLocalRequestGenerationConnection;
    std::optional<QMetaObject::Connection> mLocalPartialResponseConnection;
    std::optional<QMetaObject::Connection> mLocalGenerationFinishedConnection;
    std::optional<QMetaObject::Connection> mLocalGenerationFinishedToQMLConnection;
    std::optional<QMetaObject::Connection> mLocalGenerationErrorConnection;
    std::optional<QMetaObject::Connection> mLocalGenerationErrorToQmlConnection;

    VoiceRecognitionEngine* m_voiceRecognitionEngine = nullptr;
    VoiceDetector*          m_voiceDetector = nullptr;
    QLocale                 m_detectedVoiceLocale;

    OperationPhase          m_operationPhase = WaitingUserInput;

    // Additional helper connection setup/teardown
    void setupRemoteConnections();
    void teardownRemoteConnections();
    void setupLocalConnections();
    void teardownLocalConnections();
    void setupCommonConnections();
    void teardownCommonConnections();
};

#endif // LLAMACHATENGINE_H

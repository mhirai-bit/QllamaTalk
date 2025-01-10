#ifndef LLAMACHATENGINE_H
#define LLAMACHATENGINE_H

#include <optional>
#include <QObject>
#include <QQmlEngine>
#include <QRemoteObjectNode>
#include <QThread>
#include <QMetaObject>
#include "chatmessagemodel.h"
#include "llamaresponsegenerator.h"
#include "rep_LlamaResponseGenerator_replica.h"
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
    // QML Properties
    // QMLプロパティ
    //--------------------------------------------------------------------------
    Q_PROPERTY(ChatMessageModel* messages
                   READ messages
                       CONSTANT)
    Q_PROPERTY(QString userInput
                   READ userInput
                       WRITE setUserInput
                           RESET resetUserInput
                               NOTIFY userInputChanged
                                   FINAL)
    Q_PROPERTY(EngineMode currentEngineMode
                   READ currentEngineMode
                       NOTIFY currentEngineModeChanged
                           FINAL)
    Q_PROPERTY(QString ipAddress
                   READ ipAddress
                       WRITE setIpAddress
                           NOTIFY ipAddressChanged
                               FINAL)
    Q_PROPERTY(int portNumber
                   READ portNumber
                       WRITE setPortNumber
                           NOTIFY portNumberChanged
                               FINAL)
    Q_PROPERTY(bool localInitialized
                   READ localInitialized
                       WRITE setLocalInitialized
                           NOTIFY localInitializedChanged
                               FINAL)
    Q_PROPERTY(bool remoteInitialized
                   READ remoteInitialized
                       WRITE setRemoteInitialized
                           NOTIFY remoteInitializedChanged
                               FINAL)
    Q_PROPERTY(bool remoteAiInError
                   READ remoteAiInError
                       NOTIFY remoteAiInErrorChanged
                           FINAL)
    Q_PROPERTY(bool localAiInError
                   READ localAiInError
                       NOTIFY localAiInErrorChanged
                           FINAL)
    Q_PROPERTY(bool inProgress
                   READ inProgress
                       NOTIFY inProgressChanged
                           FINAL)
    Q_PROPERTY(double modelDownloadProgress READ modelDownloadProgress NOTIFY modelDownloadProgressChanged FINAL)
    Q_PROPERTY(bool modelDownloadInProgress READ modelDownloadInProgress NOTIFY modelDownloadInProgressChanged FINAL)

public:
    //--------------------------------------------------------------------------
    // EngineMode Enum
    // エンジンモード列挙 (ローカル / リモート / 未初期化)
    //--------------------------------------------------------------------------
    enum EngineMode {
        Mode_Local,
        Mode_Remote,
        Mode_Uninitialized
    };
    Q_ENUM(EngineMode)

    //--------------------------------------------------------------------------
    // Constructor / Destructor
    // コンストラクタ / デストラクタ
    //--------------------------------------------------------------------------
    explicit LlamaChatEngine(QObject* parent = nullptr);
    ~LlamaChatEngine() override;

    //--------------------------------------------------------------------------
    // Public QML-Invokable Methods
    // QMLから呼び出せるpublicメソッド
    //--------------------------------------------------------------------------
    Q_INVOKABLE void switchEngineMode(EngineMode mode);

    //--------------------------------------------------------------------------
    // QML-Exposed Getters / Setters
    // QMLに公開されるゲッター/セッター
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

public slots:
    //--------------------------------------------------------------------------
    // Public Slots
    // 外部/QMLなどから呼ばれる可能性のあるSlots
    //--------------------------------------------------------------------------
    void handle_new_user_input();

signals:
    //--------------------------------------------------------------------------
    // Signals
    // シグナル
    //--------------------------------------------------------------------------
    void userInputChanged();
    void requestGeneration(const QList<LlamaChatMessage>& messages);
    void currentEngineModeChanged();
    void ipAddressChanged();
    void portNumberChanged();
    void localInitializedChanged();
    void remoteInitializedChanged();
    void inferenceErrorToQML(const QString &errorMessage);
    void remoteAiInErrorChanged();
    void localAiInErrorChanged();
    void inProgressChanged();
    void modelDownloadFinished(bool success);
    void modelDownloadProgressChanged();

    void modelDownloadInProgressChanged();

private slots:
    //--------------------------------------------------------------------------
    // Internal Slots
    // 内部で呼ばれるSlots
    //--------------------------------------------------------------------------
    void onPartialResponse(const QString &textSoFar);
    void onGenerationFinished(const QString &finalResponse);
    void onEngineInitFinished();
    void onInferenceError(const QString &errorMessage);
    void reinitLocalEngine();
    void initAfterDownload(bool success);

private:
    //--------------------------------------------------------------------------
    // Private Helper Methods
    // プライベートヘルパーメソッド
    //--------------------------------------------------------------------------
    void doEngineInit();
    void doImmediateEngineSwitch(EngineMode newMode);

    void configureRemoteSignalSlots();
    void configureLocalSignalSlots();
    void configureRemoteObjects();
    void updateRemoteInitializationStatus();
    bool initializeModelPathForAndroid();
    void downloadModelIfNeededAsync();

    // Not called from QML
    // QMLからは呼ばない前提
    void setCurrentEngineMode(EngineMode newCurrentEngineMode);

    //--------------------------------------------------------------------------
    // Constants
    // 定数
    //--------------------------------------------------------------------------
    static constexpr int mNGl  {99};
    static constexpr int mNCtx {2048};

    // Default LLaMA model path (defined via CMake)
    // CMakeで定義されたLLaMAモデルパス
    static const std::string mModelPath;

    // modelをランタイムでダウンロードする際の進捗
    double mModelDownloadProgress {0.0};
    bool mModelDownloadInProgress {false};

    //--------------------------------------------------------------------------
    // LLaMA Model / Context
    // LLaMAモデル/コンテキスト
    //--------------------------------------------------------------------------
    llama_model_params mModelParams;
    llama_model*       mModel    {nullptr};
    llama_context_params mCtxParams;
    llama_context*       mCtx    {nullptr};

    //--------------------------------------------------------------------------
    // Engines: local or remote
    // ローカル/リモートエンジン
    //--------------------------------------------------------------------------
    LlamaResponseGenerator*        mLocalGenerator  {nullptr};
    LlamaResponseGeneratorReplica* mRemoteGenerator {nullptr};
    QRemoteObjectNode*             mRemoteNode      {nullptr};

    //--------------------------------------------------------------------------
    // Connection Info
    // 接続情報 (IP/ポート)
    //--------------------------------------------------------------------------
    QString mIpAddress;
    int     mPortNumber {0};

    //--------------------------------------------------------------------------
    // Engine States
    // エンジン状態管理
    //--------------------------------------------------------------------------
    std::optional<EngineMode> mPendingEngineSwitchMode;
    EngineMode                mCurrentEngineMode {Mode_Uninitialized};

    // Local inference thread
    // ローカル推論用スレッド
    QThread* mLocalWorkerThread {nullptr};

    // Generation / Inference flags
    // 推論中かどうかなどのフラグ
    bool mInProgress            {false};
    int  mCurrentAssistantIndex {-1};

    // Chat Data
    // チャットデータ関連
    QString          mUserInput;
    ChatMessageModel mMessages;

    // Initialization status
    // 初期化状態
    bool mLocalInitialized  {false};
    bool mRemoteInitialized {false};

    // Error flags
    // エラーフラグ
    bool mRemoteAiInError {false};
    bool mLocalAiInError  {false};

    //--------------------------------------------------------------------------
    // Connection Storage
    // 接続情報を保持
    //--------------------------------------------------------------------------
    // (common)
    std::optional<QMetaObject::Connection> mHandleNewUserInputConnection;

    // remote
    std::optional<QMetaObject::Connection> mRemoteInitializedConnection;
    std::optional<QMetaObject::Connection> mRemoteRequestGenerationConnection;
    std::optional<QMetaObject::Connection> mRemotePartialResponseConnection;
    std::optional<QMetaObject::Connection> mRemoteGenerationFinishedConnection;
    std::optional<QMetaObject::Connection> mRemoteGenerationErrorConnection;
    std::optional<QMetaObject::Connection> mRemoteGenerationErrorToQmlConnection;

    // local
    std::optional<QMetaObject::Connection> mLocalRequestGenerationConnection;
    std::optional<QMetaObject::Connection> mLocalPartialResponseConnection;
    std::optional<QMetaObject::Connection> mLocalGenerationFinishedConnection;
    std::optional<QMetaObject::Connection> mLocalGenerationErrorConnection;
    std::optional<QMetaObject::Connection> mLocalGenerationErrorToQmlConnection;

    // Additional helper connection setup/teardown
    void setupRemoteConnections();
    void teardownRemoteConnections();
    void setupLocalConnections();
    void teardownLocalConnections();
    void setupCommonConnections();
    void teardownCommonConnections();
};

#endif // LLAMACHATENGINE_H

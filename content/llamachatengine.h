#ifndef LLAMACHATENGINE_H
#define LLAMACHATENGINE_H

#include <optional>
#include <QObject>
#include <QQmlEngine>
#include <QRemoteObjectNode>
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
class LlamaChatEngine : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    // QML properties (English/Japanese)
    // QMLプロパティ
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

public:
    // EngineMode: local / remote / uninitialized
    // エンジンモード: ローカル / リモート / 未初期化
    enum EngineMode {
        Mode_Local,
        Mode_Remote,
        Mode_Uninitialized
    };
    Q_ENUM(EngineMode)

    // Constructor / Destructor
    // コンストラクタ / デストラクタ
    explicit LlamaChatEngine(QObject *parent = nullptr);
    ~LlamaChatEngine() override;

    // Switch engine mode (local / remote)
    // エンジンモードを切り替え（ローカル / リモート）
    Q_INVOKABLE void switchEngineMode(EngineMode mode);

    // QML-exposed getters/setters
    // QMLに公開するゲッター/セッター
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

public slots:
    // Process user input (apply template, request generation)
    // ユーザー入力を処理 (テンプレート適用, 推論リクエスト)
    void handle_new_user_input();

signals:
    // Notifies userInput changes
    // userInputが変わったら通知
    void userInputChanged();

    // Called to request generation with prepared messages
    // 準備されたメッセージで推論を行うようリクエスト
    void requestGeneration(const QList<LlamaChatMessage>& messages);

    // Informs that the engine mode changed
    // エンジンモードが変わったことを通知
    void currentEngineModeChanged();

    // Informs IP/port changes
    // IPアドレス/ポート番号が変わったことを通知
    void ipAddressChanged();
    void portNumberChanged();

    // Informs local/remote init status changes
    // ローカル/リモート初期化状態が変わったことを通知
    void localInitializedChanged();
    void remoteInitializedChanged();

    void inferenceErrorToQML(const QString& errorMessage);

    void remoteAiInErrorChanged();

    void localAiInErrorChanged();

    void inProgressChanged();

private slots:
    // Receives partial AI response
    // 部分的なAI応答を受け取る
    void onPartialResponse(const QString &textSoFar);

    // Receives final AI response
    // 最終的なAI応答を受け取る
    void onGenerationFinished(const QString &finalResponse);

    // Called after engine init is done
    // エンジン初期化完了後に呼ばれる
    void onEngineInitFinished();

    void onInferenceError(const QString& errorMessage);

    void reinitLocalEngine();

private:
    // Runs heavy init in another thread
    // 重い初期化を別スレッドで実行
    void doEngineInit();

    // Immediately switch between local/remote
    // ローカル/リモートを即時切り替え
    void doImmediateEngineSwitch(EngineMode newMode);

    // Internal configuration helpers
    // 内部的な設定用ヘルパー
    void configureRemoteSignalSlots();
    void configureLocalSignalSlots();
    void configureRemoteObjects();
    void updateRemoteInitializationStatus();

    // Not meant to be called by QML
    // QMLからは呼ばれない想定
    void setCurrentEngineMode(EngineMode newCurrentEngineMode);

    // Constants (English/Japanese)
    // 定数
    static constexpr int mNGl {99};
    static constexpr int mNCtx {2048};

    // Default LLaMA model path from CMake
    // CMakeで指定したデフォルトLLaMAモデルパス
    static const std::string mModelPath;

    // LLaMA model/context parameters
    // LLaMAモデル/コンテキストのパラメータ
    llama_model_params mModelParams;
    llama_model* mModel {nullptr};
    llama_context_params mCtxParams;
    llama_context* mCtx {nullptr};

    // Engines: local / remote
    // エンジン(ローカル/リモート)
    LlamaResponseGenerator* mLocalGenerator {nullptr};
    LlamaResponseGeneratorReplica* mRemoteGenerator {nullptr};

    // Remote connection
    // リモート接続設定
    QString mIpAddress;
    int mPortNumber;
    QRemoteObjectNode* mRemoteNode {nullptr};

    // Switch request mid-inference
    // 推論中に切り替え要求がきた場合の保管
    std::optional<EngineMode> mPendingEngineSwitchMode;
    EngineMode mCurrentEngineMode {Mode_Uninitialized};

    // Worker thread for local inference
    // ローカル推論用ワーカースレッド
    QThread* mLocalWorkerThread {nullptr};

    // Generation/inference flags
    // 推論状態フラグ
    bool mInProgress {false};
    int mCurrentAssistantIndex {-1};

    // Chat data
    // チャットデータ
    QString mUserInput;
    ChatMessageModel mMessages;

    // Engine init status
    // エンジン初期化ステータス
    bool mLocalInitialized {false};
    bool mRemoteInitialized {false};

    bool mRemoteAiInError {false};
    bool mLocalAiInError {false};

    void setupRemoteConnections();
    void teardownRemoteConnections();
    void setupLocalConnections();
    void teardownLocalConnections();
    void setupCommonConnections();
    void teardownCommonConnections();

    // common
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
};

#endif // LLAMACHATENGINE_H

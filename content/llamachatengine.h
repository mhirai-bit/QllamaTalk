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
  - Provides an interface for local or remote inference
  - Manages chat messages and engine initialization
  - Exposes QML-friendly properties and methods

  チャットロジックを管理し、ローカル/リモート推論を切り替えるクラス
  QMLから使いやすいプロパティやメソッドを提供
*/
class LlamaChatEngine : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    // QML properties: messages, user_input, engine_initialized
    // QMLプロパティ: messages, user_input, engine_initialized
    Q_PROPERTY(ChatMessageModel* messages READ messages CONSTANT)
    Q_PROPERTY(QString user_input READ user_input WRITE setUser_input RESET resetUser_input NOTIFY user_inputChanged FINAL)
    Q_PROPERTY(EngineMode currentEngineMode READ currentEngineMode WRITE setCurrentEngineMode NOTIFY currentEngineModeChanged FINAL)
    Q_PROPERTY(QString ip_address READ ip_address WRITE setIp_address NOTIFY ip_addressChanged FINAL)
    Q_PROPERTY(int port_number READ port_number WRITE setPort_number NOTIFY port_numberChanged FINAL)
    Q_PROPERTY(bool local_initialized READ local_initialized WRITE setLocal_initialized NOTIFY local_initializedChanged FINAL)
    Q_PROPERTY(bool remote_initialzied READ remote_initialzied WRITE setRemote_initialzied NOTIFY remote_initialziedChanged FINAL)

public:
    // Distinguish local vs remote inference
    // ローカル推論とリモート推論を区別する列挙値
    enum EngineMode {
        Mode_Local,
        Mode_Remote,
        Mode_Uninitialized
    };
    Q_ENUM(EngineMode)

    explicit LlamaChatEngine(QObject *parent = nullptr);
    ~LlamaChatEngine() override;

    // Switch engine mode between local or remote
    // ローカル/リモートエンジンの切り替え
    Q_INVOKABLE void switchEngineMode(EngineMode mode);

    // QML-facing getters/setters
    // QML側に公開するゲッター/セッター
    ChatMessageModel* messages();
    QString user_input() const;
    Q_INVOKABLE void setUser_input(const QString &newUser_input);
    void resetUser_input();

    EngineMode currentEngineMode() const;
    void setCurrentEngineMode(EngineMode newCurrentEngineMode);

    QString ip_address() const;
    void setIp_address(const QString &newIp_address);

    int port_number() const;
    void setPort_number(int newPort_number);

    bool local_initialized() const;
    void setLocal_initialized(bool newLocal_initialized);

    bool remote_initialzied() const;
    void setRemote_initialzied(bool newRemote_initialzied);

public slots:
    // Handle new user input
    // 新しいユーザー入力を処理
    void handle_new_user_input();

signals:
    // Emitted when user_input changes
    // user_inputが変化した時に送出
    void user_inputChanged();
    // Emitted with newly formed prompt to trigger inference
    // 生成したプロンプトを通知して推論を呼び出す
    void requestGeneration(const QList<LlamaChatMessage>& messages);

    void currentEngineModeChanged();

    void ip_addressChanged();

    void port_numberChanged();

    void local_initializedChanged();

    void remote_initialziedChanged();

private slots:
    // Receive partial AI response
    // 部分的なAI応答を受け取る
    void onPartialResponse(const QString &textSoFar);

    // Receive final AI response
    // 最終的なAI応答を受け取る
    void onGenerationFinished(const QString &finalResponse);

    // Called after engine initialization
    // エンジン初期化完了後に呼ばれる
    void onEngineInitFinished();

private:
    // Heavy initialization in separate thread
    // 別スレッドで重い初期化を行う
    void doEngineInit();

    // Immediately switch local/remote engines
    // ローカル/リモートエンジンを即座に切り替える
    void doImmediateEngineSwitch(EngineMode newMode);

    static constexpr int m_ngl {99};
    static constexpr int m_n_ctx {2048};

    // Path to default LLaMA model (defined by CMake)
    // CMakeで定義されたデフォルトのLLaMAモデルパス
    static const std::string m_model_path;

    llama_model_params m_model_params;
    llama_model* m_model {nullptr};
    llama_context_params m_ctx_params;
    llama_context* m_ctx {nullptr};

    // Local and remote engines
    // ローカルエンジンとリモートエンジン
    LlamaResponseGenerator* m_localGenerator {nullptr};
    LlamaResponseGeneratorReplica* m_remoteGenerator {nullptr};

    // IP and port for remote inference
    // リモート推論用のIPとポート
    QString m_ip_address;
    int m_port_number;

    QRemoteObjectNode *m_remoteMode {nullptr};

    // If a switch request occurs mid-inference, store it here
    // 推論中にモード切り替え要求が来た場合、ここに保管
    std::optional<EngineMode> m_pendingEngineSwitchMode;
    EngineMode m_currentEngineMode {Mode_Uninitialized};

    // Worker thread for local inference
    // ローカル推論用ワーカースレッド
    QThread* m_localWorkerThread {nullptr};

    // Generation/inference state
    // 推論状態を示すフラグなど
    bool m_inProgress {false};
    int m_currentAssistantIndex {-1};

    // Chat data
    // チャットデータ関連
    QString m_user_input;
    ChatMessageModel m_messages;

    // Engine init status
    // エンジンの初期化状態
    bool m_local_initialized {false};
    bool m_remote_initialzied {false};
    void configureRemoteSignalSlots();
    void configureLocalSignalSlots();
    void configureRemoteObjects();
    void updateRemoteInitializationStatus();
};

#endif // LLAMACHATENGINE_H

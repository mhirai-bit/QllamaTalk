#ifndef LLAMACHATENGINE_H
#define LLAMACHATENGINE_H

#include <optional>
#include <QObject>
#include <QQmlEngine>
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

    // QML properties (messages, user_input, engine_initialized)
    // QMLで利用するプロパティ（messages、user_input、engine_initialized）
    Q_PROPERTY(ChatMessageModel* messages READ messages CONSTANT)
    Q_PROPERTY(QString user_input READ user_input WRITE setUser_input RESET resetUser_input NOTIFY user_inputChanged FINAL)
    Q_PROPERTY(bool engine_initialized READ engine_initialized NOTIFY engine_initializedChanged FINAL)

public:
    // Distinguish local vs remote inference
    // ローカル推論とリモート推論を区別するための列挙
    enum EngineMode {
        Mode_Local,
        Mode_Remote
    };
    Q_ENUM(EngineMode)

    explicit LlamaChatEngine(QObject *parent = nullptr);
    ~LlamaChatEngine() override;

    // Switch inference between local or remote engine
    // ローカル/リモートエンジンを切り替える
    Q_INVOKABLE void switchEngineMode(EngineMode mode);

    // Accessors for QML
    // QML側からのアクセスに使うゲッターやセッター
    ChatMessageModel* messages();
    QString user_input() const;
    Q_INVOKABLE void setUser_input(const QString &newUser_input);
    void resetUser_input();
    bool engine_initialized() const;

public slots:
    // Process new user input
    // 新しいユーザー入力を処理
    void handle_new_user_input();

signals:
    // Emitted when user_input changes
    // user_input変更時に送出
    void user_inputChanged();
    // Emitted with generated prompt, triggers inference
    // プロンプト生成を通知し、推論を呼び出す
    void requestGeneration(const QString &prompt);
    // Notifies when engine initialization status changes
    // エンジン初期化状態の変化を通知
    void engine_initializedChanged();

private slots:
    // Receives partial AI response
    // 部分的なAI応答を受け取る
    void onPartialResponse(const QString &textSoFar);
    // Receives final AI response
    // 最終的なAI応答を受け取る
    void onGenerationFinished(const QString &finalResponse);
    // Called after engine init is complete
    // エンジンの初期化が完了したあとに呼ばれる
    void onEngineInitFinished();

private:
    // Run heavy init process in a separate thread
    // 重い初期化処理を別スレッドで実行
    void doEngineInit();
    // Sets engine_initialized flag
    // engine_initializedフラグを設定
    void setEngine_initialized(bool newEngine_initialized);
    // Immediately switch engines
    // エンジンを即切り替える処理
    void doImmediateEngineSwitch(EngineMode newMode);

    static constexpr int m_ngl {99};
    static constexpr int m_n_ctx {2048};
    // Defined in .cpp, holds the default LLaMA model path
    // デフォルトのLLaMAモデルパス（.cppで定義）
    static const std::string m_model_path;

    llama_model_params m_model_params;
    llama_model* m_model {nullptr};
    llama_context_params m_ctx_params;
    llama_context* m_ctx {nullptr};

    // Hold both local and remote engines
    // ローカルエンジンとリモートエンジンの両方を保持
    LlamaResponseGenerator* m_localGenerator {nullptr};
    LlamaResponseGeneratorReplica* m_remoteGenerator {nullptr};

    // If a switch request occurs mid-inference, we store it here
    // 推論中に切り替え要求が来た場合、ここで待機させる
    std::optional<EngineMode> m_pendingEngineSwitchMode;
    EngineMode m_currentEngineMode {Mode_Local};

    // Worker thread for local inference
    // ローカル推論用のワーカースレッド
    QThread* m_localWorkerThread {nullptr};

    // Flags for ongoing generation and UI updates
    // 推論中かどうか、UI更新用のフラグなど
    bool m_inProgress {false};
    int m_currentAssistantIndex {-1};

    // Chat data
    // チャットデータ
    QString m_user_input;
    ChatMessageModel m_messages;

    // Engine init status
    // エンジン初期化状態
    bool m_engine_initialized {false};
};

#endif // LLAMACHATENGINE_H

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
    Q_PROPERTY(QString user_input READ user_input WRITE setUser_input RESET resetUser_input NOTIFY user_inputChanged FINAL)
    Q_PROPERTY(EngineMode currentEngineMode READ currentEngineMode NOTIFY currentEngineModeChanged FINAL)
    Q_PROPERTY(QString ip_address READ ip_address WRITE setIp_address NOTIFY ip_addressChanged FINAL)
    Q_PROPERTY(int port_number READ port_number WRITE setPort_number NOTIFY port_numberChanged FINAL)
    Q_PROPERTY(bool local_initialized READ local_initialized WRITE setLocal_initialized NOTIFY local_initializedChanged FINAL)
    Q_PROPERTY(bool remote_initialized READ remote_initialized WRITE setRemote_initialized NOTIFY remote_initializedChanged FINAL)

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

    QString user_input() const;
    Q_INVOKABLE void setUser_input(const QString &new_user_input);
    void resetUser_input();

    EngineMode currentEngineMode() const;

    QString ip_address() const;
    void setIp_address(const QString &new_ip_address);

    int port_number() const;
    void setPort_number(int new_port_number);

    bool local_initialized() const;
    void setLocal_initialized(bool new_local_initialized);

    bool remote_initialized() const;
    void setRemote_initialized(bool new_remote_initialized);

public slots:
    // Process user input (apply template, request generation)
    // ユーザー入力を処理 (テンプレート適用, 推論リクエスト)
    void handle_new_user_input();

signals:
    // Notifies user_input changes
    // user_inputが変わったら通知
    void user_inputChanged();

    // Called to request generation with prepared messages
    // 準備されたメッセージで推論を行うようリクエスト
    void requestGeneration(const QList<LlamaChatMessage>& messages);

    // Informs that the engine mode changed
    // エンジンモードが変わったことを通知
    void currentEngineModeChanged();

    // Informs IP/port changes
    // IPアドレス/ポート番号が変わったことを通知
    void ip_addressChanged();
    void port_numberChanged();

    // Informs local/remote init status changes
    // ローカル/リモート初期化状態が変わったことを通知
    void local_initializedChanged();
    void remote_initializedChanged();

    void inferenceErrorToQML(const QString& error_message);

private slots:
    // Receives partial AI response
    // 部分的なAI応答を受け取る
    void onPartialResponse(const QString &text_so_far);

    // Receives final AI response
    // 最終的なAI応答を受け取る
    void onGenerationFinished(const QString &final_response);

    // Called after engine init is done
    // エンジン初期化完了後に呼ばれる
    void onEngineInitFinished();

private:
    // Runs heavy init in another thread
    // 重い初期化を別スレッドで実行
    void doEngineInit();

    // Immediately switch between local/remote
    // ローカル/リモートを即時切り替え
    void doImmediateEngineSwitch(EngineMode new_mode);

    // Internal configuration helpers
    // 内部的な設定用ヘルパー
    void configureRemoteSignalSlots();
    void configureLocalSignalSlots();
    void configureRemoteObjects();
    void updateRemoteInitializationStatus();

    // Not meant to be called by QML
    // QMLからは呼ばれない想定
    void setCurrentEngineMode(EngineMode new_current_engine_mode);

    // Constants (English/Japanese)
    // 定数
    static constexpr int m_n_gl {99};
    static constexpr int m_n_ctx {2048};

    // Default LLaMA model path from CMake
    // CMakeで指定したデフォルトLLaMAモデルパス
    static const std::string m_model_path;

    // LLaMA model/context parameters
    // LLaMAモデル/コンテキストのパラメータ
    llama_model_params m_model_params;
    llama_model* m_model {nullptr};
    llama_context_params m_ctx_params;
    llama_context* m_ctx {nullptr};

    // Engines: local / remote
    // エンジン(ローカル/リモート)
    LlamaResponseGenerator* m_local_generator {nullptr};
    LlamaResponseGeneratorReplica* m_remote_generator {nullptr};

    // Remote connection
    // リモート接続設定
    QString m_ip_address;
    int m_port_number;
    QRemoteObjectNode* m_remote_mode {nullptr};

    // Switch request mid-inference
    // 推論中に切り替え要求がきた場合の保管
    std::optional<EngineMode> m_pending_engine_switch_mode;
    EngineMode m_current_engine_mode {Mode_Uninitialized};

    // Worker thread for local inference
    // ローカル推論用ワーカースレッド
    QThread* m_local_worker_thread {nullptr};

    // Generation/inference flags
    // 推論状態フラグ
    bool m_in_progress {false};
    int m_current_assistant_index {-1};

    // Chat data
    // チャットデータ
    QString m_user_input;
    ChatMessageModel m_messages;

    // Engine init status
    // エンジン初期化ステータス
    bool m_local_initialized {false};
    bool m_remote_initialized {false};
};

#endif // LLAMACHATENGINE_H

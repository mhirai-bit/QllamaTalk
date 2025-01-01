#ifndef LLAMACHATENGINE_H
#define LLAMACHATENGINE_H

#include <QObject>
#include <QQmlEngine>
#include "llama.h"
#include "chatmessagemodel.h"
#include "llamaresponsegenerator.h"

// Main controller for chat logic
// チャットロジックのメインコントローラ
class LlamaChatEngine : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    // QML properties
    // QMLプロパティ
    Q_PROPERTY(ChatMessageModel* messages READ messages CONSTANT)
    Q_PROPERTY(QString user_input READ user_input WRITE setUser_input RESET resetUser_input NOTIFY user_inputChanged FINAL)
    Q_PROPERTY(bool engine_initialized READ engine_initialized NOTIFY engine_initializedChanged FINAL)

public:
    explicit LlamaChatEngine(QObject *parent = nullptr);
    ~LlamaChatEngine() override;

    // Expose ChatMessageModel
    // ChatMessageModelを公開
    ChatMessageModel* messages();
    // user_input accessors
    // user_inputのゲッター/セッター
    QString user_input() const;
    Q_INVOKABLE void setUser_input(const QString &newUser_input);
    void resetUser_input();

    // Engine initialization status
    // エンジン初期化状態
    bool engine_initialized() const;

public slots:
    // Called when user_input changes
    // user_input変更時に呼ばれる
    void handle_new_user_input();

signals:
    // Notify QML that user_input changed
    // user_inputの変化をQMLに通知
    void user_inputChanged();
    // Request LlamaResponseGenerator to generate text
    // テキスト生成をリクエスト
    void requestGeneration(const QString &prompt);

    // Notify QML that engine init status changed
    // エンジン初期化状態の変化を通知
    void engine_initializedChanged();

private slots:
    // Update UI for partial AI responses
    // AI応答の途中結果をUIに反映
    void onPartialResponse(const QString &textSoFar);
    // Finalize UI after generation finishes
    // 生成完了後のUI更新
    void onGenerationFinished(const QString &finalResponse);
    // Set up after engine init completes
    // エンジン初期化完了後のセットアップ
    void onEngineInitFinished();

private:
    // Model and context configuration
    // モデルとコンテキスト設定
    static constexpr int m_ngl {99};
    static constexpr int m_n_ctx {2048};
    static const std::string m_model_path;
    llama_model_params m_model_params;
    llama_model* m_model;
    llama_context_params m_ctx_params;
    llama_context* m_ctx;

    // Worker for text generation (runs in separate thread)
    // 別スレッドで実行されるテキスト生成ワーカー
    LlamaResponseGenerator* m_response_generator;

    // True if generation is ongoing
    // 生成中ならtrue
    bool m_inProgress {false};
    // Index of the latest assistant message
    // 最後に追加されたassistantメッセージのIndex
    int m_currentAssistantIndex {-1};

    // User's input text
    // ユーザー入力テキスト
    QString m_user_input;
    // Chat messages model
    // チャットメッセージモデル
    ChatMessageModel m_messages;

    // Engine init flag
    // エンジン初期化フラグ
    bool m_engine_initialized {false};

    // Optional synchronous generation placeholder
    // 同期生成のプレースホルダ
    void generate(const std::string &prompt, std::string &response);

    // Actual heavy init process (async)
    // 実際の重い初期化処理（非同期）
    void doEngineInit();

    // Only this class can set engine_initialized
    // engine_initializedの変更はこのクラスだけ
    void setEngine_initialized(bool newEngine_initialized);
};

#endif // LLAMACHATENGINE_H

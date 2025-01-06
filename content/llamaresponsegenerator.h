#ifndef LLAMA_RESPONSE_GENERATOR_H
#define LLAMA_RESPONSE_GENERATOR_H

#include <QObject>
#include <QString>
#include "llama.h"
#include "rep_LlamaResponseGenerator_replica.h"

/*
  LlamaResponseGenerator:
    - Generates text using a LLaMA model and context, typically in a worker thread
    - Emits partial/final results for incremental updates

  LLaMAモデルとコンテキストを用いてテキスト生成を行うクラス。
  ワーカースレッドで動作し、途中経過と最終結果をシグナルで通知する。
*/
class LlamaResponseGenerator : public QObject {
    Q_OBJECT

public:
    // Constructor is disabled if model/context not provided
    // モデル/コンテキストが提供されない場合はコンストラクタを無効化
    LlamaResponseGenerator() = delete;

    // Constructor: takes optional parent plus llama_model/llama_context
    // コンストラクタ: 親と llama_model/llama_context を受け取る
    explicit LlamaResponseGenerator(QObject *parent = nullptr,
                                    llama_model* model = nullptr,
                                    llama_context* ctx = nullptr);

    // Destructor: frees sampler if created
    // デストラクタ: 作成されたサンプラーがあれば解放
    ~LlamaResponseGenerator() override;

public slots:
    // Generates text from the given messages, emits partial/final
    // 指定メッセージからテキスト生成し、途中/最終結果をemit
    void generate(const QList<LlamaChatMessage>& messages);

signals:
    // Emitted during generation for incremental text
    // 生成中に逐次テキストを通知
    void partialResponseReady(const QString &textSoFar);

    // Emitted when generation is done
    // 生成完了時に通知
    void generationFinished(const QString &finalResponse);

    // Emitted on error (e.g., tokenization failure)
    // エラー発生時に通知 (例: トークナイズ失敗)
    void generationError(const QString &errorMessage);

    void initialized();

private:
    // Holds LLaMA model/context/sampler references
    // LLaMAのモデル/コンテキスト/サンプル参照
    llama_model*   mModel   {nullptr};
    llama_context* mCtx     {nullptr};
    llama_sampler* mSampler {nullptr};

    // Initializes the sampler with default settings
    // サンプラーをデフォルト設定で初期化
    void initialize_sampler();

    // Converts from QList<LlamaChatMessage> to std::vector<llama_chat_message>
    // QList<LlamaChatMessage> → std::vector<llama_chat_message> に変換
    std::vector<llama_chat_message> to_llama_messages(const QList<LlamaChatMessage> &user_messages);
};

#endif // LLAMA_RESPONSE_GENERATOR_H

#ifndef LLAMA_RESPONSE_GENERATOR_H
#define LLAMA_RESPONSE_GENERATOR_H

#include <QObject>
#include <QString>
#include "llama.h"
#include "rep_LlamaResponseGenerator_replica.h"

/*
  LlamaResponseGenerator:
    - Generates text using a LLaMA model/context (often in a worker thread)
    - Emits partial/final signals for incremental updates

  LlamaResponseGeneratorクラス:
    - LLaMAのモデル/コンテキストを用いてテキスト生成（多くの場合ワーカースレッドで動作）
    - 部分的な更新や最終結果をシグナルで通知
*/
class LlamaResponseGenerator : public QObject
{
    Q_OBJECT

public:
    //--------------------------------------------------------------------------
    // Constructor / Destructor
    // コンストラクタ / デストラクタ
    //--------------------------------------------------------------------------
    LlamaResponseGenerator() = delete;  // No default construction if no model/ctx

    explicit LlamaResponseGenerator(QObject* parent = nullptr,
                                    llama_model* model = nullptr,
                                    llama_context* ctx = nullptr);

    ~LlamaResponseGenerator() override;

public slots:
    //--------------------------------------------------------------------------
    // Generates text from the provided messages, emits partial/final signals
    // 指定メッセージからテキストを生成し、途中・最終シグナルをemit
    //--------------------------------------------------------------------------
    void generate(const QList<LlamaChatMessage>& messages);

signals:
    //--------------------------------------------------------------------------
    // Signals for incremental / final output, or error
    // インクリメンタル・最終出力、エラー用シグナル
    //--------------------------------------------------------------------------
    void partialResponseReady(const QString &textSoFar);
    void generationFinished(const QString &finalResponse);
    void generationError(const QString &errorMessage);
    void initialized();

private:
    //--------------------------------------------------------------------------
    // Private Helper Methods
    // プライベートヘルパーメソッド
    //--------------------------------------------------------------------------
    void initializeSampler();
    std::vector<llama_chat_message> toLlamaMessages(const QList<LlamaChatMessage> &userMessages);

    //--------------------------------------------------------------------------
    // Member Variables
    // メンバ変数
    //--------------------------------------------------------------------------
    llama_model*   m_model   {nullptr};  // LLaMA model
    llama_context* m_ctx     {nullptr};  // LLaMA context
    llama_sampler* m_sampler {nullptr};  // LLaMA sampler
};

#endif // LLAMA_RESPONSE_GENERATOR_H

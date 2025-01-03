#include "llamaresponsegenerator.h"
#include <QDebug>

// Constructor: initializes the response generator with references to the LLaMA model and context
// コンストラクタ: LLaMAモデルとコンテキストへの参照でレスポンスジェネレータを初期化
LlamaResponseGenerator::LlamaResponseGenerator(QObject *parent,
                                               llama_model *model,
                                               llama_context *ctx)
    : QObject(parent), m_model(model), m_ctx(ctx) {
}

// Destructor: cleans up the sampler if it has been created
// デストラクタ: サンプラーが作成されていた場合はクリーンアップ
LlamaResponseGenerator::~LlamaResponseGenerator() {
    if (m_sampler) {
        llama_sampler_free(m_sampler);
    }
}

// Called in the worker thread to generate text from a given prompt
// ワーカースレッド内で、指定したプロンプトからテキストを生成する
void LlamaResponseGenerator::generate(const QList<LlamaChatMessage>& messages) {
    // If this is the first generation, set up the sampler
    // 初回生成ならサンプラーを初期化
    if (!m_sampler) {
        initializeSampler();
    }

    qDebug() << "About to QMetaObject::invokeMethod(generate) with messages size = " << messages.size();

    static std::vector<char> formatted(llama_n_ctx(m_ctx));
    static int prev_len {0};

    std::vector<llama_chat_message> messages_for_llama = toLlamaMessages(messages);

    // Apply chat template
    // チャットテンプレートを適用
    int new_len = llama_chat_apply_template(
        m_model, nullptr, messages_for_llama.data(), messages_for_llama.size(),
        true, formatted.data(), formatted.size()
        );
    if (new_len > static_cast<int>(formatted.size())) {
        formatted.resize(new_len);
        new_len = llama_chat_apply_template(
            m_model, nullptr, messages_for_llama.data(), messages_for_llama.size(),
            true, formatted.data(), formatted.size()
            );
    }
    if (new_len < 0) {
        fprintf(stderr, "Failed to apply chat template.\n");
        return;
    }

    std::string promptStd(formatted.begin() + prev_len, formatted.begin() + new_len);

    // Convert QString to std::string for use with LLaMA API
    // LLaMA APIで使用するためQStringをstd::stringに変換
    std::string response;

    // Tokenize prompt text. Negative indicates returning token count, ignoring special tokens
    // プロンプトテキストをトークン化。負数で呼ぶと特別トークンを無視したトークン数のみ返却
    const int n_prompt_tokens = -llama_tokenize(
        m_model,
        promptStd.c_str(),
        promptStd.size(),
        nullptr,
        0,
        true,  // is_prefix
        true   // is_bos
        );

    // Prepare vector for tokenized prompt
    // トークン化結果を格納するベクタを準備
    std::vector<llama_token> prompt_tokens(n_prompt_tokens);

    // Tokenize again, now storing tokens in the vector
    // もう一度トークン化し、今度はベクタに格納
    if (llama_tokenize(
            m_model,
            promptStd.c_str(),
            promptStd.size(),
            prompt_tokens.data(),
            prompt_tokens.size(),
            llama_get_kv_cache_used_cells(m_ctx) == 0,
            true) < 0) {
        emit generationError("failed to tokenize the prompt");
    }

    // Prepare a single batch for decoding
    // デコード用のバッチを1つ用意
    llama_batch batch = llama_batch_get_one(prompt_tokens.data(),
                                            prompt_tokens.size());
    llama_token new_token_id;

    static constexpr int max_reply_tokens {1024};
    static constexpr int extra_cutoff_tokens {32};
    int generated_token_count {0};

    // Decode in a loop until an end-of-generation token is reached
    // 終了トークンが出るまでループしながらデコード
    while (true) {
        const int n_ctx      = llama_n_ctx(m_ctx);
        const int n_ctx_used = llama_get_kv_cache_used_cells(m_ctx);

        // Evaluate the batch
        // バッチを評価
        if (llama_decode(m_ctx, batch)) {
            emit generationError("failed to decode");
            break;
        }

        // Sample the next token
        // 次のトークンをサンプリング
        new_token_id = llama_sampler_sample(m_sampler, m_ctx, -1);

        // Check if end-of-generation token
        // 終了トークンならループを抜ける
        if (llama_token_is_eog(m_model, new_token_id)) {
            break;
        }

        // Convert token to text
        // トークンを文字列に変換
        char buf[256] = {};
        int n = llama_token_to_piece(m_model, new_token_id, buf,
                                     sizeof(buf), 0, true);
        if (n < 0) {
            emit generationError("failed to convert token to piece");
            break;
        }

        std::string piece(buf, n);
        printf("%s", piece.c_str());
        fflush(stdout);

        // Accumulate partial text
        // 部分的なテキストを蓄積
        response += piece;

        // Notify partial response
        // 部分的なレスポンスを通知
        emit partialResponseReady(QString::fromStdString(response));

        // Prepare next batch
        // 次のバッチを準備
        batch = llama_batch_get_one(&new_token_id, 1);

        ++generated_token_count;
        if (generated_token_count > max_reply_tokens) {
            if (piece.find('\n') != std::string::npos) {
                qDebug() << "Cutting off the generation at a newline character";
                break;
            } else if (generated_token_count > max_reply_tokens + extra_cutoff_tokens) {
                qDebug() << "Cutting off the generation +"
                         << extra_cutoff_tokens
                         << " tokens";
                break;
            }
        }
    }

    // Update prev_len
    // prev_len を更新
    prev_len = llama_chat_apply_template(
        m_model, nullptr, messages_for_llama.data(), messages_for_llama.size(),
        false, nullptr, 0
        );
    if (prev_len < 0) {
        fprintf(stderr, "Failed to apply chat template.\n");
    }

    // Done: emit final response
    // 完了: 最終レスポンスを通知
    emit generationFinished(QString::fromStdString(response));
}

// userMessages (QList<LlamaChatMessage>) → llamaMessages (std::vector<llama_chat_message>)
std::vector<llama_chat_message> LlamaResponseGenerator::toLlamaMessages(const QList<LlamaChatMessage> &userMessages)
{
    std::vector<llama_chat_message> llamaMessages;
    llamaMessages.reserve(userMessages.size());

    for (const auto &um : userMessages) {
        llama_chat_message lm;
        lm.role = strdup(um.role().toUtf8().constData());
        lm.content = strdup(um.content().toUtf8().constData());
        llamaMessages.push_back(lm);
    }

    return llamaMessages;
}


// Initializes the sampler with default parameters
// サンプラーをデフォルトパラメータで初期化
void LlamaResponseGenerator::initializeSampler() {
    m_sampler = llama_sampler_chain_init(llama_sampler_chain_default_params());
    llama_sampler_chain_add(m_sampler, llama_sampler_init_min_p(0.05f, 1));
    llama_sampler_chain_add(m_sampler, llama_sampler_init_temp(0.8f));
    llama_sampler_chain_add(m_sampler, llama_sampler_init_dist(LLAMA_DEFAULT_SEED));
}

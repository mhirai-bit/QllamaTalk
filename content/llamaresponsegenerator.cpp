#include "llamaresponsegenerator.h"
#include <QDebug>

/*
  Constructor:
    - Stores llama_model/llama_context references
    - Typically parent is nullptr if run in worker thread

  コンストラクタ:
    - llama_model/llama_context の参照を保持
    - ワーカースレッドで使う場合、parent は通常 nullptr
*/
LlamaResponseGenerator::LlamaResponseGenerator(QObject *parent,
                                               llama_model *model,
                                               llama_context *ctx)
    : QObject(parent)
    , m_model(model)
    , m_ctx(ctx)
{
}

/*
  Destructor:
    - Frees m_sampler if allocated

  デストラクタ:
    - m_sampler が作成済みなら解放
*/
LlamaResponseGenerator::~LlamaResponseGenerator()
{
    if (m_sampler) {
        llama_sampler_free(m_sampler);
    }
}

/*
  generate(...):
    - Invoked in worker thread to produce text from given messages
    - Emits partialResponseReady for incremental output
    - Emits generationFinished when complete

  generate(...):
    - ワーカースレッドで呼び出され、与えられたメッセージ群からテキスト生成を行う
    - 部分的な結果は partialResponseReady、最終結果は generationFinished を emit
*/
void LlamaResponseGenerator::generate(const QList<LlamaChatMessage>& messages)
{
    // Initialize sampler on first call
    // 初回呼び出し時にサンプラー初期化
    if (!m_sampler) {
        initialize_sampler();
    }

    qDebug() << "About to QMetaObject::invokeMethod(generate). messages.size() ="
             << messages.size();

    // Use static buffers to hold formatted text / prev length
    // フォーマットされたテキストや前回長さをstatic変数で保持
    static std::vector<char> formatted_vec(llama_n_ctx(m_ctx));
    static int prev_len {0};

    // Convert input messages to llama_chat_message vector
    // 入力メッセージを llama_chat_message のベクタに変換
    std::vector<llama_chat_message> messages_for_llama = to_llama_messages(messages);

    // Apply chat template
    // チャットテンプレートを適用
    int new_len = llama_chat_apply_template(
        m_model, nullptr,
        messages_for_llama.data(), messages_for_llama.size(),
        true,
        formatted_vec.data(), formatted_vec.size()
        );
    if (new_len > static_cast<int>(formatted_vec.size())) {
        formatted_vec.resize(new_len);
        new_len = llama_chat_apply_template(
            m_model, nullptr,
            messages_for_llama.data(), messages_for_llama.size(),
            true,
            formatted_vec.data(), formatted_vec.size()
            );
    }
    if (new_len < 0) {
        fprintf(stderr, "Failed to apply chat template.\n");
        return;
    }

    // Extract new portion of prompt
    // 新しく追加されたプロンプト部分を取り出す
    std::string prompt_std(formatted_vec.begin() + prev_len,
                           formatted_vec.begin() + new_len);
    std::string response;

    // Tokenize the prompt text
    // プロンプトをトークナイズ
    const int n_prompt_tokens = -llama_tokenize(
        m_model,
        prompt_std.c_str(),
        prompt_std.size(),
        nullptr,
        0,
        true,  // is_prefix
        true   // is_bos
        );

    // Prepare tokens
    // トークンのベクタ準備
    std::vector<llama_token> prompt_tokens(n_prompt_tokens);
    if (llama_tokenize(
            m_model,
            prompt_std.c_str(),
            prompt_std.size(),
            prompt_tokens.data(),
            prompt_tokens.size(),
            llama_get_kv_cache_used_cells(m_ctx) == 0,
            true) < 0)
    {
        emit generationError("failed to tokenize the prompt");
    }

    // Single batch for decode
    // デコード用バッチを1つ作成
    llama_batch batch = llama_batch_get_one(prompt_tokens.data(),
                                            prompt_tokens.size());
    llama_token new_token_id;

    static constexpr int max_reply_tokens    {1024};
    static constexpr int extra_cutoff_tokens {32};
    int generated_token_count {0};

    // Decode tokens until end-of-generation
    // 終了トークンに達するまでトークンをデコード
    while (true) {
        if (llama_decode(m_ctx, batch)) {
            emit generationError("failed to decode");
            break;
        }
        new_token_id = llama_sampler_sample(m_sampler, m_ctx, -1);

        if (llama_token_is_eog(m_model, new_token_id)) {
            break;
        }

        char buf[256] = {};
        const int n = llama_token_to_piece(m_model, new_token_id, buf,
                                           sizeof(buf), 0, true);
        if (n < 0) {
            emit generationError("failed to convert token to piece");
            break;
        }

        std::string piece(buf, n);
        printf("%s", piece.c_str());
        fflush(stdout);

        response += piece;
        emit partialResponseReady(QString::fromStdString(response));

        // Add next token to batch
        // 次のトークンをバッチに追加
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

    // Update prev_len for next
    // 次の生成に備えて prev_len を更新
    prev_len = llama_chat_apply_template(
        m_model, nullptr,
        messages_for_llama.data(), messages_for_llama.size(),
        false,
        nullptr, 0
        );
    if (prev_len < 0) {
        fprintf(stderr, "Failed to apply chat template.\n");
    }

    emit generationFinished(QString::fromStdString(response));
}

/*
  to_llama_messages():
    - Converts from QList<LlamaChatMessage> to std::vector<llama_chat_message>
    - Freed automatically if needed

  to_llama_messages():
    - QList<LlamaChatMessage> を std::vector<llama_chat_message> に変換
    - 必要なら自動的に解放される
*/
std::vector<llama_chat_message>
LlamaResponseGenerator::to_llama_messages(const QList<LlamaChatMessage> &user_messages)
{
    std::vector<llama_chat_message> llama_messages;
    llama_messages.reserve(user_messages.size());

    for (const auto &um : user_messages) {
        llama_chat_message lm;
        lm.role    = strdup(um.role().toUtf8().constData());
        lm.content = strdup(um.content().toUtf8().constData());
        llama_messages.push_back(lm);
    }
    return llama_messages;
}

/*
  initialize_sampler():
    - Sets up default chain of sampler modules
    - Called once before generation if no sampler exists

  initialize_sampler():
    - デフォルトのサンプラーチェーンを構築
    - サンプラーが無い場合、生成前に一度だけ呼ばれる
*/
void LlamaResponseGenerator::initialize_sampler()
{
    m_sampler = llama_sampler_chain_init(llama_sampler_chain_default_params());
    llama_sampler_chain_add(m_sampler, llama_sampler_init_min_p(0.05f, 1));
    llama_sampler_chain_add(m_sampler, llama_sampler_init_temp(0.8f));
    llama_sampler_chain_add(m_sampler, llama_sampler_init_dist(LLAMA_DEFAULT_SEED));
}

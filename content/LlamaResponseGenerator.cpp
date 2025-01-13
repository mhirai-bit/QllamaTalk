#include "LlamaResponseGenerator.h"
#include <QDebug>

/*
  Constructor:
    - Stores llama_model / llama_context references
    - Often parent is nullptr in worker thread usage

  コンストラクタ:
    - llama_model / llama_context の参照を保持
    - ワーカースレッドで使う場合、parent は通常 nullptr
*/
LlamaResponseGenerator::LlamaResponseGenerator(QObject* parent,
                                               llama_model* model,
                                               llama_context* ctx)
    : QObject(parent)
    , m_model(model)
    , m_ctx(ctx)
{
}

/*
  Destructor:
    - Frees sampler if allocated

  デストラクタ:
    - サンプラーが作成されていれば解放
*/
LlamaResponseGenerator::~LlamaResponseGenerator()
{
    if (m_sampler) {
        llama_sampler_free(m_sampler);
        m_sampler = nullptr;
    }
}

/*
  generate(...):
    - Called (usually in worker thread) to produce text from messages
    - Emits partialResponseReady(...) for incremental output
    - Emits generationFinished(...) when complete

  generate(...):
    - （通常ワーカースレッドで）メッセージからテキスト生成
    - 部分的な出力は partialResponseReady(...) をemit
    - 完了時に generationFinished(...) をemit
*/
void LlamaResponseGenerator::generate(const QList<LlamaChatMessage>& messages)
{
    // If sampler not ready, initialize once
    // サンプラー未初期化なら1度だけ初期化
    if (!m_sampler) {
        initializeSampler();
    }

    qDebug() << "[LlamaResponseGenerator::generate] messages.size() =" << messages.size();

    // Use static buffers to hold formatted text and track previous length
    // フォーマット済みテキストや前回長さをstaticで保持
    static std::vector<char> formattedVec(llama_n_ctx(m_ctx));
    static int prevLen {0};

    // Convert QList<LlamaChatMessage> → std::vector<llama_chat_message>
    std::vector<llama_chat_message> llamaMsgs = toLlamaMessages(messages);

    // Apply chat template to generate prompt
    // チャットテンプレートを適用してプロンプト生成
    int newLen = llama_chat_apply_template(m_model,
                                           nullptr,
                                           llamaMsgs.data(),
                                           llamaMsgs.size(),
                                           true,
                                           formattedVec.data(),
                                           formattedVec.size());
    if (newLen > static_cast<int>(formattedVec.size())) {
        // Resize if needed
        formattedVec.resize(newLen);
        newLen = llama_chat_apply_template(m_model,
                                           nullptr,
                                           llamaMsgs.data(),
                                           llamaMsgs.size(),
                                           true,
                                           formattedVec.data(),
                                           formattedVec.size());
    }
    if (newLen < 0) {
        fprintf(stderr, "[LlamaResponseGenerator] Failed to apply chat template.\n");
        return;
    }

    // Extract newly added portion of prompt
    // 新たに追加されたプロンプト部分を抜き出す
    std::string promptStd(formattedVec.begin() + prevLen,
                          formattedVec.begin() + newLen);

    std::string response;
    const int nPromptTokens = -llama_tokenize(m_model,
                                              promptStd.c_str(),
                                              promptStd.size(),
                                              nullptr,
                                              0,
                                              true,  // is_prefix
                                              true); // is_bos

    // Allocate space for tokens
    // トークンを格納
    std::vector<llama_token> promptTokens(nPromptTokens);
    if (llama_tokenize(m_model,
                       promptStd.c_str(),
                       promptStd.size(),
                       promptTokens.data(),
                       promptTokens.size(),
                       llama_get_kv_cache_used_cells(m_ctx) == 0,
                       true) < 0) {
        emit generationError("failed to tokenize the prompt");
    }

    // Prepare single batch for decoding
    llama_batch batch = llama_batch_get_one(promptTokens.data(), promptTokens.size());
    llama_token newTokenId;

    static constexpr int maxReplyTokens    = 1024;
    static constexpr int extraCutoffTokens = 32;
    int generatedTokenCount = 0;

    // Decode tokens until end-of-generation
    // 終了トークンに達するまでデコードし続ける
    while (true) {
        if (llama_decode(m_ctx, batch)) {
            emit generationError("failed to decode");
            break;
        }
        newTokenId = llama_sampler_sample(m_sampler, m_ctx, -1);

        // If end-of-generation token
        if (llama_token_is_eog(m_model, newTokenId)) {
            break;
        }

        char buf[256] = {};
        int n = llama_token_to_piece(m_model, newTokenId, buf, sizeof(buf), 0, true);
        if (n < 0) {
            emit generationError("failed to convert token to piece");
            break;
        }

        std::string piece(buf, n);
        response += piece;

        // Emit partial text
        emit partialResponseReady(QString::fromStdString(response));

        // Prepare for next token
        batch = llama_batch_get_one(&newTokenId, 1);

        ++generatedTokenCount;
        if (generatedTokenCount > maxReplyTokens) {
            // Cut off if too long (end on newline or extra tokens)
            if (piece.find('\n') != std::string::npos) {
                qDebug() << "[LlamaResponseGenerator] Cutting off at newline.";
                break;
            } else if (generatedTokenCount > maxReplyTokens + extraCutoffTokens) {
                qDebug() << "[LlamaResponseGenerator] Cutting off after extra tokens.";
                break;
            }
        }
    }

    // Update prevLen for next call
    prevLen = llama_chat_apply_template(m_model,
                                        nullptr,
                                        llamaMsgs.data(),
                                        llamaMsgs.size(),
                                        false,
                                        nullptr,
                                        0);
    if (prevLen < 0) {
        fprintf(stderr, "[LlamaResponseGenerator] Failed to apply chat template.\n");
    }

    // Finally emit finished text
    emit generationFinished(QString::fromStdString(response));
}

/*
  toLlamaMessages(...):
    - Helper to convert from QList<LlamaChatMessage> to std::vector<llama_chat_message>
    - Freed automatically if needed

  toLlamaMessages(...):
    - QList<LlamaChatMessage> から std::vector<llama_chat_message> へ変換
    - 必要なければ自動解放される
*/
std::vector<llama_chat_message>
LlamaResponseGenerator::toLlamaMessages(const QList<LlamaChatMessage> &userMessages)
{
    std::vector<llama_chat_message> llamaMessages;
    llamaMessages.reserve(userMessages.size());

    for (const auto &um : userMessages) {
        llama_chat_message lm;
        lm.role    = strdup(um.role().toUtf8().constData());
        lm.content = strdup(um.content().toUtf8().constData());
        llamaMessages.push_back(lm);
    }
    return llamaMessages;
}

/*
  initializeSampler():
    - Sets up a default chain of sampler modules
    - Called once if no sampler exists

  initializeSampler():
    - デフォルトのサンプラーチェーンを作成
    - まだサンプラーがなければ初回のみ呼ばれる
*/
void LlamaResponseGenerator::initializeSampler()
{
    m_sampler = llama_sampler_chain_init(llama_sampler_chain_default_params());
    llama_sampler_chain_add(m_sampler, llama_sampler_init_min_p(0.05f, 1));
    llama_sampler_chain_add(m_sampler, llama_sampler_init_temp(0.8f));
    llama_sampler_chain_add(m_sampler, llama_sampler_init_dist(LLAMA_DEFAULT_SEED));
}

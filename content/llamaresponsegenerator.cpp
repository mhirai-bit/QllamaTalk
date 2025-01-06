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
    , mModel(model)
    , mCtx(ctx)
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
    if (mSampler) {
        llama_sampler_free(mSampler);
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
    if (!mSampler) {
        initialize_sampler();
    }

    qDebug() << "About to QMetaObject::invokeMethod(generate). messages.size() ="
             << messages.size();

    // Use static buffers to hold formatted text / prev length
    // フォーマットされたテキストや前回長さをstatic変数で保持
    static std::vector<char> formattedVec(llama_n_ctx(mCtx));
    static int prevLen {0};

    // Convert input messages to llama_chat_message vector
    // 入力メッセージを llama_chat_message のベクタに変換
    std::vector<llama_chat_message> messagesForLlama = to_llama_messages(messages);

    // Apply chat template
    // チャットテンプレートを適用
    int newLen = llama_chat_apply_template(
        mModel, nullptr,
        messagesForLlama.data(), messagesForLlama.size(),
        true,
        formattedVec.data(), formattedVec.size()
        );

    if (newLen > static_cast<int>(formattedVec.size())) {
        formattedVec.resize(newLen);
        newLen = llama_chat_apply_template(
            mModel, nullptr,
            messagesForLlama.data(), messagesForLlama.size(),
            true,
            formattedVec.data(), formattedVec.size()
            );
    }
    if (newLen < 0) {
        fprintf(stderr, "Failed to apply chat template.\n");
        return;
    }

    // Extract new portion of prompt
    // 新しく追加されたプロンプト部分を取り出す
    std::string promptStd(formattedVec.begin() + prevLen,
                          formattedVec.begin() + newLen);
    std::string response;

    // Tokenize the prompt text
    // プロンプトをトークナイズ
    const int nPromptTokens = -llama_tokenize(
        mModel,
        promptStd.c_str(),
        promptStd.size(),
        nullptr,
        0,
        true,  // is_prefix
        true   // is_bos
        );

    // Prepare tokens
    // トークンのベクタ準備
    std::vector<llama_token> promptTokens(nPromptTokens);
    if (llama_tokenize(
            mModel,
            promptStd.c_str(),
            promptStd.size(),
            promptTokens.data(),
            promptTokens.size(),
            llama_get_kv_cache_used_cells(mCtx) == 0,
            true) < 0)
    {
        emit generationError("failed to tokenize the prompt");
    }

    // Single batch for decode
    // デコード用バッチを1つ作成
    llama_batch batch = llama_batch_get_one(promptTokens.data(),
                                            promptTokens.size());
    llama_token newTokenId;

    static constexpr int maxReplyTokens    {1024};
    static constexpr int extraCutoffTokens {32};
    int generatedTokenCount {0};

    // Decode tokens until end-of-generation
    // 終了トークンに達するまでトークンをデコード
    while (true) {
        if (llama_decode(mCtx, batch)) {
            emit generationError("failed to decode");
            break;
        }
        newTokenId = llama_sampler_sample(mSampler, mCtx, -1);

        if (llama_token_is_eog(mModel, newTokenId)) {
            break;
        }

        char buf[256] = {};
        const int n = llama_token_to_piece(mModel, newTokenId, buf,
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
        batch = llama_batch_get_one(&newTokenId, 1);

        ++generatedTokenCount;
        if (generatedTokenCount > maxReplyTokens) {
            if (piece.find('\n') != std::string::npos) {
                qDebug() << "Cutting off the generation at a newline character";
                break;
            } else if (generatedTokenCount > maxReplyTokens + extraCutoffTokens) {
                qDebug() << "Cutting off the generation +"
                         << extraCutoffTokens
                         << " tokens";
                break;
            }
        }
    }

    // Update prevLen for next
    // 次の生成に備えて prev_len を更新
    prevLen = llama_chat_apply_template(
        mModel, nullptr,
        messagesForLlama.data(), messagesForLlama.size(),
        false,
        nullptr, 0
        );
    if (prevLen < 0) {
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
LlamaResponseGenerator::to_llama_messages(const QList<LlamaChatMessage> &userMessages)
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
  initialize_sampler():
    - Sets up default chain of sampler modules
    - Called once before generation if no sampler exists

  initialize_sampler():
    - デフォルトのサンプラーチェーンを構築
    - サンプラーが無い場合、生成前に一度だけ呼ばれる
*/
void LlamaResponseGenerator::initialize_sampler()
{
    mSampler = llama_sampler_chain_init(llama_sampler_chain_default_params());
    llama_sampler_chain_add(mSampler, llama_sampler_init_min_p(0.05f, 1));
    llama_sampler_chain_add(mSampler, llama_sampler_init_temp(0.8f));
    llama_sampler_chain_add(mSampler, llama_sampler_init_dist(LLAMA_DEFAULT_SEED));
}

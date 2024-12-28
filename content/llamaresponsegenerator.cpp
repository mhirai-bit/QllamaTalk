#include "llamaresponsegenerator.h"

LlamaResponseGenerator::LlamaResponseGenerator(QObject *parent, llama_model *model, llama_context *ctx)
: QObject(parent), m_model(model), m_ctx(ctx)
{

}

void LlamaResponseGenerator::initializeSampler() {
    m_sampler = llama_sampler_chain_init(llama_sampler_chain_default_params());
    llama_sampler_chain_add(m_sampler, llama_sampler_init_min_p(0.05f, 1));
    llama_sampler_chain_add(m_sampler, llama_sampler_init_temp(0.8f));
    llama_sampler_chain_add(m_sampler, llama_sampler_init_dist(LLAMA_DEFAULT_SEED));
}

LlamaResponseGenerator::~LlamaResponseGenerator()
{
    if(m_sampler) {
        llama_sampler_free(m_sampler);
    }
}

void LlamaResponseGenerator::generate(const QString &prompt)
{
    if(!m_sampler) {
        initializeSampler();
    }

    std::string response {};
    std::string promptStd = prompt.toStdString();

    const int n_prompt_tokens = -llama_tokenize(m_model, promptStd.c_str(), prompt.size(), NULL, 0, true, true);
    std::vector<llama_token> prompt_tokens(n_prompt_tokens);
    if (llama_tokenize(m_model, promptStd.c_str(), promptStd.size(), prompt_tokens.data(), prompt_tokens.size(), llama_get_kv_cache_used_cells(m_ctx) == 0, true) < 0) {
        emit generationError("failed to tokenize the prompt");
        GGML_ABORT("failed to tokenize the prompt\n");
    }

    llama_batch batch = llama_batch_get_one(prompt_tokens.data(), prompt_tokens.size());
    llama_token new_token_id;
    while (true) {
        int n_ctx = llama_n_ctx(m_ctx);
        int n_ctx_used = llama_get_kv_cache_used_cells(m_ctx);

        if (llama_decode(m_ctx, batch)) {
            emit generationError("failed to decode");
            GGML_ABORT("failed to decode\n");
        }

        new_token_id = llama_sampler_sample(m_sampler, m_ctx, -1);

        if (llama_token_is_eog(m_model, new_token_id)) {
            break;
        }

        char buf[256] = {};
        int n = llama_token_to_piece(m_model, new_token_id, buf, sizeof(buf), 0, true);
        if (n < 0) {
            emit generationError("failed to convert token to piece");
            GGML_ABORT("failed to convert token to piece\n");
        }
        std::string piece(buf, n);
        printf("%s", piece.c_str());
        fflush(stdout);
        response += piece;
        // メインスレッドにトークンの追加生成が完了した旨を通知
        emit partialResponseReady(QString::fromStdString(response));

        batch = llama_batch_get_one(&new_token_id, 1);
    }

    // 完了通知
    emit generationFinished(QString::fromStdString(response));
}

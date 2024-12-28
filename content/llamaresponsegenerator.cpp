#include "llamaresponsegenerator.h"

// Constructor: initializes the response generator with references to the LLaMA model and context.
LlamaResponseGenerator::LlamaResponseGenerator(QObject *parent,
                                               llama_model *model,
                                               llama_context *ctx)
    : QObject(parent), m_model(model), m_ctx(ctx) {
}

// Initializes the sampler with default parameters (temperature, min_p, etc.).
void LlamaResponseGenerator::initializeSampler() {
    m_sampler = llama_sampler_chain_init(llama_sampler_chain_default_params());
    llama_sampler_chain_add(m_sampler, llama_sampler_init_min_p(0.05f, 1));
    llama_sampler_chain_add(m_sampler, llama_sampler_init_temp(0.8f));
    llama_sampler_chain_add(m_sampler, llama_sampler_init_dist(LLAMA_DEFAULT_SEED));
}

// Destructor: cleans up the sampler if it has been created.
LlamaResponseGenerator::~LlamaResponseGenerator() {
    if (m_sampler) {
        llama_sampler_free(m_sampler);
    }
}

// Called in the worker thread to generate text from a given prompt.
// Emits partial and final responses as they become available.
void LlamaResponseGenerator::generate(const QString &prompt) {
    // If this is the first generation, set up the sampler.
    if (!m_sampler) {
        initializeSampler();
    }

    // Convert Qstring to std::string for use with the LLaMA API.
    std::string response;
    std::string promptStd = prompt.toStdString();

    // Tokenize the prompt text. Negative sign indicates returning the token count, ignoring special tokens.
    const int n_prompt_tokens = -llama_tokenize(m_model, promptStd.c_str(),
                                                prompt.size(), nullptr, 0,
                                                true, true);

    // Prepare a vector to hold the tokenized prompt.
    std::vector<llama_token> prompt_tokens(n_prompt_tokens);

    // Tokenize again, this time storing tokens in the vector.
    if (llama_tokenize(m_model, promptStd.c_str(), promptStd.size(),
                       prompt_tokens.data(), prompt_tokens.size(),
                       llama_get_kv_cache_used_cells(m_ctx) == 0,
                       true) < 0) {
        emit generationError("failed to tokenize the prompt");
        GGML_ABORT("failed to tokenize the prompt\n");
    }

    // Prepare a single batch for decoding.
    llama_batch batch = llama_batch_get_one(prompt_tokens.data(),
                                            prompt_tokens.size());
    llama_token new_token_id;

    // Decode in a loop until the model signals an end-of-generation token.
    while (true) {
        int n_ctx = llama_n_ctx(m_ctx);
        int n_ctx_used = llama_get_kv_cache_used_cells(m_ctx);

        // Evaluate the current batch with the LLaMA context.
        if (llama_decode(m_ctx, batch)) {
            emit generationError("failed to decode");
            GGML_ABORT("failed to decode\n");
        }

        // Sample the next token from the logits.
        new_token_id = llama_sampler_sample(m_sampler, m_ctx, -1);

        // If we've reached an end-of-generation token, break.
        if (llama_token_is_eog(m_model, new_token_id)) {
            break;
        }

        // Convert the token to text. 'buf' is zero-initialized to avoid leftover characters.
        char buf[256] = {};
        int n = llama_token_to_piece(m_model, new_token_id, buf,
                                     sizeof(buf), 0, true);
        if (n < 0) {
            emit generationError("failed to convert token to piece");
            GGML_ABORT("failed to convert token to piece\n");
        }

        std::string piece(buf, n);
        printf("%s", piece.c_str());
        fflush(stdout);

        // Accumulate partial text in 'response'.
        response += piece;

        // Emit partial progress so the UI can update.
        emit partialResponseReady(QString::fromStdString(response));

        // Prepare for next iteration by creating a new batch from the newly sampled token.
        batch = llama_batch_get_one(&new_token_id, 1);
    }

    // When finished, emit the final response to the main thread for display.
    emit generationFinished(QString::fromStdString(response));
}

#include "llamachatengine.h"

const std::string LlamaChatEngine::m_model_path {"/Users/mainuser/Documents/Projects/QllamaTalk/3rdparty/llama.cpp/models/Llama-3.1-8B-Open-SFT.Q4_K_M.gguf"};

auto LlamaChatEngine::generate(const std::string& prompt) {
    std::string response;

    const int n_prompt_tokens = -llama_tokenize(m_model, prompt.c_str(), prompt.size(), NULL, 0, true, true);
    std::vector<llama_token> prompt_tokens(n_prompt_tokens);
    if (llama_tokenize(m_model, prompt.c_str(), prompt.size(), prompt_tokens.data(), prompt_tokens.size(), llama_get_kv_cache_used_cells(m_ctx) == 0, true) < 0) {
        GGML_ABORT("failed to tokenize the prompt\n");
    }

    llama_batch batch = llama_batch_get_one(prompt_tokens.data(), prompt_tokens.size());
    llama_token new_token_id;
    while (true) {
        int n_ctx = llama_n_ctx(m_ctx);
        int n_ctx_used = llama_get_kv_cache_used_cells(m_ctx);

        if (llama_decode(m_ctx, batch)) {
            GGML_ABORT("failed to decode\n");
        }

        new_token_id = llama_sampler_sample(m_sampler, m_ctx, -1);

        if (llama_token_is_eog(m_model, new_token_id)) {
            break;
        }

        char buf[256];
        int n = llama_token_to_piece(m_model, new_token_id, buf, sizeof(buf), 0, true);
        if (n < 0) {
            GGML_ABORT("failed to convert token to piece\n");
        }
        std::string piece(buf, n);
        printf("%s", piece.c_str());
        fflush(stdout);
        response += piece;

        batch = llama_batch_get_one(&new_token_id, 1);
    }

    return response;
}

void LlamaChatEngine::handle_new_user_input() {

    std::vector<llama_chat_message> messages;

    if (m_user_input.empty()) {
        return;
    }

    std::vector<char> formatted(llama_n_ctx(m_ctx));
    static int prev_len {0};

    messages.push_back({"user", strdup(m_user_input.c_str())});
    int new_len = llama_chat_apply_template(m_model, nullptr, messages.data(), messages.size(), true, formatted.data(), formatted.size());
    if (new_len > (int)formatted.size()) {
        formatted.resize(new_len);
        new_len = llama_chat_apply_template(m_model, nullptr, messages.data(), messages.size(), true, formatted.data(), formatted.size());
    }
    if (new_len < 0) {
        fprintf(stderr, "failed to apply the chat template\n");
        return;
    }

    std::string prompt(formatted.begin() + prev_len, formatted.begin() + new_len);
    std::string response = generate(prompt);

    messages.push_back({"assistant", strdup(response.c_str())});
    prev_len = llama_chat_apply_template(m_model, nullptr, messages.data(), messages.size(), false, nullptr, 0);

    m_messages.append(messages);

    if (prev_len < 0) {
        fprintf(stderr, "failed to apply the chat template\n");
        return;
    }
}

LlamaChatEngine::LlamaChatEngine(QObject *parent)
    : QObject(parent), m_messages(this) {

    connect(this, &LlamaChatEngine::user_inputChanged, this, &LlamaChatEngine::handle_new_user_input);

    // load dynamic backends
    ggml_backend_load_all();

    m_model_params = llama_model_default_params();
    m_model_params.n_gpu_layers = m_ngl;

    m_model = llama_load_model_from_file(m_model_path.c_str(), m_model_params);
    if (!m_model) {
        fprintf(stderr, "%s: error: unable to load model\n", __func__);
        return;
    }

    m_ctx_params = llama_context_default_params();
    m_ctx_params.n_ctx = m_n_ctx;
    m_ctx_params.n_batch = m_n_ctx;

    m_ctx = llama_new_context_with_model(m_model, m_ctx_params);
    if (!m_ctx) {
        fprintf(stderr, "%s: error: failed to create the llama_context\n", __func__);
        return;
    }

    m_sampler = llama_sampler_chain_init(llama_sampler_chain_default_params());
    llama_sampler_chain_add(m_sampler, llama_sampler_init_min_p(0.05f, 1));
    llama_sampler_chain_add(m_sampler, llama_sampler_init_temp(0.8f));
    llama_sampler_chain_add(m_sampler, llama_sampler_init_dist(LLAMA_DEFAULT_SEED));
}

LlamaChatEngine::~LlamaChatEngine() {
    llama_sampler_free(m_sampler);
    llama_free(m_ctx);
    llama_free_model(m_model);
}

std::string LlamaChatEngine::user_input() const
{
    return m_user_input;
}

void LlamaChatEngine::setUser_input(const std::string &newUser_input)
{
    if (m_user_input == newUser_input)
        return;
    m_user_input = newUser_input;
    emit user_inputChanged();
}

void LlamaChatEngine::resetUser_input()
{
    setUser_input({}); // TODO: Adapt to use your actual default value
}

ChatMessageModel* LlamaChatEngine::messages()
{
    return &m_messages;
}

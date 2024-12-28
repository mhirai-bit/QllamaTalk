#include <QThread>
#include "llamachatengine.h"

const std::string LlamaChatEngine::m_model_path {"/Users/mainuser/Documents/Projects/QllamaTalk/3rdparty/llama.cpp/models/Llama-3.1-8B-Open-SFT.Q4_K_M.gguf"};

void LlamaChatEngine::generate(const std::string& prompt, std::string& response) {

}

void LlamaChatEngine::handle_new_user_input() {

    static std::vector<llama_chat_message> messages;
    static std::vector<char> formatted(llama_n_ctx(m_ctx));
    static int prev_len {0};

    std::string user_input = m_user_input.toStdString();

    if (user_input.empty()) {
        return;
    }

    std::vector<llama_chat_message> user_input_message;
    user_input_message.push_back({"user", strdup(user_input.c_str())});
    messages.push_back({"user", strdup(user_input.c_str())});
    m_messages.append(user_input_message);

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
    emit requestGeneration(prompt.c_str());

    prev_len = llama_chat_apply_template(m_model, nullptr, messages.data(), messages.size(), false, nullptr, 0);

    if (prev_len < 0) {
        fprintf(stderr, "failed to apply the chat template\n");
        return;
    }
}

void LlamaChatEngine::onPartialResponse(const QString &textSoFar)
{
    if (!m_inProgress) {
        m_currentAssistantIndex = m_messages.appendSingle("assistant", textSoFar);
        m_inProgress = true;
    } else {
        m_messages.updateMessageContent(m_currentAssistantIndex, textSoFar);
    }
}

void LlamaChatEngine::onGenerationFinished(const QString &finalResponse)
{
    if (m_inProgress) {
        m_messages.updateMessageContent(m_currentAssistantIndex, finalResponse);
        m_inProgress = false;
        m_currentAssistantIndex = -1;
    }
}

LlamaChatEngine::LlamaChatEngine(QObject *parent)
    : QObject(parent), m_messages(this) {

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

    QThread* workerThread = new QThread(this);
    m_response_generator = new LlamaResponseGenerator(this, m_model, m_ctx);
    m_response_generator->moveToThread(workerThread);

    workerThread->start();

    connect(this, &LlamaChatEngine::user_inputChanged, this, &LlamaChatEngine::handle_new_user_input);
    connect(this, &LlamaChatEngine::requestGeneration, m_response_generator, &LlamaResponseGenerator::generate);
    connect(m_response_generator, &LlamaResponseGenerator::partialResponseReady, this, &LlamaChatEngine::onPartialResponse);
    connect(m_response_generator, &LlamaResponseGenerator::generationFinished, this, &LlamaChatEngine::onGenerationFinished);
}

LlamaChatEngine::~LlamaChatEngine() {
    llama_free(m_ctx);
    llama_free_model(m_model);
}

QString LlamaChatEngine::user_input() const
{
    return m_user_input;
}

void LlamaChatEngine::setUser_input(const QString &newUser_input)
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

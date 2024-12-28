#include <QThread>
#include "llamachatengine.h"

// The path to the LLaMA model we load by default.
const std::string LlamaChatEngine::m_model_path {
    "Llama-3.1-8B-Open-SFT.Q4_K_M.gguf"
};

// This function is kept as a placeholder for potential synchronous generation usage.
void LlamaChatEngine::generate(const std::string &prompt, std::string &response) {
    // Currently not used. Could be implemented for blocking generation if necessary.
}

// Handles user input changes, updates the model with the user message,
// and prepares a prompt for generation.
void LlamaChatEngine::handle_new_user_input() {
    // These static variables are used to preserve state across multiple calls.
    static std::vector<llama_chat_message> messages;
    static std::vector<char> formatted(llama_n_ctx(m_ctx));
    static int prev_len {0};

    std::string user_input = m_user_input.toStdString();
    if (user_input.empty()) {
        return;
    }

    // Add the new user message to both the local vector and the QML-exposed model.
    std::vector<llama_chat_message> user_input_message;
    user_input_message.push_back({"user", strdup(user_input.c_str())});
    messages.push_back({"user", strdup(user_input.c_str())});
    m_messages.append(user_input_message);

    // Apply the chat template with the current messages.
    int new_len = llama_chat_apply_template(m_model,
                                            nullptr, // no custom template string
                                            messages.data(),
                                            messages.size(),
                                            true,
                                            formatted.data(),
                                            formatted.size());
    if (new_len > static_cast<int>(formatted.size())) {
        formatted.resize(new_len);
        new_len = llama_chat_apply_template(m_model,
                                            nullptr,
                                            messages.data(),
                                            messages.size(),
                                            true,
                                            formatted.data(),
                                            formatted.size());
    }
    if (new_len < 0) {
        fprintf(stderr, "failed to apply the chat template\n");
        return;
    }

    // Extract the newly applied portion of the prompt.
    std::string prompt(formatted.begin() + prev_len, formatted.begin() + new_len);
    emit requestGeneration(prompt.c_str());

    // Prepare for subsequent calls to append the next chunk.
    prev_len = llama_chat_apply_template(m_model,
                                         nullptr,
                                         messages.data(),
                                         messages.size(),
                                         false,
                                         nullptr,
                                         0);
    if (prev_len < 0) {
        fprintf(stderr, "failed to apply the chat template\n");
        return;
    }
}

// Receives partial AI responses, either creating a new assistant message or
// updating the existing one if we're in progress.
void LlamaChatEngine::onPartialResponse(const QString &textSoFar) {
    if (!m_inProgress) {
        m_currentAssistantIndex = m_messages.appendSingle("assistant", textSoFar);
        m_inProgress = true;
    } else {
        m_messages.updateMessageContent(m_currentAssistantIndex, textSoFar);
    }
}

// Receives the final AI response, updates the message, and resets generation state.
void LlamaChatEngine::onGenerationFinished(const QString &finalResponse) {
    if (m_inProgress) {
        m_messages.updateMessageContent(m_currentAssistantIndex, finalResponse);
        m_inProgress = false;
        m_currentAssistantIndex = -1;
    }
}

// Constructor loads the model and context, then starts a worker thread for generation.
LlamaChatEngine::LlamaChatEngine(QObject *parent)
    : QObject(parent), m_messages(this) {

    // Load ggml backends (e.g. CPU, Metal).
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

    // Set up a dedicated QThread for LlamaResponseGenerator.
    QThread* workerThread = new QThread(this);
    m_response_generator = new LlamaResponseGenerator(nullptr, m_model, m_ctx);
    m_response_generator->moveToThread(workerThread);

    workerThread->start();

    // Connect input and generation signals/slots.
    connect(this, &LlamaChatEngine::user_inputChanged,
            this, &LlamaChatEngine::handle_new_user_input);

    connect(this, &LlamaChatEngine::requestGeneration,
            m_response_generator, &LlamaResponseGenerator::generate);

    connect(m_response_generator, &LlamaResponseGenerator::partialResponseReady,
            this, &LlamaChatEngine::onPartialResponse);

    connect(m_response_generator, &LlamaResponseGenerator::generationFinished,
            this, &LlamaChatEngine::onGenerationFinished);

    // Clean up the worker objects when the thread finishes.
    connect(workerThread, &QThread::finished,
            m_response_generator, &QObject::deleteLater);
}

// Destructor frees the LLaMA context and model.
LlamaChatEngine::~LlamaChatEngine() {
    llama_free(m_ctx);
    llama_free_model(m_model);
}

// Q_PROPERTY getters/setters and basic user_input handling.
QString LlamaChatEngine::user_input() const {
    return m_user_input;
}

void LlamaChatEngine::setUser_input(const QString &newUser_input) {
    if (m_user_input == newUser_input) {
        return;
    }
    m_user_input = newUser_input;
    emit user_inputChanged();
}

void LlamaChatEngine::resetUser_input() {
    setUser_input({});
}

// Exposes the ChatMessageModel to QML.
ChatMessageModel* LlamaChatEngine::messages() {
    return &m_messages;
}

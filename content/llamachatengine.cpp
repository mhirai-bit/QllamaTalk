#include <QThread>
#include <QtConcurrent>
#include "llamachatengine.h"

// Loads the default LLaMA model path (defined via CMake)
// 将来的に同期的なテキスト生成を行う可能性があるためのプレースホルダ関数
const std::string LlamaChatEngine::m_model_path {
#ifdef LLAMA_MODEL_FILE
    LLAMA_MODEL_FILE
#else
#error "LLAMA_MODEL_FILE is not defined. Please define it via target_compile_definitions() in CMake."
#endif
};

// Placeholder for potential synchronous generation in the future
// 将来的に同期的なテキスト生成を行う可能性があるためのプレースホルダ関数
void LlamaChatEngine::generate(const std::string &prompt, std::string &response) {
    // Not used currently
    // 現在は使用していません
}

// Asynchronous engine initialization: loads backends, model, and context
// 非同期のエンジン初期化処理: バックエンドやモデル、コンテキストをロード
void LlamaChatEngine::doEngineInit()
{
    // Load ggml backends (CPU, Metal, etc.)
    // ggml のバックエンドを読み込み（CPU, Metal など）
    ggml_backend_load_all();

    m_model_params = llama_model_default_params();
    m_model_params.n_gpu_layers = m_ngl;

    m_model = llama_load_model_from_file(m_model_path.c_str(), m_model_params);
    if (!m_model) {
        fprintf(stderr, "%s: error: unable to load model\n", __func__);
        return;
    }

    m_ctx_params = llama_context_default_params();
    m_ctx_params.n_ctx   = m_n_ctx;
    m_ctx_params.n_batch = m_n_ctx;

    m_ctx = llama_new_context_with_model(m_model, m_ctx_params);
    if (!m_ctx) {
        fprintf(stderr, "%s: error: failed to create the llama_context\n", __func__);
        return;
    }

    // Once initialization is done, notify UI thread
    // 初期化完了後、UIスレッドに通知
    QMetaObject::invokeMethod(this, [this] {
        onEngineInitFinished();
    }, Qt::QueuedConnection);
}

// Handles user input: appends user message to model, applies template, then requests generation
// ユーザー入力を処理: ユーザーメッセージをモデルに追加し、テンプレートを適用して生成をリクエスト
void LlamaChatEngine::handle_new_user_input() {
    if(m_inProgress) {
        qDebug() << "Returning because generation is in progress";
        return;
    }

    // Static vars: hold conversation state across multiple calls
    // 静的変数: 複数回の呼び出し間で会話状態を保持
    static std::vector<llama_chat_message> messages;
    static std::vector<char> formatted(llama_n_ctx(m_ctx));
    static int prev_len {0};

    std::string user_input = m_user_input.toStdString();
    if (user_input.empty()) {
        return;
    }

    // Add user message to both local vector and QML model
    // ユーザーメッセージをローカルのベクタとQMLモデル両方に追加
    std::vector<llama_chat_message> user_input_message;
    user_input_message.push_back({"user", strdup(user_input.c_str())});
    messages.push_back({"user", strdup(user_input.c_str())});
    m_messages.append(user_input_message);

    // Apply chat template
    // チャットテンプレートを適用
    int new_len = llama_chat_apply_template(m_model,
                                            nullptr,
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

    // Extract newly added text as prompt
    // 新たに追加された部分を取り出してプロンプトとする
    std::string prompt(formatted.begin() + prev_len, formatted.begin() + new_len);
    emit requestGeneration(QString::fromStdString(prompt));

    // Update prev_len for next chunk
    // 次のチャンクに備えて prev_len を更新
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

// Partially received AI response: create assistant message if first chunk, or update existing
// 部分的に受け取ったAI応答: 最初のチャンクはメッセージ作成、それ以降は更新
void LlamaChatEngine::onPartialResponse(const QString &textSoFar) {
    if (!m_inProgress) {
        m_currentAssistantIndex = m_messages.appendSingle("assistant", textSoFar);
        m_inProgress = true;
    } else {
        m_messages.updateMessageContent(m_currentAssistantIndex, textSoFar);
    }
}

// Final AI response: update message content, reset generation state
// AI応答が完了: メッセージ内容を更新し、生成状態をリセット
void LlamaChatEngine::onGenerationFinished(const QString &finalResponse) {
    if (m_inProgress) {
        m_messages.updateMessageContent(m_currentAssistantIndex, finalResponse);
        m_inProgress = false;
        m_currentAssistantIndex = -1;
    }
}

// Called in UI thread once engine init is done. Sets up worker thread and marks engine as initialized
// エンジン初期化完了後にUIスレッドで呼ばれる。ワーカースレッドをセットアップして初期化済みフラグを立てる
void LlamaChatEngine::onEngineInitFinished()
{
    QThread* workerThread = new QThread(this);
    m_response_generator = new LlamaResponseGenerator(nullptr, m_model, m_ctx);
    m_response_generator->moveToThread(workerThread);

    workerThread->start();

    connect(this, &LlamaChatEngine::user_inputChanged,
            this, &LlamaChatEngine::handle_new_user_input);

    connect(this, &LlamaChatEngine::requestGeneration,
            m_response_generator, &LlamaResponseGenerator::generate);

    connect(m_response_generator, &LlamaResponseGenerator::partialResponseReady,
            this, &LlamaChatEngine::onPartialResponse);

    connect(m_response_generator, &LlamaResponseGenerator::generationFinished,
            this, &LlamaChatEngine::onGenerationFinished);

    connect(workerThread, &QThread::finished,
            m_response_generator, &QObject::deleteLater);

    // Mark engine as initialized
    // エンジンを初期化済みとマーク
    setEngine_initialized(true);
}

// Engine initialization status: read-only in QML
// エンジンの初期化状態: QMLで読み取り専用
bool LlamaChatEngine::engine_initialized() const
{
    return m_engine_initialized;
}

// Private setter for engine_initialized: only we can change it
// engine_initialized のプライベートセッター: このクラス内だけで変更可能
void LlamaChatEngine::setEngine_initialized(bool newEngine_initialized)
{
    if (m_engine_initialized == newEngine_initialized)
        return;
    m_engine_initialized = newEngine_initialized;
    emit engine_initializedChanged();
}

// Constructor: starts asynchronous init in a separate thread
// コンストラクタ: 非同期の初期化を別スレッドで開始
LlamaChatEngine::LlamaChatEngine(QObject *parent)
    : QObject(parent), m_messages(this) {
    QtConcurrent::run(&LlamaChatEngine::doEngineInit, this);
}

// Destructor: frees llama context and model
// デストラクタ: llama のコンテキストとモデルを解放
LlamaChatEngine::~LlamaChatEngine() {
    llama_free(m_ctx);
    llama_free_model(m_model);
}

// Returns user_input
// user_inputを返す
QString LlamaChatEngine::user_input() const {
    return m_user_input;
}

// Sets new user_input, notifies QML
// user_inputを更新し、QMLに通知
void LlamaChatEngine::setUser_input(const QString &newUser_input) {
    if (m_user_input == newUser_input) {
        return;
    }
    m_user_input = newUser_input;
    emit user_inputChanged();
}

// Resets user_input to empty
// user_input を空にリセット
void LlamaChatEngine::resetUser_input() {
    setUser_input({});
}

// Exposes the ChatMessageModel to QML
// ChatMessageModelをQMLに公開
ChatMessageModel* LlamaChatEngine::messages() {
    return &m_messages;
}

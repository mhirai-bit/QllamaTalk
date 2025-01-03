#include <QThread>
#include <QtConcurrent>
#include <QRemoteObjectNode>
#include "llamachatengine.h"
#include "llamaresponsegenerator.h"
#include "rep_LlamaResponseGenerator_replica.h"

// Constructor: starts asynchronous init in another thread
// コンストラクタ: 非同期の初期化を別スレッドで開始
LlamaChatEngine::LlamaChatEngine(QObject *parent)
    : QObject(parent)
    , m_messages(this)
{
    QtConcurrent::run(&LlamaChatEngine::doEngineInit, this);
}

// Switch engine mode
// エンジンモードを切り替える
void LlamaChatEngine::switchEngineMode(EngineMode newMode)
{
    if (newMode == m_currentEngineMode) {
        // Already in this mode, do nothing
        // 既に同じモードなので何もしない
        return;
    }
    if (m_inProgress) {
        // Defer switch until generation finishes
        // 生成が完了するまで切り替えを保留
        m_pendingEngineSwitchMode = newMode;
        return;
    }
    doImmediateEngineSwitch(newMode);
}

// Handles new user input (applies chat template, emits requestGeneration)
// ユーザー入力の処理（チャットテンプレート適用、requestGenerationをemit）
void LlamaChatEngine::handle_new_user_input()
{
    if (m_inProgress) {
        qDebug() << "Generation in progress, ignoring new input.";
        return;
    }

    static std::vector<llama_chat_message> messages;
    static std::vector<char> formatted(llama_n_ctx(m_ctx));
    static int prev_len {0};

    std::string userText = m_user_input.toStdString();
    if (userText.empty()) {
        return;
    }

    // Append user message to local/state
    // ユーザーメッセージをローカルに追加
    std::vector<llama_chat_message> userInputMsg;
    userInputMsg.push_back({"user", strdup(userText.c_str())});
    messages.push_back({"user", strdup(userText.c_str())});
    m_messages.append(userInputMsg);

    // Apply chat template
    // チャットテンプレートを適用
    int new_len = llama_chat_apply_template(
        m_model, nullptr, messages.data(), messages.size(),
        true, formatted.data(), formatted.size()
        );
    if (new_len > static_cast<int>(formatted.size())) {
        formatted.resize(new_len);
        new_len = llama_chat_apply_template(
            m_model, nullptr, messages.data(), messages.size(),
            true, formatted.data(), formatted.size()
            );
    }
    if (new_len < 0) {
        fprintf(stderr, "Failed to apply chat template.\n");
        return;
    }

    // Emit requestGeneration with new prompt segment
    // 新しいプロンプト部分を含めて requestGeneration をemit
    std::string prompt(formatted.begin() + prev_len, formatted.begin() + new_len);
    emit requestGeneration(QString::fromStdString(prompt));

    // Update prev_len
    // prev_len を更新
    prev_len = llama_chat_apply_template(
        m_model, nullptr, messages.data(), messages.size(),
        false, nullptr, 0
        );
    if (prev_len < 0) {
        fprintf(stderr, "Failed to apply chat template.\n");
    }
}

// Receives partial AI response (first chunk -> new message, otherwise update)
// 部分的なAI応答を受け取る（最初のチャンクならメッセージ新規、以降は更新）
void LlamaChatEngine::onPartialResponse(const QString &textSoFar)
{
    if (!m_inProgress) {
        m_currentAssistantIndex = m_messages.appendSingle("assistant", textSoFar);
        m_inProgress = true;
    } else {
        m_messages.updateMessageContent(m_currentAssistantIndex, textSoFar);
    }
}

// Called after AI generation finishes (resets state, checks pending mode switch)
// AI生成完了後に呼ばれる（状態リセット、保留中の切り替えがあれば処理）
void LlamaChatEngine::onGenerationFinished(const QString &finalResponse)
{
    if (m_inProgress) {
        m_messages.updateMessageContent(m_currentAssistantIndex, finalResponse);
        m_inProgress = false;
        m_currentAssistantIndex = -1;
    }

    // If pending engine switch, do it now
    // 保留中のエンジン切り替えがあれば実行
    if (m_pendingEngineSwitchMode.has_value()) {
        EngineMode pendingMode = *m_pendingEngineSwitchMode;
        m_pendingEngineSwitchMode.reset();
        doImmediateEngineSwitch(pendingMode);
    }
}

// Called on UI thread after engine init (prepare local/remote engines, default local)
// エンジン初期化完了後にUIスレッドで呼ばれる（ローカル/リモートエンジン用意、デフォルトはローカル）
void LlamaChatEngine::onEngineInitFinished()
{
    // Prepare local engine
    m_localGenerator = new LlamaResponseGenerator(nullptr, m_model, m_ctx);
    m_localWorkerThread = new QThread(this);
    m_localGenerator->moveToThread(m_localWorkerThread);
    connect(m_localWorkerThread, &QThread::finished, m_localGenerator, &QObject::deleteLater);
    m_localWorkerThread->start();

    // Prepare remote engine
    QRemoteObjectNode *node = new QRemoteObjectNode(this);
    node->connectToNode(QUrl("local:myRemoteEngine")); // Example
    m_remoteGenerator = node->acquire<LlamaResponseGeneratorReplica>();
    if (!m_remoteGenerator) {
        // Handle error
        // エラー処理
    }
    m_remoteGenerator->setParent(this);

    connect(this, &LlamaChatEngine::user_inputChanged,
            this, &LlamaChatEngine::handle_new_user_input);

    // Default to local
    // デフォルトをローカルに
    m_currentEngineMode = Mode_Local;
    doImmediateEngineSwitch(m_currentEngineMode);

    setEngine_initialized(true);
}

// Immediately switch local/remote engines (disconnect old, connect new)
// ローカル/リモートを即切り替え（古い接続を切り、新しいエンジンに接続）
void LlamaChatEngine::doImmediateEngineSwitch(EngineMode newMode)
{
    if (newMode == m_currentEngineMode) {
        qDebug() << "Already in this mode:" << newMode << ", skipping.";
        return;
    }

    // Disconnect old engine
    // 古いエンジンとの接続を切断
    disconnect(this, &LlamaChatEngine::requestGeneration, nullptr, nullptr);

    if (newMode == Mode_Local) {
        // Disconnect remote signals
        disconnect(m_remoteGenerator, &LlamaResponseGeneratorReplica::partialResponseReady,
                   this, &LlamaChatEngine::onPartialResponse);
        disconnect(m_remoteGenerator, &LlamaResponseGeneratorReplica::generationFinished,
                   this, &LlamaChatEngine::onGenerationFinished);

        // Connect local signals
        connect(this, &LlamaChatEngine::requestGeneration,
                m_localGenerator, [this](const QString &prompt) {
                    QMetaObject::invokeMethod(m_localGenerator, "generate",
                                              Qt::QueuedConnection,
                                              Q_ARG(QString, prompt));
                });
        connect(m_localGenerator, &LlamaResponseGenerator::partialResponseReady,
                this, &LlamaChatEngine::onPartialResponse);
        connect(m_localGenerator, &LlamaResponseGenerator::generationFinished,
                this, &LlamaChatEngine::onGenerationFinished);

        qDebug() << "[EngineSwitch] Now using LOCAL engine.";
    } else {
        // Disconnect local signals
        disconnect(m_localGenerator, &LlamaResponseGenerator::partialResponseReady,
                   this, &LlamaChatEngine::onPartialResponse);
        disconnect(m_localGenerator, &LlamaResponseGenerator::generationFinished,
                   this, &LlamaChatEngine::onGenerationFinished);

        // Connect remote signals
        connect(this, &LlamaChatEngine::requestGeneration,
                m_remoteGenerator, &LlamaResponseGeneratorReplica::generate);
        connect(m_remoteGenerator, &LlamaResponseGeneratorReplica::partialResponseReady,
                this, &LlamaChatEngine::onPartialResponse);
        connect(m_remoteGenerator, &LlamaResponseGeneratorReplica::generationFinished,
                this, &LlamaChatEngine::onGenerationFinished);

        qDebug() << "[EngineSwitch] Now using REMOTE engine.";
    }

    m_currentEngineMode = newMode;
    qDebug() << "[EngineSwitch] doImmediateEngineSwitch -> newMode =" << newMode;
}

// Runs in another thread, loads ggml backends and llama model/context
// 別スレッドで実行、ggmlのバックエンドやllamaのモデル/コンテキストをロード
void LlamaChatEngine::doEngineInit()
{
    ggml_backend_load_all();

    m_model_params = llama_model_default_params();
    m_model_params.n_gpu_layers = m_ngl;

    m_model = llama_load_model_from_file(m_model_path.c_str(), m_model_params);
    if (!m_model) {
        fprintf(stderr, "Error: unable to load model.\n");
        return;
    }

    m_ctx_params = llama_context_default_params();
    m_ctx_params.n_ctx = m_n_ctx;
    m_ctx_params.n_batch = m_n_ctx;

    m_ctx = llama_new_context_with_model(m_model, m_ctx_params);
    if (!m_ctx) {
        fprintf(stderr, "Error: failed to create llama_context.\n");
        return;
    }

    QMetaObject::invokeMethod(this, [this] {
        onEngineInitFinished();
    }, Qt::QueuedConnection);
}

// Check if engine is initialized
// エンジンが初期化完了か判定
bool LlamaChatEngine::engine_initialized() const
{
    return m_engine_initialized;
}

// Private setter for engine_initialized
// engine_initializedフラグをプライベートにセット
void LlamaChatEngine::setEngine_initialized(bool newEngine_initialized)
{
    if (m_engine_initialized == newEngine_initialized)
        return;
    m_engine_initialized = newEngine_initialized;
    emit engine_initializedChanged();
}

// Destructor: free llama context/model
// デストラクタ: llamaのコンテキスト/モデルを解放
LlamaChatEngine::~LlamaChatEngine()
{
    llama_free(m_ctx);
    llama_free_model(m_model);
}

// Getter for user_input
// user_inputのゲッター
QString LlamaChatEngine::user_input() const
{
    return m_user_input;
}

// Setter for user_input
// user_inputのセッター
void LlamaChatEngine::setUser_input(const QString &newUser_input)
{
    if (m_user_input == newUser_input) {
        return;
    }
    m_user_input = newUser_input;
    emit user_inputChanged();
}

// Reset user_input to empty
// user_inputを空にリセット
void LlamaChatEngine::resetUser_input()
{
    setUser_input({});
}

// Return ChatMessageModel
// ChatMessageModelを返す
ChatMessageModel* LlamaChatEngine::messages()
{
    return &m_messages;
}

// Default model path defined via CMake
// デフォルトモデルパス（CMakeで定義）
const std::string LlamaChatEngine::m_model_path {
#ifdef LLAMA_MODEL_FILE
    LLAMA_MODEL_FILE
#else
#error "LLAMA_MODEL_FILE is not defined. Please define it via target_compile_definitions() in CMake."
#endif
};

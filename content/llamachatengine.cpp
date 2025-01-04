#include <QThread>
#include <QtConcurrent>
#include <QRemoteObjectNode>
#include <QtSystemDetection>
#include "llamachatengine.h"
#include "llamaresponsegenerator.h"
#include "rep_LlamaResponseGenerator_replica.h"

// Constructor: begins async initialization in another thread
// コンストラクタ: 非同期の初期化を別スレッドで開始
LlamaChatEngine::LlamaChatEngine(QObject *parent)
    : QObject(parent)
    , m_messages(this)
{
    QtConcurrent::run(&LlamaChatEngine::doEngineInit, this);
}

// Destructor: frees llama context/model
// デストラクタ: llamaコンテキスト/モデルを解放
LlamaChatEngine::~LlamaChatEngine()
{
    llama_free(m_ctx);
    llama_free_model(m_model);
}

// Switch engine mode (local <-> remote)
// エンジンモードを切り替える (ローカル <-> リモート)
void LlamaChatEngine::switchEngineMode(EngineMode newMode)
{
    if (newMode == m_currentEngineMode) {
        // No need to switch if already in that mode
        // 既に同じモードなら切り替え不要
        return;
    }
    if (m_inProgress) {
        // Defer switch until current generation finishes
        // 推論中なら、完了後に切り替える
        m_pendingEngineSwitchMode = newMode;
        return;
    }
    doImmediateEngineSwitch(newMode);
}

// Handle new user input: accumulate message, emit requestGeneration
// ユーザー入力を処理: メッセージを追加し、requestGenerationをemit
void LlamaChatEngine::handle_new_user_input()
{
    if (m_inProgress) {
        qDebug() << "Generation in progress, ignoring new input.";
        return;
    }

    static QList<LlamaChatMessage> messages;

    if (m_user_input.isEmpty()) {
        return;
    }

    LlamaChatMessage msg;
    msg.setRole(QStringLiteral("user"));
    msg.setContent(m_user_input);

    // Add the user's message both locally and in QML
    // ユーザーメッセージをローカル/ QMLに追加
    messages.append(msg);
    m_messages.appendSingle("user", msg.content());

    emit requestGeneration(messages);
}

// Receive partial AI response
// 部分的なAI応答を受け取る
void LlamaChatEngine::onPartialResponse(const QString &textSoFar)
{
    if (!m_inProgress) {
        m_currentAssistantIndex = m_messages.appendSingle("assistant", textSoFar);
        m_inProgress = true;
    } else {
        m_messages.updateMessageContent(m_currentAssistantIndex, textSoFar);
    }
}

// Receive final AI response, reset state, check pending switch
// 最終AI応答を受け取り、状態リセット＆切り替え保留があれば対応
void LlamaChatEngine::onGenerationFinished(const QString &finalResponse)
{
    if (m_inProgress) {
        m_messages.updateMessageContent(m_currentAssistantIndex, finalResponse);
        m_inProgress = false;
        m_currentAssistantIndex = -1;
    }

    if (m_pendingEngineSwitchMode.has_value()) {
        EngineMode pendingMode = *m_pendingEngineSwitchMode;
        m_pendingEngineSwitchMode.reset();
        doImmediateEngineSwitch(pendingMode);
    }
}

void LlamaChatEngine::configureRemoteObjects()
{
    qDebug() << "Connecting to remote node...";
    qDebug() << "ip_address:" << m_ip_address;
    qDebug() << "port_number:" << m_port_number;
    m_remoteMode = new QRemoteObjectNode(this);
    bool result = m_remoteMode->connectToNode(QUrl(QStringLiteral("tcp://%1:%2").arg(m_ip_address).arg(m_port_number)));

    if(m_ip_address.isEmpty() || m_port_number == 0) {
        // QRemoteObjectNode::connectToNode() seens to be returning true even when it fails to resolve the IP address
        qDebug() << "IP address and/or port number not set.";
        return;
    }

    if(result) {
        qDebug() << "Connected to remote node.";
        m_remoteGenerator = m_remoteMode->acquire<LlamaResponseGeneratorReplica>();
        if (!m_remoteGenerator) {
            // handle error
            qDebug() << "Failed to acquire remote generator.";
        }
        m_remoteGenerator->setParent(this);
    } else {
        qDebug() << "Failed to connect to remote node.";
    }
}

void LlamaChatEngine::updateRemoteInitializationStatus()
{
    if(!m_remoteGenerator) {
        qDebug() << "No remote generator available.";
        return;
    }

    qDebug() << "m_remoteGenerator->remoteInitialized():" << m_remoteGenerator->remoteInitialized();
    if (m_remoteGenerator->remoteInitialized()) {
        qDebug() << "Remote engine initialized.";
        setRemote_initialzied(true);
    } else {
        qDebug() << "Remote engine not initialized.";
        setRemote_initialzied(false);

        // When remote engine signals initialization, check if local is also ready
        // リモートエンジン初期化を受け取り、ローカルもOKならエンジン全体を初期化済みに
        connect(m_remoteGenerator, &LlamaResponseGeneratorReplica::remoteInitializedChanged,
                this, [this]() {
                    setRemote_initialzied(m_remoteGenerator->remoteInitialized());
                });
    }
}

// Called on UI thread after doEngineInit() completes
// doEngineInit()完了後、UIスレッドで呼ばれる
void LlamaChatEngine::onEngineInitFinished()
{
    // Prepare local engine
    m_localGenerator = new LlamaResponseGenerator(nullptr, m_model, m_ctx);
    m_localWorkerThread = new QThread(this);
    m_localGenerator->moveToThread(m_localWorkerThread);
    connect(m_localWorkerThread, &QThread::finished,
            m_localGenerator, &QObject::deleteLater);
    m_localWorkerThread->start();

    connect(this, &LlamaChatEngine::user_inputChanged,
            this, &LlamaChatEngine::handle_new_user_input);

    setLocal_initialized(true);

    configureRemoteObjects();
    // Default: local engine
    doImmediateEngineSwitch(Mode_Local);
}

// Immediately switch local/remote engine connections
// ローカル/リモートエンジンの接続を即時切り替え
void LlamaChatEngine::doImmediateEngineSwitch(EngineMode newMode)
{
    // Disconnect old connections
    disconnect(this, &LlamaChatEngine::requestGeneration, nullptr, nullptr);

    if (newMode == Mode_Local) {
        configureLocalSignalSlots();
    } else {
        if (!m_remoteGenerator) {
            configureRemoteObjects();
        }
        configureRemoteSignalSlots();
        updateRemoteInitializationStatus();
    }

    setCurrentEngineMode(newMode);
    qDebug() << "[EngineSwitch] doImmediateEngineSwitch -> newMode =" << newMode;
}

void LlamaChatEngine::configureRemoteSignalSlots()
{
    // Disconnect local signals
    disconnect(m_localGenerator, &LlamaResponseGenerator::partialResponseReady,
               this, &LlamaChatEngine::onPartialResponse);
    disconnect(m_localGenerator, &LlamaResponseGenerator::generationFinished,
               this, &LlamaChatEngine::onGenerationFinished);

    // Connect remote
    connect(this, &LlamaChatEngine::requestGeneration,
            m_remoteGenerator, &LlamaResponseGeneratorReplica::generate);
    connect(m_remoteGenerator, &LlamaResponseGeneratorReplica::partialResponseReady,
            this, &LlamaChatEngine::onPartialResponse);
    connect(m_remoteGenerator, &LlamaResponseGeneratorReplica::generationFinished,
            this, &LlamaChatEngine::onGenerationFinished);

    qDebug() << "[EngineSwitch] Now using REMOTE engine.";
}

void LlamaChatEngine::configureLocalSignalSlots()
{
    if(m_remoteGenerator) {
        // Disconnect remote signals
        disconnect(m_remoteGenerator, &LlamaResponseGeneratorReplica::partialResponseReady,
                   this, &LlamaChatEngine::onPartialResponse);
        disconnect(m_remoteGenerator, &LlamaResponseGeneratorReplica::generationFinished,
                   this, &LlamaChatEngine::onGenerationFinished);
    }

    // Connect local
    connect(this, &LlamaChatEngine::requestGeneration,
            m_localGenerator, [this](const QList<LlamaChatMessage>& messages) {
                QMetaObject::invokeMethod(m_localGenerator,
                                          "generate",
                                          Qt::QueuedConnection,
                                          Q_ARG(const QList<LlamaChatMessage>&, messages));
            });
    connect(m_localGenerator, &LlamaResponseGenerator::partialResponseReady,
            this, &LlamaChatEngine::onPartialResponse);
    connect(m_localGenerator, &LlamaResponseGenerator::generationFinished,
            this, &LlamaChatEngine::onGenerationFinished);

    qDebug() << "[EngineSwitch] Now using LOCAL engine.";
}

bool LlamaChatEngine::remote_initialzied() const
{
    return m_remote_initialzied;
}

void LlamaChatEngine::setRemote_initialzied(bool newRemote_initialzied)
{
    if (m_remote_initialzied == newRemote_initialzied)
        return;
    m_remote_initialzied = newRemote_initialzied;
    emit remote_initialziedChanged();
}

bool LlamaChatEngine::local_initialized() const
{
    return m_local_initialized;
}

void LlamaChatEngine::setLocal_initialized(bool newLocal_initialized)
{
    if (m_local_initialized == newLocal_initialized)
        return;
    m_local_initialized = newLocal_initialized;
    emit local_initializedChanged();
}

int LlamaChatEngine::port_number() const
{
    return m_port_number;
}

void LlamaChatEngine::setPort_number(int newPort_number)
{
    if (m_port_number == newPort_number)
        return;
    m_port_number = newPort_number;
    emit port_numberChanged();
}

QString LlamaChatEngine::ip_address() const
{
    return m_ip_address;
}

void LlamaChatEngine::setIp_address(const QString &newIp_address)
{
    if (m_ip_address == newIp_address)
        return;
    m_ip_address = newIp_address;
    emit ip_addressChanged();
}

LlamaChatEngine::EngineMode LlamaChatEngine::currentEngineMode() const
{
    return m_currentEngineMode;
}

void LlamaChatEngine::setCurrentEngineMode(EngineMode newCurrentEngineMode)
{
    if (m_currentEngineMode == newCurrentEngineMode)
        return;
    m_currentEngineMode = newCurrentEngineMode;
    emit currentEngineModeChanged();
}

// Runs in another thread to load ggml backends, llama model/context
// 別スレッドで実行し、ggmlのバックエンドやllamaモデル/コンテキストを読み込み
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
    m_ctx_params.n_ctx   = m_n_ctx;
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

// Return the ChatMessageModel
// ChatMessageModelを返す
ChatMessageModel* LlamaChatEngine::messages()
{
    return &m_messages;
}

// Default LLaMA model path (defined via CMake)
// CMakeで定義されたLLaMAモデルのデフォルトパス
const std::string LlamaChatEngine::m_model_path {
#ifdef LLAMA_MODEL_FILE
    LLAMA_MODEL_FILE
#else
#error "LLAMA_MODEL_FILE is not defined. Please define it via target_compile_definitions() in CMake."
#endif
};

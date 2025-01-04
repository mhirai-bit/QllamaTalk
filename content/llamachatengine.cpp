#include <QThread>
#include <QtConcurrent>
#include <QRemoteObjectNode>
#include <QtSystemDetection>
#include "llamachatengine.h"
#include "llamaresponsegenerator.h"
#include "rep_LlamaResponseGenerator_replica.h"

//------------------------------------------------------------------------------
// Constructor: create and start async init
// コンストラクタ: 非同期の初期化を開始
//------------------------------------------------------------------------------
LlamaChatEngine::LlamaChatEngine(QObject *parent)
    : QObject(parent)
    , m_messages(this)
{
    qSetMessagePattern("[%{file}:%{line}] %{message}");
    QtConcurrent::run(&LlamaChatEngine::doEngineInit, this);
}

//------------------------------------------------------------------------------
// Destructor: free llama context/model
// デストラクタ: llamaのコンテキスト/モデルを解放
//------------------------------------------------------------------------------
LlamaChatEngine::~LlamaChatEngine()
{
    llama_free(m_ctx);
    llama_free_model(m_model);
}

//------------------------------------------------------------------------------
// switchEngineMode: switches between local or remote
// エンジンモードをローカル/リモートに切り替え
//------------------------------------------------------------------------------
void LlamaChatEngine::switchEngineMode(EngineMode new_mode)
{
    if (new_mode == m_current_engine_mode) {
        // Already in this mode
        // 既に同じモード
        return;
    }

    if (m_in_progress) {
        // Defer if generation is ongoing
        // 推論中なら完了後に切り替え
        m_pending_engine_switch_mode = new_mode;
        return;
    }

    doImmediateEngineSwitch(new_mode);
}

//------------------------------------------------------------------------------
// handle_new_user_input: process user input, emit generation request
// ユーザー入力を処理して推論リクエストをemit
//------------------------------------------------------------------------------
void LlamaChatEngine::handle_new_user_input()
{
    if (m_in_progress) {
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

    // Store in local and QML
    // ローカル/QMLにメッセージを追加
    messages.append(msg);
    m_messages.appendSingle("user", msg.content());

    emit requestGeneration(messages);
}

//------------------------------------------------------------------------------
// onPartialResponse: update UI with partial text
// 部分的なテキストをUIに更新
//------------------------------------------------------------------------------
void LlamaChatEngine::onPartialResponse(const QString &text_so_far)
{
    if (!m_in_progress) {
        m_current_assistant_index = m_messages.appendSingle("assistant", text_so_far);
        m_in_progress = true;
    } else {
        m_messages.updateMessageContent(m_current_assistant_index, text_so_far);
    }
}

//------------------------------------------------------------------------------
// onGenerationFinished: finalize after generation
// 推論完了後の後処理
//------------------------------------------------------------------------------
void LlamaChatEngine::onGenerationFinished(const QString &final_response)
{
    if (m_in_progress) {
        m_messages.updateMessageContent(m_current_assistant_index, final_response);
        m_in_progress = false;
        m_current_assistant_index = -1;
    }

    // If user switched mode mid-inference
    // 推論中にモード変更があれば実行
    if (m_pending_engine_switch_mode.has_value()) {
        EngineMode pending_mode = *m_pending_engine_switch_mode;
        m_pending_engine_switch_mode.reset();
        doImmediateEngineSwitch(pending_mode);
    }
}

//------------------------------------------------------------------------------
// onEngineInitFinished: set up local engine, connect signals, default to local
// エンジン初期化完了後: ローカルエンジン等をセットアップ, ローカルをデフォルトに
//------------------------------------------------------------------------------
void LlamaChatEngine::onEngineInitFinished()
{
    m_local_generator = new LlamaResponseGenerator(nullptr, m_model, m_ctx);
    m_local_worker_thread = new QThread(this);

    m_local_generator->moveToThread(m_local_worker_thread);
    connect(m_local_worker_thread, &QThread::finished,
            m_local_generator, &QObject::deleteLater);

    m_local_worker_thread->start();

    // For user input handling
    connect(this, &LlamaChatEngine::user_inputChanged,
            this, &LlamaChatEngine::handle_new_user_input);

    setLocal_initialized(true);

    // Try connecting to remote objects
    configureRemoteObjects();

    // Start in local mode by default
    doImmediateEngineSwitch(Mode_Local);
}

//------------------------------------------------------------------------------
// doImmediateEngineSwitch: actually switch local/remote connections
// 即時にローカル/リモートを切り替え
//------------------------------------------------------------------------------
void LlamaChatEngine::doImmediateEngineSwitch(EngineMode new_mode)
{
    // Disconnect old requestGeneration signals
    disconnect(this, &LlamaChatEngine::requestGeneration, nullptr, nullptr);

    if (new_mode == Mode_Local) {
        configureLocalSignalSlots();
    } else {
        if (!m_remote_generator) {
            configureRemoteObjects();
        }
        configureRemoteSignalSlots();
        updateRemoteInitializationStatus();
    }

    setCurrentEngineMode(new_mode);
    qDebug() << "[EngineSwitch] doImmediateEngineSwitch -> newMode =" << new_mode;
}

//------------------------------------------------------------------------------
// configureRemoteObjects: set up QRemoteObjectNode, connect to server
// リモートオブジェクトを準備してサーバーと接続
//------------------------------------------------------------------------------
void LlamaChatEngine::configureRemoteObjects()
{
    qDebug() << "Connecting to remote node...";
    qDebug() << "ip_address:" << m_ip_address;
    qDebug() << "port_number:" << m_port_number;

    m_remote_mode = new QRemoteObjectNode(this);
    bool result = m_remote_mode->connectToNode(
        QUrl(QStringLiteral("tcp://%1:%2").arg(m_ip_address).arg(m_port_number))
        );

    // If IP/port is not set
    if (m_ip_address.isEmpty() || m_port_number == 0) {
        qDebug() << "IP address and/or port number not set.";
        return;
    }

    if (result) {
        qDebug() << "Connected to remote node.";
        m_remote_generator = m_remote_mode->acquire<LlamaResponseGeneratorReplica>();
        if (!m_remote_generator) {
            qDebug() << "Failed to acquire remote generator.";
        }
        m_remote_generator->setParent(this);
    } else {
        qDebug() << "Failed to connect to remote node.";
    }
}

//------------------------------------------------------------------------------
// updateRemoteInitializationStatus: check if remote is initialized
// リモートの初期化状態を確認
//------------------------------------------------------------------------------
void LlamaChatEngine::updateRemoteInitializationStatus()
{
    if (!m_remote_generator) {
        qDebug() << "No remote generator available.";
        return;
    }

    qDebug() << "m_remoteGenerator->remoteInitialized():" << m_remote_generator->remoteInitialized();
    if (m_remote_generator->remoteInitialized()) {
        qDebug() << "Remote engine initialized.";
        setRemote_initialized(true);
    } else {
        qDebug() << "Remote engine not initialized.";
        setRemote_initialized(false);

        // If remote becomes initialized, update state
        // リモートが初期化されたら状態を更新
        connect(m_remote_generator, &LlamaResponseGeneratorReplica::remoteInitializedChanged,
                this, [this]() {
                    setRemote_initialized(m_remote_generator->remoteInitialized());
                });
    }
}

//------------------------------------------------------------------------------
// configureRemoteSignalSlots: disconnect local, connect remote signals
// リモートのシグナルスロット設定
//------------------------------------------------------------------------------
void LlamaChatEngine::configureRemoteSignalSlots()
{
    // Disconnect local signals
    disconnect(m_local_generator, &LlamaResponseGenerator::partialResponseReady,
               this, &LlamaChatEngine::onPartialResponse);
    disconnect(m_local_generator, &LlamaResponseGenerator::generationFinished,
               this, &LlamaChatEngine::onGenerationFinished);

    // Connect remote signals
    connect(this, &LlamaChatEngine::requestGeneration,
            m_remote_generator, &LlamaResponseGeneratorReplica::generate);
    connect(m_remote_generator, &LlamaResponseGeneratorReplica::partialResponseReady,
            this, &LlamaChatEngine::onPartialResponse);
    connect(m_remote_generator, &LlamaResponseGeneratorReplica::generationFinished,
            this, &LlamaChatEngine::onGenerationFinished);

    qDebug() << "[EngineSwitch] Now using REMOTE engine.";
}

//------------------------------------------------------------------------------
// configureLocalSignalSlots: disconnect remote, connect local signals
// ローカルのシグナルスロット設定
//------------------------------------------------------------------------------
void LlamaChatEngine::configureLocalSignalSlots()
{
    if (m_remote_generator) {
        // Disconnect remote signals
        disconnect(m_remote_generator, &LlamaResponseGeneratorReplica::partialResponseReady,
                   this, &LlamaChatEngine::onPartialResponse);
        disconnect(m_remote_generator, &LlamaResponseGeneratorReplica::generationFinished,
                   this, &LlamaChatEngine::onGenerationFinished);
    }

    // Connect local signals
    connect(this, &LlamaChatEngine::requestGeneration,
            m_local_generator, [this](const QList<LlamaChatMessage>& messages) {
                QMetaObject::invokeMethod(
                    m_local_generator, "generate",
                    Qt::QueuedConnection,
                    Q_ARG(const QList<LlamaChatMessage>&, messages));
            });
    connect(m_local_generator, &LlamaResponseGenerator::partialResponseReady,
            this, &LlamaChatEngine::onPartialResponse);
    connect(m_local_generator, &LlamaResponseGenerator::generationFinished,
            this, &LlamaChatEngine::onGenerationFinished);

    qDebug() << "[EngineSwitch] Now using LOCAL engine.";
}

//------------------------------------------------------------------------------
// setCurrentEngineMode: not for QML use, updates engine mode
// setCurrentEngineMode: QMLから呼ばれない、エンジンモードを更新
//------------------------------------------------------------------------------
void LlamaChatEngine::setCurrentEngineMode(EngineMode new_current_engine_mode)
{
    if (m_current_engine_mode == new_current_engine_mode) {
        return;
    }
    m_current_engine_mode = new_current_engine_mode;
    emit currentEngineModeChanged();
}

//------------------------------------------------------------------------------
// doEngineInit: run heavy init in a separate thread
// doEngineInit: 重い初期化処理を別スレッドで実行
//------------------------------------------------------------------------------
void LlamaChatEngine::doEngineInit()
{
    ggml_backend_load_all();

    m_model_params = llama_model_default_params();
    m_model_params.n_gpu_layers = m_n_gl;

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

//------------------------------------------------------------------------------
// user_input getter
// user_inputゲッター
//------------------------------------------------------------------------------
QString LlamaChatEngine::user_input() const
{
    return m_user_input;
}

//------------------------------------------------------------------------------
// setUser_input: updates and notifies QML
// setUser_input: 更新しQMLに通知
//------------------------------------------------------------------------------
void LlamaChatEngine::setUser_input(const QString &new_user_input)
{
    if (m_user_input == new_user_input) {
        return;
    }
    m_user_input = new_user_input;
    emit user_inputChanged();
}

//------------------------------------------------------------------------------
// resetUser_input: clear user_input
// resetUser_input: user_inputをクリア
//------------------------------------------------------------------------------
void LlamaChatEngine::resetUser_input()
{
    setUser_input({});
}

//------------------------------------------------------------------------------
// messages(): returns ChatMessageModel instance
// messages(): ChatMessageModelのインスタンスを返す
//------------------------------------------------------------------------------
ChatMessageModel* LlamaChatEngine::messages()
{
    return &m_messages;
}

//------------------------------------------------------------------------------
// static m_model_path: default LLaMA model path from CMake
// 静的メンバ: CMakeで定義されたLLaMAモデルパス
//------------------------------------------------------------------------------
const std::string LlamaChatEngine::m_model_path {
#ifdef LLAMA_MODEL_FILE
    LLAMA_MODEL_FILE
#else
#error "LLAMA_MODEL_FILE is not defined. Please define it via target_compile_definitions() in CMake."
#endif
};

// getter
QString LlamaChatEngine::ip_address() const
{
    return m_ip_address;
}

// setter
void LlamaChatEngine::setIp_address(const QString &new_ip_address)
{
    if (m_ip_address == new_ip_address)
        return;
    m_ip_address = new_ip_address;
    emit ip_addressChanged();
}

// getter
int LlamaChatEngine::port_number() const
{
    return m_port_number;
}

// setter
void LlamaChatEngine::setPort_number(int new_port_number)
{
    if (m_port_number == new_port_number)
        return;
    m_port_number = new_port_number;
    emit port_numberChanged();
}

// getter
bool LlamaChatEngine::local_initialized() const
{
    return m_local_initialized;
}

// setter
void LlamaChatEngine::setLocal_initialized(bool new_local_initialized)
{
    if (m_local_initialized == new_local_initialized)
        return;
    m_local_initialized = new_local_initialized;
    emit local_initializedChanged();
}

// getter
bool LlamaChatEngine::remote_initialized() const
{
    return m_remote_initialized;
}

// setter
void LlamaChatEngine::setRemote_initialized(bool new_remote_initialized)
{
    if (m_remote_initialized == new_remote_initialized)
        return;
    m_remote_initialized = new_remote_initialized;
    emit remote_initializedChanged();
}

// getter
LlamaChatEngine::EngineMode LlamaChatEngine::currentEngineMode() const
{
    return m_current_engine_mode;
}


#include "llamachatengine.h"
#include <QThread>
#include <QThreadPool>
#include <QRemoteObjectNode>
#include <QtSystemDetection>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QEventLoop>
#include <QTimer>
#include <functional>
#include "llamaresponsegenerator.h"
#include "rep_LlamaResponseGenerator_replica.h"


//------------------------------------------------------------------------------
// Static: Default LLaMA model path
// 静的メンバ: デフォルトLLaMAモデルパス
//    → Androidの場合はあとで動的に設定するため、デスクトップ用にだけ適用
//------------------------------------------------------------------------------
#ifdef Q_OS_ANDROID
// Android では最初は空文字にする (実際のパスは initializeModelPathForAndroid() で決める)
const std::string LlamaChatEngine::mModelPath {""};
#else
// Windows/macOS/Linux/iOSなどの例 (従来通り)
const std::string LlamaChatEngine::mModelPath {
#ifdef LLAMA_MODEL_FILE
    LLAMA_MODEL_FILE
#else
#error "LLAMA_MODEL_FILE is not defined. Please define it via target_compile_definitions() in CMake."
#endif
};
#endif

//------------------------------------------------------------------------------
// Constructor: launch async engine init in threadpool
// コンストラクタ: スレッドプールで非同期初期化を起動
//------------------------------------------------------------------------------
LlamaChatEngine::LlamaChatEngine(QObject *parent)
    : QObject(parent)
    , mMessages(this)
{
    qSetMessagePattern("[%{file}:%{line}] %{message}");

#ifdef Q_OS_ANDROID
    // Android向け: 実行時にassetsからモデルファイルをコピー＆mModelPath設定
    if (!initializeModelPathForAndroid()) {
        qWarning() << "[LlamaChatEngine] Failed to initialize model path on Android!";
    }
#else
    // Run doEngineInit() in a background thread
    QThreadPool::globalInstance()->start([this]() {
        doEngineInit();
    });
#endif
}

//------------------------------------------------------------------------------
// Destructor: free model/context
// デストラクタ: モデル/コンテキストを解放
//------------------------------------------------------------------------------
LlamaChatEngine::~LlamaChatEngine()
{
    llama_free(mCtx);
    llama_free_model(mModel);
}

//------------------------------------------------------------------------------
// Android用モデル初期化
//------------------------------------------------------------------------------
bool LlamaChatEngine::initializeModelPathForAndroid()
{
#ifndef LLAMA_MODEL_FILE
    qWarning() << "LLAMA_MODEL_FILE is not defined. Cannot proceed.";
    return false;
#endif

#ifdef LLAMA_DOWNLOAD_URL
    downloadModelIfNeededAsync();
#else
    qWarning() << "LLAMA_DOWNLOAD_URL is not defined. Cannot proceed.";
    return false;
#endif

    return true;
}

//---------------------------------------------------------------------------
// ダウンロードして "/data/data/<package>/files/LLAMA_MODEL_FILE" に保存
//---------------------------------------------------------------------------
void LlamaChatEngine::downloadModelIfNeededAsync()
{
    connect(this, &LlamaChatEngine::modelDownloadFinished,
            this, &LlamaChatEngine::initAfterDownload);

    // (0) Check if file already exists
    QString writableDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (writableDir.isEmpty()) {
        qWarning() << "[downloadModelIfNeededAsync] No writable directory found!";
        // ダウンロード失敗を通知
        QMetaObject::invokeMethod(this, [this](){
            emit modelDownloadFinished(false);
        }, Qt::QueuedConnection);
        return;
    }
    QDir().mkpath(writableDir);

    QString localModelPath = writableDir + "/" + LLAMA_MODEL_FILE;
    if (QFile::exists(localModelPath)) {
        qDebug() << "[downloadModelIfNeededAsync] Model already exists:" << localModelPath;
        // 既にあるので成功扱い
        QMetaObject::invokeMethod(this, [this](){
            emit modelDownloadFinished(true);
        }, Qt::QueuedConnection);
        return;
    }

    // (1) ラムダでダウンロード処理を行う (同期だが別スレッドなのでUIはブロックされない)
    auto task = [=]() {
        qDebug() << "[downloadModelIfNeededAsync] Downloading from:" << LLAMA_DOWNLOAD_URL
                 << "to:" << localModelPath;

        QNetworkAccessManager manager;  // ローカルで作成
        QNetworkRequest request( QUrl(QStringLiteral( LLAMA_DOWNLOAD_URL )) );
        QNetworkReply* reply = manager.get(request);

        connect(reply, &QNetworkReply::downloadProgress, this, [=](qint64 bytesReceived, qint64 bytesTotal) {
            // (1) 進捗計算
            int progress = 0.0;
            if (bytesTotal > 0)
                progress = (double)bytesReceived / (double)bytesTotal * 100;

            // (2) メインスレッドへ発行するために QMetaObject::invokeMethod を使う
            QMetaObject::invokeMethod(this, [this, progress]() {
                // ここはUIスレッド (QueuedConnection)
                // 例: シグナルを発行してQMLから受け取るなど
                emit modelDownloadProgressChanged(progress);
                qDebug() << "[downloadModelIfNeededAsync] Progress:" << progress;
            }, Qt::QueuedConnection);
        });


        // 同期的に待つが、ここはスレッドプール内
        QEventLoop loop;
        QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        loop.exec();

        bool success = true;
        if (reply->error() != QNetworkReply::NoError) {
            qWarning() << "[downloadModelIfNeededAsync] Download error:" << reply->errorString();
            success = false;
        } else {
            // ダウンロード成功 → ファイルに書き込み
            QByteArray data = reply->readAll();
            QFile outFile(localModelPath);
            if (!outFile.open(QIODevice::WriteOnly)) {
                qWarning() << "[downloadModelIfNeededAsync] Failed to open for writing:" << localModelPath;
                success = false;
            } else {
                outFile.write(data);
                outFile.close();
                qDebug() << "[downloadModelIfNeededAsync] Model saved to:" << localModelPath;
            }
        }
        reply->deleteLater();

        // (2) メインスレッドに完了を通知 (modelDownloadFinishedシグナルを発行)
        QMetaObject::invokeMethod(this, [this, success]() {
            emit modelDownloadFinished(success);
        }, Qt::QueuedConnection);
    };

    // (3) スレッドプールへ投入
    QThreadPool::globalInstance()->start(task);
}

//-----------------------------------------------------------------------------
// ダウンロード完了後に呼ばれるスロット (シグナル modelDownloadFinished で繋ぐ想定)
//-----------------------------------------------------------------------------
void LlamaChatEngine::initAfterDownload(bool success)
{
    if (!success) {
        qWarning() << "[initAfterDownload] Model download failed, cannot proceed.";
        return;
    }

    // (1) ダウンロード成功 → mModelPath に設定
    QString writableDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QString localModelPath = writableDir + "/" + LLAMA_MODEL_FILE;
    std::string *ptr = const_cast<std::string*>(&mModelPath);
    *ptr = localModelPath.toStdString();

    // (2) モデルロード開始
    QThreadPool::globalInstance()->start([this]() {
        doEngineInit();
    });
}

//------------------------------------------------------------------------------
// switchEngineMode: user chooses local vs remote
// switchEngineMode: ユーザーがローカル/リモートを切り替える
//------------------------------------------------------------------------------
void LlamaChatEngine::switchEngineMode(EngineMode newMode)
{
    if (newMode == mCurrentEngineMode) {
        // Already in this mode, do nothing
        return;
    }

    if (mInProgress) {
        // If generation is ongoing, switch after finishing
        mPendingEngineSwitchMode = newMode;
        return;
    }
    doImmediateEngineSwitch(newMode);
}

//------------------------------------------------------------------------------
// handle_new_user_input: process user input, emit requestGeneration
// handle_new_user_input: ユーザー入力を処理し、requestGenerationをemit
//------------------------------------------------------------------------------
void LlamaChatEngine::handle_new_user_input()
{
    if (mInProgress) {
        qDebug() << "Generation in progress, ignoring new input.";
        return;
    }

    static QList<LlamaChatMessage> messages;

    if (mUserInput.isEmpty()) {
        return;
    }

    // Add user message
    LlamaChatMessage msg;
    msg.setRole(QStringLiteral("user"));
    msg.setContent(mUserInput);
    messages.append(msg);
    mMessages.appendSingle("user", msg.content());

    // Emit request for generation
    emit requestGeneration(messages);
}

//------------------------------------------------------------------------------
// onPartialResponse: update partial text in UI
// onPartialResponse: 部分的なテキストをUIに反映
//------------------------------------------------------------------------------
void LlamaChatEngine::onPartialResponse(const QString &textSoFar)
{
    if (!mInProgress) {
        mCurrentAssistantIndex = mMessages.appendSingle("assistant", textSoFar);
        mInProgress = true;
    } else {
        mMessages.updateMessageContent(mCurrentAssistantIndex, textSoFar);
    }
}

//------------------------------------------------------------------------------
// onGenerationFinished: finalize generation
// onGenerationFinished: 生成完了後の処理
//------------------------------------------------------------------------------
void LlamaChatEngine::onGenerationFinished(const QString &finalResponse)
{
    if (mInProgress) {
        mMessages.updateMessageContent(mCurrentAssistantIndex, finalResponse);
        mInProgress = false;
        mCurrentAssistantIndex = -1;
    }

    // Switch mode if needed
    if (mPendingEngineSwitchMode.has_value()) {
        EngineMode pending = *mPendingEngineSwitchMode;
        mPendingEngineSwitchMode.reset();
        doImmediateEngineSwitch(pending);
    }
}

//------------------------------------------------------------------------------
// onEngineInitFinished: set up local engine, default = local
// onEngineInitFinished: ローカルエンジンをセットアップ、デフォルトはローカル
//------------------------------------------------------------------------------
void LlamaChatEngine::onEngineInitFinished()
{
    // Create local generator
    mLocalGenerator = new LlamaResponseGenerator(nullptr, mModel, mCtx);
    mLocalWorkerThread = new QThread(this);

    mLocalGenerator->moveToThread(mLocalWorkerThread);
    connect(mLocalWorkerThread, &QThread::finished,
            mLocalGenerator, &QObject::deleteLater);

    mLocalWorkerThread->start();

    // Setup common signal/slot (e.g. user input)
    setupCommonConnections();

    setLocalInitialized(true);

    // Attempt remote connection
    configureRemoteObjects();

    // Default to local
    doImmediateEngineSwitch(Mode_Local);
}

//------------------------------------------------------------------------------
// onInferenceError: handle generationError from local/remote
// onInferenceError: ローカル/リモートからのgenerationErrorを処理
//------------------------------------------------------------------------------
void LlamaChatEngine::onInferenceError(const QString &errorMessage)
{
    if (mCurrentEngineMode == Mode_Local) {
        setLocalAiInError(true);

        // Reinit local in background
        QThreadPool::globalInstance()->start([this]() {
            reinitLocalEngine();
        });
    } else {
        setRemoteAiInError(true);

        // If remote generator exists, reinit
        if (mRemoteGenerator) {
            mRemoteGenerator->reinitEngine();
        }
    }
}

//------------------------------------------------------------------------------
// reinitLocalEngine: re-initialize local engine
// reinitLocalEngine: ローカル推論エンジンを再初期化
//------------------------------------------------------------------------------
void LlamaChatEngine::reinitLocalEngine()
{
    qDebug() << "[LlamaChatEngine::reinitLocalEngine] Start reinitializing local engine.";

    setLocalInitialized(false);
    teardownLocalConnections();
    teardownCommonConnections();

    // Stop worker thread
    if (mLocalWorkerThread) {
        mLocalWorkerThread->quit();
        mLocalWorkerThread->wait();
        mLocalWorkerThread->deleteLater();
        mLocalWorkerThread = nullptr;
    }

    // Release context/model
    if (mCtx) {
        llama_free(mCtx);
        mCtx = nullptr;
    }
    if (mModel) {
        llama_free_model(mModel);
        mModel = nullptr;
    }

    // Re-run doEngineInit()
    doEngineInit();
    setLocalAiInError(false);
}

//------------------------------------------------------------------------------
// doEngineInit: run heavy init
// doEngineInit: 重い初期化を実行
//------------------------------------------------------------------------------
void LlamaChatEngine::doEngineInit()
{
    ggml_backend_load_all();

    mModelParams = llama_model_default_params();
    mModelParams.n_gpu_layers = mNGl;

    // ここで mModelPath を参照
    if (mModelPath.empty()) {
        qWarning() << "[doEngineInit] mModelPath is empty. Model cannot be loaded.";
        return;
    }
    qDebug() << "[doEngineInit] Loading model from path:" << mModelPath.c_str();

    mModel = llama_load_model_from_file(mModelPath.c_str(), mModelParams);
    if (!mModel) {
        fprintf(stderr, "Error: unable to load model.\n");
        return;
    }

    mCtxParams = llama_context_default_params();
    mCtxParams.n_ctx   = mNCtx;
    mCtxParams.n_batch = mNCtx;

    mCtx = llama_new_context_with_model(mModel, mCtxParams);
    if (!mCtx) {
        fprintf(stderr, "Error: failed to create llama_context.\n");
        return;
    }

    QMetaObject::invokeMethod(this, [this] {
        onEngineInitFinished();
    }, Qt::QueuedConnection);
}

//------------------------------------------------------------------------------
// doImmediateEngineSwitch: actually switch local/remote
// doImmediateEngineSwitch: 実際にローカル/リモートを切り替える
//------------------------------------------------------------------------------
void LlamaChatEngine::doImmediateEngineSwitch(EngineMode newMode)
{
    if (newMode == Mode_Local) {
        configureLocalSignalSlots();
    } else {
        if (!mRemoteGenerator) {
            configureRemoteObjects();
        }
        configureRemoteSignalSlots();
        updateRemoteInitializationStatus();
    }

    setCurrentEngineMode(newMode);
    qDebug() << "[EngineSwitch] doImmediateEngineSwitch -> newMode =" << newMode;
}

//------------------------------------------------------------------------------
// configureRemoteObjects: attempt connecting to remote server
// configureRemoteObjects: リモートサーバーに接続
//------------------------------------------------------------------------------
void LlamaChatEngine::configureRemoteObjects()
{
    qDebug() << "Connecting to remote node...";
    qDebug() << "ip_address:" << mIpAddress;
    qDebug() << "port_number:" << mPortNumber;

    if (mIpAddress.isEmpty() || mPortNumber == 0) {
        qDebug() << "IP address and/or port number not set.";
        return;
    }

    mRemoteNode = new QRemoteObjectNode(this);
    bool result = mRemoteNode->connectToNode(
        QUrl(QStringLiteral("tcp://%1:%2").arg(mIpAddress).arg(mPortNumber))
        );

    if (result) {
        qDebug() << "Connected to remote node.";
        mRemoteGenerator = mRemoteNode->acquire<LlamaResponseGeneratorReplica>();
        if (!mRemoteGenerator) {
            qDebug() << "Failed to acquire remote generator.";
        }
        mRemoteGenerator->setParent(this);

        if (!mRemoteInitializedConnection.has_value()) {
            mRemoteInitializedConnection = connect(
                mRemoteGenerator, &LlamaResponseGeneratorReplica::remoteInitializedChanged,
                this, &LlamaChatEngine::updateRemoteInitializationStatus
                );
        }
    } else {
        qDebug() << "Failed to connect to remote node.";
    }
}

//------------------------------------------------------------------------------
// updateRemoteInitializationStatus: check remote init flag
// updateRemoteInitializationStatus: リモート初期化フラグを確認
//------------------------------------------------------------------------------
void LlamaChatEngine::updateRemoteInitializationStatus()
{
    if (!mRemoteGenerator) {
        qDebug() << "No remote generator available.";
        return;
    }

    qDebug() << "mRemoteGenerator->remoteInitialized():" << mRemoteGenerator->remoteInitialized();
    if (mRemoteGenerator->remoteInitialized()) {
        qDebug() << "Remote engine initialized.";
        setRemoteInitialized(true);
        setRemoteAiInError(false);
    } else {
        qDebug() << "Remote engine not initialized.";
        setRemoteInitialized(false);
        setRemoteAiInError(true);
    }
}

//------------------------------------------------------------------------------
// configureRemoteSignalSlots: switch from local signals to remote signals
// configureRemoteSignalSlots: ローカル→リモートのシグナルスロットに切り替え
//------------------------------------------------------------------------------
void LlamaChatEngine::configureRemoteSignalSlots()
{
    teardownLocalConnections();
    setupRemoteConnections();
    qDebug() << "[EngineSwitch] Now using REMOTE engine.";
}

//------------------------------------------------------------------------------
// configureLocalSignalSlots: switch from remote signals to local signals
// configureLocalSignalSlots: リモート→ローカルのシグナルスロットに切り替え
//------------------------------------------------------------------------------
void LlamaChatEngine::configureLocalSignalSlots()
{
    teardownRemoteConnections();
    setupLocalConnections();
    qDebug() << "[EngineSwitch] Now using LOCAL engine.";
}

//------------------------------------------------------------------------------
// setCurrentEngineMode: private setter
// setCurrentEngineMode: private セッター
//------------------------------------------------------------------------------
void LlamaChatEngine::setCurrentEngineMode(EngineMode newCurrentEngineMode)
{
    if (mCurrentEngineMode == newCurrentEngineMode) {
        return;
    }
    mCurrentEngineMode = newCurrentEngineMode;
    emit currentEngineModeChanged();
}

//------------------------------------------------------------------------------
// Setup / Teardown Connections
//------------------------------------------------------------------------------
void LlamaChatEngine::setupRemoteConnections()
{
    if (!mRemoteGenerator) {
        qWarning() << "No remote generator available. Cannot connect.";
        return;
    }

    teardownRemoteConnections();

    mRemoteRequestGenerationConnection = connect(
        this, &LlamaChatEngine::requestGeneration,
        mRemoteGenerator, &LlamaResponseGeneratorReplica::generate
        );

    mRemotePartialResponseConnection = connect(
        mRemoteGenerator, &LlamaResponseGeneratorReplica::partialResponseReady,
        this, &LlamaChatEngine::onPartialResponse
        );

    mRemoteGenerationFinishedConnection = connect(
        mRemoteGenerator, &LlamaResponseGeneratorReplica::generationFinished,
        this, &LlamaChatEngine::onGenerationFinished
        );

    mRemoteGenerationErrorConnection = connect(
        mRemoteGenerator, &LlamaResponseGeneratorReplica::generationError,
        this, &LlamaChatEngine::inferenceErrorToQML
        );

    mRemoteGenerationErrorToQmlConnection = connect(
        mRemoteGenerator, &LlamaResponseGeneratorReplica::generationError,
        this, &LlamaChatEngine::onInferenceError
        );

    qDebug() << "[setupRemoteConnections] Remote connections established.";
}

void LlamaChatEngine::teardownRemoteConnections()
{
    if (mRemoteRequestGenerationConnection.has_value()) {
        disconnect(*mRemoteRequestGenerationConnection);
        mRemoteRequestGenerationConnection.reset();
    }

    if (mRemotePartialResponseConnection.has_value()) {
        disconnect(*mRemotePartialResponseConnection);
        mRemotePartialResponseConnection.reset();
    }

    if (mRemoteGenerationFinishedConnection.has_value()) {
        disconnect(*mRemoteGenerationFinishedConnection);
        mRemoteGenerationFinishedConnection.reset();
    }

    if (mRemoteGenerationErrorConnection.has_value()) {
        disconnect(*mRemoteGenerationErrorConnection);
        mRemoteGenerationErrorConnection.reset();
    }

    if (mRemoteGenerationErrorToQmlConnection.has_value()) {
        disconnect(*mRemoteGenerationErrorToQmlConnection);
        mRemoteGenerationErrorToQmlConnection.reset();
    }

    qDebug() << "[teardownRemoteConnections] Remote connections torn down.";
}

void LlamaChatEngine::setupLocalConnections()
{
    if (!mLocalGenerator) {
        qWarning() << "No local generator available. Cannot connect.";
        return;
    }

    teardownLocalConnections();

    mLocalRequestGenerationConnection = connect(
        this, &LlamaChatEngine::requestGeneration,
        mLocalGenerator, &LlamaResponseGenerator::generate
        );

    mLocalPartialResponseConnection = connect(
        mLocalGenerator, &LlamaResponseGenerator::partialResponseReady,
        this, &LlamaChatEngine::onPartialResponse
        );

    mLocalGenerationFinishedConnection = connect(
        mLocalGenerator, &LlamaResponseGenerator::generationFinished,
        this, &LlamaChatEngine::onGenerationFinished
        );

    mLocalGenerationErrorConnection = connect(
        mLocalGenerator, &LlamaResponseGenerator::generationError,
        this, &LlamaChatEngine::onInferenceError
        );

    mLocalGenerationErrorToQmlConnection = connect(
        mLocalGenerator, &LlamaResponseGenerator::generationError,
        this, &LlamaChatEngine::inferenceErrorToQML
        );

    qDebug() << "[setupLocalConnections] Local connections established.";
}

void LlamaChatEngine::teardownLocalConnections()
{
    if (mLocalRequestGenerationConnection.has_value()) {
        disconnect(*mLocalRequestGenerationConnection);
        mLocalRequestGenerationConnection.reset();
    }
    if (mLocalPartialResponseConnection.has_value()) {
        disconnect(*mLocalPartialResponseConnection);
        mLocalPartialResponseConnection.reset();
    }
    if (mLocalGenerationFinishedConnection.has_value()) {
        disconnect(*mLocalGenerationFinishedConnection);
        mLocalGenerationFinishedConnection.reset();
    }
    if (mLocalGenerationErrorConnection.has_value()) {
        disconnect(*mLocalGenerationErrorConnection);
        mLocalGenerationErrorConnection.reset();
    }
    if (mLocalGenerationErrorToQmlConnection.has_value()) {
        disconnect(*mLocalGenerationErrorToQmlConnection);
        mLocalGenerationErrorToQmlConnection.reset();
    }

    qDebug() << "[teardownLocalConnections] Local connections torn down.";
}

void LlamaChatEngine::setupCommonConnections()
{
    teardownCommonConnections();

    mHandleNewUserInputConnection = connect(
        this, &LlamaChatEngine::userInputChanged,
        this, &LlamaChatEngine::handle_new_user_input
        );

    qDebug() << "[setupCommonConnections] Common connections established.";
}

void LlamaChatEngine::teardownCommonConnections()
{
    if (mHandleNewUserInputConnection.has_value()) {
        disconnect(*mHandleNewUserInputConnection);
        mHandleNewUserInputConnection.reset();
    }

    qDebug() << "[teardownCommonConnections] Common connections torn down.";
}

//------------------------------------------------------------------------------
// inProgress Getter/Setter
//------------------------------------------------------------------------------
bool LlamaChatEngine::inProgress() const
{
    return mInProgress;
}

void LlamaChatEngine::setInProgress(bool newInProgress)
{
    if (mInProgress == newInProgress)
        return;
    mInProgress = newInProgress;
    emit inProgressChanged();
}

//------------------------------------------------------------------------------
// localAiInError Getter/Setter
//------------------------------------------------------------------------------
bool LlamaChatEngine::localAiInError() const
{
    return mLocalAiInError;
}

void LlamaChatEngine::setLocalAiInError(bool newLocalAiInError)
{
    if (mLocalAiInError == newLocalAiInError)
        return;
    mLocalAiInError = newLocalAiInError;
    emit localAiInErrorChanged();
}

//------------------------------------------------------------------------------
// remoteAiInError Getter/Setter
//------------------------------------------------------------------------------
bool LlamaChatEngine::remoteAiInError() const
{
    return mRemoteAiInError;
}

void LlamaChatEngine::setRemoteAiInError(bool newRemoteAiInError)
{
    if (mRemoteAiInError == newRemoteAiInError)
        return;
    mRemoteAiInError = newRemoteAiInError;
    emit remoteAiInErrorChanged();
}

//------------------------------------------------------------------------------
// userInput Getter/Setter
//------------------------------------------------------------------------------
QString LlamaChatEngine::userInput() const
{
    return mUserInput;
}

void LlamaChatEngine::setUserInput(const QString &newUserInput)
{
    if (mUserInput == newUserInput) {
        return;
    }
    mUserInput = newUserInput;
    emit userInputChanged();
}

void LlamaChatEngine::resetUserInput()
{
    setUserInput({});
}

//------------------------------------------------------------------------------
// messages(): return ChatMessageModel reference
// messages(): ChatMessageModelの参照を返す
//------------------------------------------------------------------------------
ChatMessageModel* LlamaChatEngine::messages()
{
    return &mMessages;
}

//------------------------------------------------------------------------------
// ipAddress Getter/Setter
//------------------------------------------------------------------------------
QString LlamaChatEngine::ipAddress() const
{
    return mIpAddress;
}

void LlamaChatEngine::setIpAddress(const QString &newIpAddress)
{
    if (mIpAddress == newIpAddress)
        return;
    mIpAddress = newIpAddress;
    emit ipAddressChanged();
}

//------------------------------------------------------------------------------
// portNumber Getter/Setter
//------------------------------------------------------------------------------
int LlamaChatEngine::portNumber() const
{
    return mPortNumber;
}

void LlamaChatEngine::setPortNumber(int newPortNumber)
{
    if (mPortNumber == newPortNumber)
        return;
    mPortNumber = newPortNumber;
    emit portNumberChanged();
}

//------------------------------------------------------------------------------
// localInitialized Getter/Setter
//------------------------------------------------------------------------------
bool LlamaChatEngine::localInitialized() const
{
    return mLocalInitialized;
}

void LlamaChatEngine::setLocalInitialized(bool newLocalInitialized)
{
    if (mLocalInitialized == newLocalInitialized)
        return;
    mLocalInitialized = newLocalInitialized;
    emit localInitializedChanged();
}

//------------------------------------------------------------------------------
// remoteInitialized Getter/Setter
//------------------------------------------------------------------------------
bool LlamaChatEngine::remoteInitialized() const
{
    return mRemoteInitialized;
}

void LlamaChatEngine::setRemoteInitialized(bool newRemoteInitialized)
{
    if (mRemoteInitialized == newRemoteInitialized)
        return;
    mRemoteInitialized = newRemoteInitialized;
    emit remoteInitializedChanged();
}

//------------------------------------------------------------------------------
// currentEngineMode Getter
//------------------------------------------------------------------------------
LlamaChatEngine::EngineMode LlamaChatEngine::currentEngineMode() const
{
    return mCurrentEngineMode;
}

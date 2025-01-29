#include "LlamaChatEngine.h"
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
#include "LlamaResponseGenerator.h"
#include "rep_LlamaResponseGenerator_replica.h"
#include "common.h"

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
// Constructor
// コンストラクタ: スレッドプールで非同期初期化を起動 (iOS/Androidで挙動分岐)
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
    // デスクトップなど: スレッドプールで doEngineInit() を実行
    QThreadPool::globalInstance()->start([this]() {
        doEngineInit();
    });
#endif
}

//------------------------------------------------------------------------------
// Destructor
// デストラクタ: モデル/コンテキストを解放
//------------------------------------------------------------------------------
LlamaChatEngine::~LlamaChatEngine()
{
    llama_free(mCtx);
    llama_free_model(mModel);
}

//------------------------------------------------------------------------------
// doEngineInit
// モデル読込などの重い初期化処理
//------------------------------------------------------------------------------
void LlamaChatEngine::doEngineInit()
{
    ggml_backend_load_all();

    if (mModelPath.empty()) {
        qWarning() << "[doEngineInit] mModelPath is empty. Model cannot be loaded.";
        return;
    }
    qDebug() << "[doEngineInit] Loading model from path:" << mModelPath.c_str();

    mModelParams = llama_model_default_params();
    mModelParams.n_gpu_layers = mNGl;

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
// onEngineInitFinished
// ローカルエンジンをセットアップし、デフォルトをローカルに設定
//------------------------------------------------------------------------------
void LlamaChatEngine::onEngineInitFinished()
{
    mLocalGenerator = new LlamaResponseGenerator(nullptr, mModel, mCtx);
    mLocalWorkerThread = new QThread(this);

    mLocalGenerator->moveToThread(mLocalWorkerThread);
    connect(mLocalWorkerThread, &QThread::finished,
            mLocalGenerator, &QObject::deleteLater);

    mLocalWorkerThread->start();

    setupCommonConnections();

    setLocalInitialized(true);

    // Attempt remote connection
    configureRemoteObjects();
    // Default to local
    doImmediateEngineSwitch(Mode_Local);
}

//------------------------------------------------------------------------------
// configureRemoteObjects
// リモートサーバーに接続
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

    // --- QUrl にスキームを付けずにホスト名(IP)とポートだけを設定 ---
    QUrl url;
    url.setHost(mIpAddress);
    url.setPort(mPortNumber);

    const bool result = mRemoteGenerator.setupRemoteConnection(url);

    if (result) {
        if (!mRemoteInitializedConnection.has_value()) {
            mRemoteInitializedConnection = connect(
                &mRemoteGenerator,
                &RemoteResponseGeneratorCompositor::remoteInitializedChanged,
                this,
                &LlamaChatEngine::updateRemoteInitializationStatus
                );
        }
    } else {
        qWarning() << "Failed to connect to the remote inference server at "
                   << mIpAddress << ":" << mPortNumber;
    }
}


//------------------------------------------------------------------------------
// updateRemoteInitializationStatus
// リモート初期化フラグを確認
//------------------------------------------------------------------------------
void LlamaChatEngine::updateRemoteInitializationStatus()
{
    qDebug() << "mRemoteGenerator->remoteInitialized():" << mRemoteGenerator.remoteInitialized();

    if (mRemoteGenerator.remoteInitialized()) {
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
// doImmediateEngineSwitch
// 実際にローカル/リモートを切り替える
//------------------------------------------------------------------------------
void LlamaChatEngine::doImmediateEngineSwitch(EngineMode newMode)
{
    if (newMode == Mode_Local) {
        configureLocalSignalSlots();
    } else {
        if(!mRemoteGenerator.remoteInitialized()) {
            configureRemoteObjects();
        }
        configureRemoteSignalSlots();
        updateRemoteInitializationStatus();
    }
    setCurrentEngineMode(newMode);
    qDebug() << "[EngineSwitch] doImmediateEngineSwitch -> newMode =" << newMode;
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
// configureLocalSignalSlots
// リモート→ローカルのシグナルスロット切り替え
//------------------------------------------------------------------------------
void LlamaChatEngine::configureLocalSignalSlots()
{
    teardownRemoteConnections();
    setupLocalConnections();
    qDebug() << "[EngineSwitch] Now using LOCAL engine.";
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
    if (mLocalGenerationFinishedToQMLConnection.has_value()) {
        disconnect(*mLocalGenerationFinishedToQMLConnection);
        mLocalGenerationFinishedToQMLConnection.reset();
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

    mLocalGenerationFinishedToQMLConnection = connect(
        mLocalGenerator, &LlamaResponseGenerator::generationFinished,
        this, &LlamaChatEngine::generationFinishedToQML
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

//------------------------------------------------------------------------------
// configureRemoteSignalSlots
// ローカル→リモートのシグナルスロット切り替え
//------------------------------------------------------------------------------
void LlamaChatEngine::configureRemoteSignalSlots()
{
    teardownLocalConnections();
    setupRemoteConnections();
    qDebug() << "[EngineSwitch] Now using REMOTE engine.";
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
    if (mRemoteGenerationFinishedToQMLConnection.has_value()) {
        disconnect(*mRemoteGenerationFinishedToQMLConnection);
        mRemoteGenerationFinishedToQMLConnection.reset();
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

void LlamaChatEngine::setupRemoteConnections()
{
    teardownRemoteConnections();

    mRemoteRequestGenerationConnection = connect(
        this, &LlamaChatEngine::requestGeneration,
        &mRemoteGenerator, &RemoteResponseGeneratorCompositor::generate
        );

    mRemotePartialResponseConnection = connect(
        &mRemoteGenerator, &RemoteResponseGeneratorCompositor::partialResponseReady,
        this, &LlamaChatEngine::onPartialResponse
        );

    mRemoteGenerationFinishedConnection = connect(
        &mRemoteGenerator, &RemoteResponseGeneratorCompositor::generationFinished,
        this, &LlamaChatEngine::onGenerationFinished
        );

    mRemoteGenerationFinishedToQMLConnection = connect(
        &mRemoteGenerator, &RemoteResponseGeneratorCompositor::generationFinished,
        this, &LlamaChatEngine::generationFinishedToQML
        );

    mRemoteGenerationErrorConnection = connect(
        &mRemoteGenerator, &RemoteResponseGeneratorCompositor::generationError,
        this, &LlamaChatEngine::inferenceErrorToQML
        );

    mRemoteGenerationErrorToQmlConnection = connect(
        &mRemoteGenerator, &RemoteResponseGeneratorCompositor::generationError,
        this, &LlamaChatEngine::onInferenceError
        );

    qDebug() << "[setupRemoteConnections] Remote connections established.";
}

//------------------------------------------------------------------------------
// handle_new_user_input
// ユーザー入力を処理し、requestGenerationをemit
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

    setOperationPhase(LlamaRunning);
    LlamaChatMessage msg;
    msg.setRole(QStringLiteral("user"));
    msg.setContent(mUserInput);
    messages.append(msg);

    mMessages.appendSingle("user", msg.content());
    emit requestGeneration(messages);
}

//------------------------------------------------------------------------------
// switchEngineMode
// ユーザーがローカル/リモートエンジンを切り替える (QML Invokable)
//------------------------------------------------------------------------------
void LlamaChatEngine::switchEngineMode(EngineMode newMode)
{
    if (newMode == mCurrentEngineMode) {
        // Already in this mode
        return;
    }
    if (mInProgress) {
        // Switch after finishing if generation is ongoing
        mPendingEngineSwitchMode = newMode;
        return;
    }
    doImmediateEngineSwitch(newMode);
}

void LlamaChatEngine::pauseVoiceDetection()
{
    if(!m_voiceDetector) {
        qWarning() << "Voice detector not initialized.";
        return;
    }
    m_voiceDetector->pause();
}

void LlamaChatEngine::resumeVoiceDetection()
{
    if(!m_voiceDetector) {
        qWarning() << "Voice detector not initialized.";
        return;
    }
    m_voiceDetector->resume();
}

void LlamaChatEngine::setVoiceRecognitionLanguage(const QString &language)
{
    if(!m_voiceRecognitionEngine) {
        qWarning() << "Voice recognition engine not initialized.";
        return;
    }
    m_voiceRecognitionEngine->setLanguage(language);
}

void LlamaChatEngine::initiateVoiceRecognition()
{
    initVoiceRecognition();
    startVoiceRecognition();
}

//------------------------------------------------------------------------------
// onPartialResponse
// 部分的なテキストをUIに反映
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
// onGenerationFinished
// 生成完了後の後処理
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

    if(m_voiceDetector && m_voiceRecognitionEngine) {
        setOperationPhase(Listening);
    } else {
        setOperationPhase(WaitingUserInput);
    }
}

//------------------------------------------------------------------------------
// onInferenceError
// ローカル/リモートからのgenerationErrorを処理
//------------------------------------------------------------------------------
void LlamaChatEngine::onInferenceError(const QString &errorMessage)
{
    if (mCurrentEngineMode == Mode_Local) {
        setLocalAiInError(true);
        QThreadPool::globalInstance()->start([this]() {
            reinitLocalEngine();
        });
    } else {
        setRemoteAiInError(true);
        mRemoteGenerator.reinitEngine();
    }
}

//------------------------------------------------------------------------------
// reinitLocalEngine
// ローカル推論エンジンを再初期化
//------------------------------------------------------------------------------
void LlamaChatEngine::reinitLocalEngine()
{
    qDebug() << "[LlamaChatEngine::reinitLocalEngine] Start reinitializing local engine.";

    setLocalInitialized(false);
    teardownLocalConnections();
    teardownCommonConnections();

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
// initializeModelPathForAndroid
// Android用モデル初期化 (assets→filesへコピー or ダウンロード)
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

//------------------------------------------------------------------------------
// downloadModelIfNeededAsync
// ダウンロードして "/data/data/<package>/files/LLAMA_MODEL_FILE" に保存
//------------------------------------------------------------------------------
void LlamaChatEngine::downloadModelIfNeededAsync()
{
    connect(this, &LlamaChatEngine::modelDownloadFinished,
            this, &LlamaChatEngine::initAfterDownload);

    // (0) check if file already exists
    QString writableDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (writableDir.isEmpty()) {
        qWarning() << "[downloadModelIfNeededAsync] No writable directory found!";
        QMetaObject::invokeMethod(this, [this](){
            emit modelDownloadFinished(false);
        }, Qt::QueuedConnection);
        return;
    }
    QDir().mkpath(writableDir);

    QString localModelPath = writableDir + "/" + LLAMA_MODEL_FILE;
    if (QFile::exists(localModelPath)) {
        qDebug() << "[downloadModelIfNeededAsync] Model already exists:" << localModelPath;
        QMetaObject::invokeMethod(this, [this](){
            emit modelDownloadFinished(true);
        }, Qt::QueuedConnection);
        return;
    }

    auto task = [=]() {
        qDebug() << "[downloadModelIfNeededAsync] Downloading from:" << LLAMA_DOWNLOAD_URL
                 << "to:" << localModelPath;

        QNetworkAccessManager manager;
        QNetworkRequest request( QUrl(QStringLiteral( LLAMA_DOWNLOAD_URL )) );
        QNetworkReply* reply = manager.get(request);

        connect(reply, &QNetworkReply::downloadProgress, this, [=](qint64 bytesReceived, qint64 bytesTotal) {
            if (bytesTotal > 0) {
                mModelDownloadProgress = static_cast<double>(bytesReceived) / static_cast<double>(bytesTotal);
            }
            QMetaObject::invokeMethod(this, [this]() {
                emit modelDownloadProgressChanged();
                qDebug() << "[downloadModelIfNeededAsync] Progress:" << mModelDownloadProgress;
            }, Qt::QueuedConnection);
        });

        // Wait synchronously in this thread pool thread
        QEventLoop loop;
        QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        loop.exec();

        bool success = true;
        setModelDownloadInProgress(false);

        if (reply->error() != QNetworkReply::NoError) {
            qWarning() << "[downloadModelIfNeededAsync] Download error:" << reply->errorString();
            success = false;
        } else {
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

        QMetaObject::invokeMethod(this, [this, success]() {
            emit modelDownloadFinished(success);
        }, Qt::QueuedConnection);
    };

    QThreadPool::globalInstance()->start(task);
    setModelDownloadInProgress(true);
}

//------------------------------------------------------------------------------
// initAfterDownload
// ダウンロード完了後に呼ばれるスロット
//------------------------------------------------------------------------------
void LlamaChatEngine::initAfterDownload(bool success)
{
    if (!success) {
        qWarning() << "[initAfterDownload] Model download failed, cannot proceed.";
        return;
    }

    QString writableDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QString localModelPath = writableDir + "/" + LLAMA_MODEL_FILE;
    std::string *ptr = const_cast<std::string*>(&mModelPath);
    *ptr = localModelPath.toStdString();

    // モデルロード開始 (非同期)
    QThreadPool::globalInstance()->start([this]() {
        doEngineInit();
    });
}

//------------------------------------------------------------------------------
// setCurrentEngineMode (private setter)
//------------------------------------------------------------------------------
void LlamaChatEngine::setCurrentEngineMode(EngineMode newCurrentEngineMode)
{
    if (mCurrentEngineMode == newCurrentEngineMode) {
        return;
    }
    mCurrentEngineMode = newCurrentEngineMode;
    emit currentEngineModeChanged();
}

void LlamaChatEngine::initVoiceRecognition()
{
    if (m_voiceRecognitionEngine) {
        // すでに作ってたら再初期化など
        delete m_voiceRecognitionEngine;
        m_voiceRecognitionEngine = nullptr;
    }
    m_voiceRecognitionEngine = new VoiceRecognitionEngine(this);

    // Whisper設定
    VoiceRecParams vrParams;
    vrParams.language = "auto";
    vrParams.model    = WHISPER_MODEL_NAME;
    vrParams.length_for_inference_ms  = 10000;  // 10秒取りたい
    vrParams.vad_thold  = 0.6f;
    vrParams.freq_thold = 100.0f;
    // ... GPU設定など

    const bool ok = m_voiceRecognitionEngine->initWhisper(vrParams);
    if (!ok) {
        qWarning() << "Failed to init VoiceRecognitionEngine";
        return;
    }

    // シグナル接続: 音声認識結果 -> handleRecognizedText()
    connect(m_voiceRecognitionEngine, &VoiceRecognitionEngine::textRecognized,
            this, &LlamaChatEngine::handleRecognizedText);
    // whisperが検知した言語が変わったらdetectedVoiceLocaleにセット
    connect(m_voiceRecognitionEngine, &VoiceRecognitionEngine::detectedVoiceLocaleChanged,
            this, &LlamaChatEngine::setDetectedVoiceLocale);
    // オペレーションのフェーズ遷移シグナルの接続
    connect(m_voiceRecognitionEngine, &VoiceRecognitionEngine::changeOperationPhaseTo,
            this, &LlamaChatEngine::setOperationPhase);

    if (!m_voiceDetector) {
        m_voiceDetector = new VoiceDetector(vrParams.length_for_inference_ms, this);
        // VoiceDetectorが音声を取得したら voiceEngine->addAudio(...) へ渡す
        connect(m_voiceDetector, &VoiceDetector::audioAvailable,
                m_voiceRecognitionEngine, &VoiceRecognitionEngine::addAudio);
        // オペレーションのフェーズ遷移シングナルの接続
        connect(m_voiceDetector, &VoiceDetector::changeOperationPhaseTo,
                this, &LlamaChatEngine::setOperationPhase);
        // VoiceDetectorの初期化 (例: init(16kHz), start capturing, etc.)
        m_voiceDetector->init(/*sampleRate=*/COMMON_SAMPLE_RATE, /*channelCount=*/1);
    }
}

void LlamaChatEngine::startVoiceRecognition()
{
    if (!m_voiceRecognitionEngine) {
        initVoiceRecognition();
    }
    if (m_voiceDetector && !m_voiceRecognitionEngine->isRunning()) {
        // VoiceDetectorスタート
        m_voiceDetector->resume();   // or start capturing
        // VoiceRecognitionEngineスタート
        m_voiceRecognitionEngine->start();
    }
}

OperationPhase LlamaChatEngine::operationPhase() const
{
    return m_operationPhase;
}

void LlamaChatEngine::setOperationPhase(OperationPhase newOperationPhase)
{
    if (m_operationPhase == newOperationPhase)
        return;

    if((m_operationPhase == LlamaRunning || m_operationPhase == Speaking) && (newOperationPhase != WaitingUserInput && newOperationPhase != Listening)) {
        return;
    }

    m_operationPhase = newOperationPhase;
    emit operationPhaseChanged();
}

void LlamaChatEngine::stopVoiceRecognition()
{
    if (m_voiceDetector) {
        m_voiceDetector->pause(); // or stop
    }
    if (m_voiceRecognitionEngine && m_voiceRecognitionEngine->isRunning()) {
        m_voiceRecognitionEngine->stop();
    }
    setOperationPhase(WaitingUserInput);
}

void LlamaChatEngine::setDetectedVoiceLocale(const QLocale &newDetectedVoiceLocale)
{
    if (m_detectedVoiceLocale == newDetectedVoiceLocale)
        return;
    m_detectedVoiceLocale = newDetectedVoiceLocale;
    emit detectedVoiceLocaleChanged();
}

QLocale LlamaChatEngine::detectedVoiceLocale() const
{
    return m_detectedVoiceLocale;
}

void LlamaChatEngine::handleRecognizedText(const QString &text)
{
    // LlamaChatEngineの setUserInput を呼ぶ例
    qDebug() << "[LlamaChatEngine] recognized text => setUserInput:" << text;
    setUserInput(text);
}

//------------------------------------------------------------------------------
// modelDownloadInProgress Getter/Setter
//------------------------------------------------------------------------------
bool LlamaChatEngine::modelDownloadInProgress() const
{
    return mModelDownloadInProgress;
}

void LlamaChatEngine::setModelDownloadInProgress(bool newModelDownloadInProgress)
{
    if (mModelDownloadInProgress == newModelDownloadInProgress)
        return;
    mModelDownloadInProgress = newModelDownloadInProgress;
    emit modelDownloadInProgressChanged();
}

//------------------------------------------------------------------------------
// modelDownloadProgress Getter/Setter
//------------------------------------------------------------------------------
double LlamaChatEngine::modelDownloadProgress() const
{
    return mModelDownloadProgress;
}

void LlamaChatEngine::setModelDownloadProgress(double newModelDownloadProgress)
{
    if (qFuzzyCompare(mModelDownloadProgress, newModelDownloadProgress))
        return;
    mModelDownloadProgress = newModelDownloadProgress;
    emit modelDownloadProgressChanged();
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
// messages
// ChatMessageModelの参照を返す
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

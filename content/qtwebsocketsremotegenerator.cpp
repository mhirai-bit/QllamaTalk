#include "qtwebsocketsremotegenerator.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>

QtWebSocketsRemoteGenerator::QtWebSocketsRemoteGenerator(QObject *parent)
    : RemoteGeneratorInterface{parent},
    m_remoteInitialized(false)
{
    // WebSocket のオプションや初期設定
    // 例: m_webSocket.ignoreSslErrors() など
}

bool QtWebSocketsRemoteGenerator::setupRemoteConnection(const QUrl &url)
{
    // すでに接続されている場合はクローズして再接続するなどの対応
    if (m_webSocket.state() == QAbstractSocket::ConnectedState ||
        m_webSocket.state() == QAbstractSocket::ConnectingState) {
        qWarning() << "[QtWebSocketsRemoteGenerator] WebSocket is already connecting or connected. Closing it first...";
        m_webSocket.close();
    }

    // WebSocket のシグナルを接続
    // setupQObjectConnections() は「シグナルとこのクラスのメソッドをつなげる」などを想定
    setupQObjectConnections();

    qDebug() << "[QtWebSocketsRemoteGenerator] Attempting to connect to:" << url;
    m_webSocket.open(url);
    // → openすると、非同期で onConnected() / onError() のようなシグナルが飛んでくる

    return true;
}

void QtWebSocketsRemoteGenerator::generate(const QList<LlamaChatMessage> &messages)
{
    if (m_webSocket.state() != QAbstractSocket::ConnectedState) {
        qWarning() << "[QtWebSocketsRemoteGenerator] WebSocket not connected, can't generate.";
        return;
    }

    // 例: JSON 形式で、"action":"generate" とか "data":"..." を送る
    // QList<LlamaChatMessage> を JSON 配列に変換
    QJsonArray msgs;
    for (const auto &m : messages) {
        QJsonObject obj;
        obj["role"]    = m.role();
        obj["content"] = m.content();
        msgs.append(obj);
    }

    QJsonObject json;
    json["action"]   = QStringLiteral("generate");
    json["messages"] = msgs;

    // シリアライズして WebSocket で送る
    const auto jsonBytes = QJsonDocument(json).toJson(QJsonDocument::Compact);
    qDebug() << "[QtWebSocketsRemoteGenerator] Sending generate request:" << jsonBytes;
    m_webSocket.sendTextMessage(QString::fromUtf8(jsonBytes));
}

void QtWebSocketsRemoteGenerator::reinitEngine()
{
    if (m_webSocket.state() != QAbstractSocket::ConnectedState) {
        qWarning() << "[QtWebSocketsRemoteGenerator] WebSocket not connected, can't reinitEngine.";
        return;
    }

    // 例: JSONで "action":"reinit" などを送る
    QJsonObject json;
    json["action"] = QStringLiteral("reinit");

    const auto jsonBytes = QJsonDocument(json).toJson(QJsonDocument::Compact);
    qDebug() << "[QtWebSocketsRemoteGenerator] Sending reinitEngine request:" << jsonBytes;
    m_webSocket.sendTextMessage(QString::fromUtf8(jsonBytes));
}

bool QtWebSocketsRemoteGenerator::remoteInitialized() const
{
    return m_remoteInitialized;
}

void QtWebSocketsRemoteGenerator::setupQObjectConnections()
{
    // QtWebSocket のシグナルを受け取り、this のスロットに転送

    // 1) 接続完了
    connect(&m_webSocket, &QWebSocket::connected, this, [this](){
        qDebug() << "[QtWebSocketsRemoteGenerator] onConnected -> WebSocket connected.";
        // コネクション成功時、何か初期ハンドシェイクしたい場合などあればここで送信
        // 例: "action":"hello" を送るなど
        // あるいは、接続直後は remoteInitialized == false としておくなど

        // ここで例えば "fetchRemoteInitStatus" というリクエストを送るなど
        // QJsonObject request;
        // request["action"] = "fetchStatus";
        // m_webSocket.sendTextMessage(QJsonDocument(request).toJson(QJsonDocument::Compact));
    });

    // 2) 切断
    connect(&m_webSocket, &QWebSocket::disconnected, this, [this](){
        qDebug() << "[QtWebSocketsRemoteGenerator] onDisconnected -> WebSocket closed.";
        // remoteInitialized の状態をリセット
        setRemoteInitialized(false);
    });

    // 3) エラー
    connect(&m_webSocket, QOverload<QAbstractSocket::SocketError>::of(&QWebSocket::errorOccurred),
            this, [this](QAbstractSocket::SocketError error){
                qWarning() << "[QtWebSocketsRemoteGenerator] SocketError" << error << m_webSocket.errorString();
                setRemoteInitialized(false);
                // 必要なら generationError() シグナルを飛ばすなど
                emit generationError(m_webSocket.errorString());
            });

    // 4) メッセージ受信（テキスト）
    connect(&m_webSocket, &QWebSocket::textMessageReceived,
            this, [this](const QString &message) {
                qDebug() << "[QtWebSocketsRemoteGenerator] Received text message:" << message;

                // JSON として解析
                const QJsonDocument doc = QJsonDocument::fromJson(message.toUtf8());
                if (!doc.isObject()) {
                    qWarning() << "[QtWebSocketsRemoteGenerator] Received non-JSON or invalid JSON message!";
                    return;
                }
                QJsonObject obj = doc.object();

                // "action": "someValue"
                const QString action = obj.value(QStringLiteral("action")).toString();
                if (action == QLatin1String("partialResponse")) {
                    // 例えば "content": "xxx"
                    const QString content = obj.value(QStringLiteral("content")).toString();
                    // partialResponseReady(content) シグナルをエミット
                    emit partialResponseReady(content);

                } else if (action == QLatin1String("generationFinished")) {
                    // "content" に最終応答がある想定
                    const QString content = obj.value(QStringLiteral("content")).toString();
                    emit generationFinished(content);

                } else if (action == QLatin1String("error")) {
                    // "errorMessage" に何かが入っている
                    const QString errorMsg = obj.value(QStringLiteral("errorMessage")).toString();
                    emit generationError(errorMsg);

                } else if (action == QLatin1String("remoteInitializedChanged")) {
                    // "initialized": bool
                    const bool initState = obj.value(QStringLiteral("initialized")).toBool();
                    if (m_remoteInitialized != initState) {
                        setRemoteInitialized(initState);
                    }

                } else {
                    qDebug() << "[QtWebSocketsRemoteGenerator] Received unknown action:" << action;
                }
            });
}

void QtWebSocketsRemoteGenerator::setRemoteInitialized(bool remoteInitialized)
{
    if (m_remoteInitialized == remoteInitialized)
        return;

    m_remoteInitialized = remoteInitialized;
    emit remoteInitializedChanged(m_remoteInitialized);
}

#include "QtRemoteObjectsRemoteGenerator.h"

QtRemoteObjectsRemoteGenerator::QtRemoteObjectsRemoteGenerator(QObject *parent)
    : RemoteGeneratorInterface{parent}
    , mRemoteNode{nullptr}
    , mRemoteGenerator{nullptr}
{
}

bool QtRemoteObjectsRemoteGenerator::setupRemoteConnection(QUrl url)
{
    mRemoteNode = new QRemoteObjectNode(this);
    url.setScheme(QStringLiteral("tcp"));
    const bool result = mRemoteNode->connectToNode(url);
    if (!result) {
        qWarning() << "[QtRemoteObjectsRemoteGenerator] Could not connect to remote node at" << url;
        return false;
    }

    qDebug() << "[QtRemoteObjectsRemoteGenerator] Connected to remote node at" << url;

    mRemoteGenerator = mRemoteNode->acquire<LlamaResponseGeneratorReplica>();
    if (!mRemoteGenerator) {
        qWarning() << "[QtRemoteObjectsRemoteGenerator] Failed to acquire remote generator.";
        return false;
    }
    mRemoteGenerator->setParent(this);

    setupQObjectConnections();

    qDebug() << "[QtRemoteObjectsRemoteGenerator] Successfully acquired replica.";
    return true;
}

void QtRemoteObjectsRemoteGenerator::generate(const QList<LlamaChatMessage> &messages)
{
    if (!mRemoteGenerator) {
        qWarning() << "[QtRemoteObjectsRemoteGenerator] Remote generator not available.";
        return;
    }

    mRemoteGenerator->generate(messages);
}

void QtRemoteObjectsRemoteGenerator::reinitEngine()
{
    if (!mRemoteGenerator) {
        qWarning() << "[QtRemoteObjectsRemoteGenerator] Remote generator not available.";
        return;
    }

    mRemoteGenerator->reinitEngine();
}

bool QtRemoteObjectsRemoteGenerator::remoteInitialized() const
{
    if (!mRemoteGenerator) {
        return false;
    }
    mRemoteGenerator->remoteInitialized();
}

void QtRemoteObjectsRemoteGenerator::setupQObjectConnections()
{
    connect(mRemoteGenerator,
            &LlamaResponseGeneratorReplica::remoteInitializedChanged,
            this,
            &RemoteGeneratorInterface::remoteInitializedChanged);

    connect(mRemoteGenerator,
            &LlamaResponseGeneratorReplica::partialResponseReady,
            this,
            &QtRemoteObjectsRemoteGenerator::partialResponseReady);
    connect(mRemoteGenerator,
            &LlamaResponseGeneratorReplica::generationFinished,
            this,
            &QtRemoteObjectsRemoteGenerator::generationFinished);
    connect(mRemoteGenerator,
            &LlamaResponseGeneratorReplica::generationError,
            this,
            &QtRemoteObjectsRemoteGenerator::generationError);
}

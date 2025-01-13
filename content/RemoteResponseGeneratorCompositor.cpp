#include "RemoteResponseGeneratorCompositor.h"
#include "QtRemoteObjectsRemoteGenerator.h"
#include "QtWebSocketsRemoteGenerator.h"

RemoteResponseGeneratorCompositor::RemoteResponseGeneratorCompositor(QObject *parent)
    : RemoteGeneratorInterface{parent},
      // mRemoteGenerator{new QtRemoteObjectsRemoteGenerator{this}}
    mRemoteGenerator{new QtWebSocketsRemoteGenerator{this}}
{
    connect(mRemoteGenerator,
            &RemoteGeneratorInterface::partialResponseReady,
            this,
            &RemoteResponseGeneratorCompositor::partialResponseReady);
    connect(mRemoteGenerator,
            &RemoteGeneratorInterface::generationFinished,
            this,
            &RemoteResponseGeneratorCompositor::generationFinished);
    connect(mRemoteGenerator,
            &RemoteGeneratorInterface::generationError,
            this,
            &RemoteResponseGeneratorCompositor::generationError);
    connect(mRemoteGenerator,
            &RemoteGeneratorInterface::remoteInitializedChanged,
            this,
            &RemoteResponseGeneratorCompositor::remoteInitializedChanged);
}

bool RemoteResponseGeneratorCompositor::setupRemoteConnection(QUrl url)
{
    return mRemoteGenerator->setupRemoteConnection(url);
}

void RemoteResponseGeneratorCompositor::generate(const QList<LlamaChatMessage> &messages)
{
    mRemoteGenerator->generate(messages);
}

void RemoteResponseGeneratorCompositor::reinitEngine()
{
    mRemoteGenerator->reinitEngine();
}

bool RemoteResponseGeneratorCompositor::remoteInitialized() const
{
    return mRemoteGenerator->remoteInitialized();
}

void RemoteResponseGeneratorCompositor::setupQObjectConnections()
{
    // No-op
}

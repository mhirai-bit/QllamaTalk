#include "remoteresponsegeneratorcompositor.h"
#include "qtremoteobjectsremotegenerator.h"
#include "qtwebsocketsremotegenerator.h"

RemoteResponseGeneratorCompositor::RemoteResponseGeneratorCompositor(QObject *parent)
    : RemoteGeneratorInterface{parent},
      m_remoteGenerator{new QtRemoteObjectsRemoteGenerator{this}}
{
    connect(m_remoteGenerator,
            &RemoteGeneratorInterface::partialResponseReady,
            this,
            &RemoteResponseGeneratorCompositor::partialResponseReady);
    connect(m_remoteGenerator,
            &RemoteGeneratorInterface::generationFinished,
            this,
            &RemoteResponseGeneratorCompositor::generationFinished);
    connect(m_remoteGenerator,
            &RemoteGeneratorInterface::generationError,
            this,
            &RemoteResponseGeneratorCompositor::generationError);
    connect(m_remoteGenerator,
            &RemoteGeneratorInterface::remoteInitializedChanged,
            this,
            &RemoteResponseGeneratorCompositor::remoteInitializedChanged);
}

bool RemoteResponseGeneratorCompositor::setupRemoteConnection(const QUrl &url)
{
    return m_remoteGenerator->setupRemoteConnection(url);
}

void RemoteResponseGeneratorCompositor::generate(const QList<LlamaChatMessage> &messages)
{
    m_remoteGenerator->generate(messages);
}

void RemoteResponseGeneratorCompositor::reinitEngine()
{
    m_remoteGenerator->reinitEngine();
}

bool RemoteResponseGeneratorCompositor::remoteInitialized() const
{
    return m_remoteGenerator->remoteInitialized();
}

void RemoteResponseGeneratorCompositor::setupQObjectConnections()
{
    // No-op
}

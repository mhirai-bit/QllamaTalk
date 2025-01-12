#include "qtwebsocketsremotegenerator.h"

QtWebSocketsRemoteGenerator::QtWebSocketsRemoteGenerator(QObject *parent)
    : RemoteGeneratorInterface{parent}
{}

bool QtWebSocketsRemoteGenerator::setupRemoteConnection(const QUrl &url)
{

}

void QtWebSocketsRemoteGenerator::generate(const QList<LlamaChatMessage> &messages)
{

}

void QtWebSocketsRemoteGenerator::reinitEngine()
{

}

bool QtWebSocketsRemoteGenerator::remoteInitialized() const
{

}

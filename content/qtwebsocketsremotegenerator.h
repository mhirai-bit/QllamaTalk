#ifndef QTWEBSOCKETSREMOTEGENERATOR_H
#define QTWEBSOCKETSREMOTEGENERATOR_H

#include "RemoteGeneratorInterface.h"
#include "rep_LlamaResponseGenerator_replica.h"

class QtWebSocketsRemoteGenerator : public RemoteGeneratorInterface
{
    Q_OBJECT
public:
    explicit QtWebSocketsRemoteGenerator(QObject *parent = nullptr);

public slots:
    bool setupRemoteConnection(const QUrl& url) override;
    void generate(const QList<LlamaChatMessage>& messages) override;
    void reinitEngine() override;
    bool remoteInitialized() const override;
};

#endif // QTWEBSOCKETSREMOTEGENERATOR_H

#ifndef QTREMOTEOBJECTSREMOTEGENERATOR_H
#define QTREMOTEOBJECTSREMOTEGENERATOR_H

#include <QRemoteObjectNode>
#include "RemoteGeneratorInterface.h"
#include "rep_LlamaResponseGenerator_replica.h"

class QtRemoteObjectsRemoteGenerator : public RemoteGeneratorInterface
{
    Q_OBJECT
public:
    explicit QtRemoteObjectsRemoteGenerator(QObject *parent = nullptr);

public slots:
    bool setupRemoteConnection(const QUrl& url) override;
    void generate(const QList<LlamaChatMessage>& messages) override;
    void reinitEngine() override;
    bool remoteInitialized() const override;

private:
    LlamaResponseGeneratorReplica* mRemoteGenerator {nullptr};
    QRemoteObjectNode*             mRemoteNode      {nullptr};
};

#endif // QTREMOTEOBJECTSREMOTEGENERATOR_H

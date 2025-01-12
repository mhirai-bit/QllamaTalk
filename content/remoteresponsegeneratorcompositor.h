#ifndef REMOTERESPONSEGENERATORCOMPOSITOR_H
#define REMOTERESPONSEGENERATORCOMPOSITOR_H

#include <QObject>
#include "rep_LlamaResponseGenerator_replica.h"
#include "RemoteGeneratorInterface.h"

class RemoteResponseGeneratorCompositor : public RemoteGeneratorInterface
{
    Q_OBJECT
public:
    explicit RemoteResponseGeneratorCompositor(QObject *parent = nullptr);

public slots:
    void setupConnection(QUrl url) override;
    void generate(const QList<LlamaChatMessage>& messages) override;
    void reinitEngine() override;

private:
    RemoteGeneratorInterface* m_remoteGenerator {nullptr};
};

#endif // REMOTERESPONSEGENERATORCOMPOSITOR_H

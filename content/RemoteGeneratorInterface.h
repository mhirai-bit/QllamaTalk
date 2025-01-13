#ifndef REMOTEGENERATORINTERFACE_H
#define REMOTEGENERATORINTERFACE_H

#include <QObject>
#include "rep_LlamaResponseGenerator_replica.h"

class RemoteGeneratorInterface: public QObject {
    Q_OBJECT
public:
    explicit RemoteGeneratorInterface(QObject* parent = nullptr) : QObject(parent) {}
    virtual ~RemoteGeneratorInterface() = default;

public slots:
    virtual bool setupRemoteConnection(QUrl url) = 0;
    virtual void generate(const QList<LlamaChatMessage>& messages) = 0;
    virtual void reinitEngine() = 0;
    virtual bool remoteInitialized() const = 0;

protected:
    virtual void setupQObjectConnections() = 0;

signals:
    void partialResponseReady(const QString &textSoFar);
    void generationFinished(const QString &finalResponse);
    void generationError(const QString &errorMessage);
    void remoteInitializedChanged(bool remoteInitialized);
};

#endif // REMOTEGENERATORINTERFACE_H

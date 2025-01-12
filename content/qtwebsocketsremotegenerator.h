#ifndef QTWEBSOCKETSREMOTEGENERATOR_H
#define QTWEBSOCKETSREMOTEGENERATOR_H

#include "RemoteGeneratorInterface.h"

class QtWebSocketsRemoteGenerator : public RemoteGeneratorInterface
{
    Q_OBJECT
public:
    explicit QtWebSocketsRemoteGenerator(QObject *parent = nullptr);

signals:
};

#endif // QTWEBSOCKETSREMOTEGENERATOR_H

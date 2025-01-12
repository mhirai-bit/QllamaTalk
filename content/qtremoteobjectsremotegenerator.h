#ifndef QTREMOTEOBJECTSREMOTEGENERATOR_H
#define QTREMOTEOBJECTSREMOTEGENERATOR_H

#include "RemoteGeneratorInterface.h"

class QtRemoteObjectsRemoteGenerator : public RemoteGeneratorInterface
{
    Q_OBJECT
public:
    explicit QtRemoteObjectsRemoteGenerator(QObject *parent = nullptr);

signals:
};

#endif // QTREMOTEOBJECTSREMOTEGENERATOR_H

#pragma once

#include "network/clientsession.h"

#include <QObject>

class ServiceRegistry;

class MessageDispatcher final : public QObject
{
    Q_OBJECT

public:
    explicit MessageDispatcher(ServiceRegistry *services, QObject *parent = nullptr);

public slots:
    void dispatch(ClientSession *session, const Charging::Message &message);

private:
    ServiceRegistry *m_services = nullptr;
};

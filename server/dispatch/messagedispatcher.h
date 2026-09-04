#pragma once

#include "network/clientsession.h"

#include <QObject>
#include <QThreadPool>

class ServiceRegistry;

class MessageDispatcher final : public QObject
{
    Q_OBJECT

public:
    explicit MessageDispatcher(ServiceRegistry *services, QObject *parent = nullptr);
    ~MessageDispatcher() override;

public slots:
    void dispatch(ClientSession *session, const Charging::Message &message);

private:
    void handleUserLogin(ClientSession *session, const Charging::Message &message);
    void handleAdminLogin(ClientSession *session, const Charging::Message &message);
    void handleUserProfile(ClientSession *session, const Charging::Message &message);
    void sendError(ClientSession *session, Charging::MessageType type, quint32 requestId,
                   Charging::ErrorCode error, const QString &detail = {});

    ServiceRegistry *m_services = nullptr;
    QThreadPool m_workerPool;
};

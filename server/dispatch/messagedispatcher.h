#pragma once

#include "network/clientsession.h"

#include <QObject>
#include <QThreadPool>
#include <functional>

class ServiceRegistry;

class MessageDispatcher final : public QObject
{
    Q_OBJECT

public:
    explicit MessageDispatcher(ServiceRegistry *services, QObject *parent = nullptr,
                               int requestTimeoutMilliseconds = 10'000);
    ~MessageDispatcher() override;

public slots:
    void dispatch(ClientSession *session, const Charging::Message &message);

private:
    struct RequestResult;
    void executeAsync(ClientSession *session, Charging::MessageType responseType,
                      quint32 requestId, std::function<RequestResult()> operation,
                      Charging::Role loginRole = Charging::Role::Anonymous);
    void sendError(ClientSession *session, Charging::MessageType type, quint32 requestId,
                   Charging::ErrorCode error, const QString &detail = {});

    ServiceRegistry *m_services = nullptr;
    QThreadPool m_workerPool;
    int m_requestTimeoutMilliseconds;
};

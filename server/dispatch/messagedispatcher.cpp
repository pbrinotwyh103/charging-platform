#include "dispatch/messagedispatcher.h"

#include "services/serviceregistry.h"

#include <QJsonObject>

MessageDispatcher::MessageDispatcher(ServiceRegistry *services, QObject *parent)
    : QObject(parent), m_services(services)
{
}

void MessageDispatcher::dispatch(ClientSession *session, const Charging::Message &message)
{
    Q_UNUSED(m_services)
    if (message.header.messageType == Charging::MessageType::Ping) {
        session->send(Charging::MessageType::Pong,
                      message.header.requestId,
                      {{QStringLiteral("server"), QStringLiteral("charging_server")}});
        return;
    }

    session->send(message.header.messageType,
                  message.header.requestId,
                  {{QStringLiteral("message"), QStringLiteral("该业务处理器将在后续模块实现")}},
                  Charging::ErrorCode::UnsupportedMessage);
}

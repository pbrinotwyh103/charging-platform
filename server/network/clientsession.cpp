#include "network/clientsession.h"

#include "protocol/packetcodec.h"

#include <QDateTime>
#include <QUuid>

ClientSession::ClientSession(QTcpSocket *socket, QObject *parent)
    : QObject(parent), m_socket(socket)
{
    m_socket->setParent(this);
    touch();
    connect(m_socket, &QTcpSocket::readyRead, this, &ClientSession::onReadyRead);
    connect(m_socket, &QTcpSocket::disconnected, this, [this] {
        emit closed(this);
    });
}

QString ClientSession::peerDescription() const
{
    return QStringLiteral("%1:%2").arg(m_socket->peerAddress().toString())
        .arg(m_socket->peerPort());
}

bool ClientSession::send(Charging::MessageType type,
                         quint32 requestId,
                         const QJsonObject &payload,
                         Charging::ErrorCode status)
{
    if (m_socket->state() == QAbstractSocket::ConnectedState) {
        auto packet = Charging::PacketCodec::encode(type, requestId, payload, status);
        // Check the final UTF-8 envelope, including dispatcher-added fields and
        // JSON escaping, at the common exit used by responses and all pushes.
        if (packet.size() - Charging::MessageHeader::SerializedSize
            > Charging::MessageHeader::MaxPayloadLength) {
            switch (type) {
            case Charging::MessageType::ChargingProgressPush:
            case Charging::MessageType::ChargingStoppedPush:
            case Charging::MessageType::AlarmPush:
            case Charging::MessageType::DeviceStatusPush:
                return false; // ServerApplication records this failed delivery.
            default:
                // Never echo any field from the oversized payload or recurse
                // through send: this fixed error envelope is bounded itself.
                packet = Charging::PacketCodec::encode(type, requestId,
                    {{QStringLiteral("message"), QStringLiteral("响应内容超过通信限制")},
                     {QStringLiteral("reason"), QStringLiteral("response_too_large")}},
                    Charging::ErrorCode::InternalError);
                break;
            }
        }
        return m_socket->write(packet) == packet.size();
    }
    return false;
}

void ClientSession::close()
{
    m_socket->disconnectFromHost();
    if (m_socket->state() != QAbstractSocket::UnconnectedState) {
        m_socket->abort();
    }
}

void ClientSession::touch()
{
    m_lastActivityAt = QDateTime::currentMSecsSinceEpoch();
}

qint64 ClientSession::lastActivityAt() const
{
    return m_lastActivityAt;
}

bool ClientSession::isAuthenticated() const
{
    return m_role != Charging::Role::Anonymous && m_principalId > 0;
}

Charging::Role ClientSession::role() const
{
    return m_role;
}

qint64 ClientSession::principalId() const
{
    return m_principalId;
}

QString ClientSession::identity() const
{
    return m_identity;
}

QString ClientSession::sessionId() const
{
    return m_sessionId;
}

quint64 ClientSession::authenticationGeneration() const
{
    return m_authenticationGeneration;
}

void ClientSession::authenticate(Charging::Role role, qint64 principalId,
                                 const QString &identity)
{
    ++m_authenticationGeneration;
    m_role = role;
    m_principalId = principalId;
    m_identity = identity;
    m_sessionId = QUuid::createUuid().toString(QUuid::WithoutBraces);
}

void ClientSession::clearAuthentication()
{
    // Even anonymous -> anonymous invalidates every earlier login attempt.
    ++m_authenticationGeneration;
    m_role = Charging::Role::Anonymous;
    m_principalId = 0;
    m_identity.clear();
    m_sessionId.clear();
}

bool ClientSession::markRequestStarted(quint32 requestId)
{
    if (requestId == 0 || m_inFlightRequests.contains(requestId)) {
        return false;
    }
    m_inFlightRequests.insert(requestId);
    return true;
}

void ClientSession::finishRequest(quint32 requestId)
{
    m_inFlightRequests.remove(requestId);
}

bool ClientSession::allowLoginAttempt(qint64 now)
{
    while (!m_failedLoginTimes.isEmpty() && now - m_failedLoginTimes.first() > 60'000) {
        m_failedLoginTimes.removeFirst();
    }
    return m_failedLoginTimes.size() < 5;
}

void ClientSession::registerLoginFailure(qint64 now)
{
    m_failedLoginTimes.append(now);
}

void ClientSession::clearLoginFailures()
{
    m_failedLoginTimes.clear();
}

void ClientSession::onReadyRead()
{
    m_receiveBuffer.append(m_socket->readAll());
    while (!m_receiveBuffer.isEmpty()) {
        const Charging::DecodeResult result = Charging::PacketCodec::tryDecode(m_receiveBuffer);
        if (result.status == Charging::DecodeStatus::NeedMoreData) {
            break;
        }
        if (result.status == Charging::DecodeStatus::Invalid) {
            emit protocolError(this, result.error);
            m_socket->disconnectFromHost();
            break;
        }
        touch();
        emit messageReceived(this, result.message);
    }
}

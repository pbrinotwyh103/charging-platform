#include "network/clientsession.h"

#include "protocol/packetcodec.h"

ClientSession::ClientSession(QTcpSocket *socket, QObject *parent)
    : QObject(parent), m_socket(socket)
{
    m_socket->setParent(this);
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

void ClientSession::send(Charging::MessageType type,
                         quint32 requestId,
                         const QJsonObject &payload,
                         Charging::ErrorCode status)
{
    m_socket->write(Charging::PacketCodec::encode(type, requestId, payload, status));
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
        emit messageReceived(this, result.message);
    }
}

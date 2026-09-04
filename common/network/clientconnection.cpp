#include "network/clientconnection.h"

#include "protocol/packetcodec.h"

namespace Charging {

ClientConnection::ClientConnection(QObject *parent)
    : QObject(parent)
{
    connect(&m_socket, &QTcpSocket::connected, this, &ClientConnection::connected);
    connect(&m_socket, &QTcpSocket::disconnected, this, &ClientConnection::disconnected);
    connect(&m_socket, &QTcpSocket::readyRead, this, &ClientConnection::onReadyRead);
    connect(&m_socket, &QTcpSocket::errorOccurred, this, [this](QAbstractSocket::SocketError) {
        emit socketError(m_socket.errorString());
    });
}

void ClientConnection::connectToServer(const QString &host, quint16 port)
{
    m_receiveBuffer.clear();
    m_socket.abort();
    m_socket.connectToHost(host, port);
}

void ClientConnection::disconnectFromServer()
{
    m_socket.disconnectFromHost();
}

void ClientConnection::send(MessageType type, quint32 requestId, const QJsonObject &payload)
{
    if (!isConnected()) {
        emit socketError(QStringLiteral("尚未连接服务器"));
        return;
    }
    m_socket.write(PacketCodec::encode(type, requestId, payload));
}

bool ClientConnection::isConnected() const
{
    return m_socket.state() == QAbstractSocket::ConnectedState;
}

void ClientConnection::onReadyRead()
{
    m_receiveBuffer.append(m_socket.readAll());
    while (!m_receiveBuffer.isEmpty()) {
        const DecodeResult result = PacketCodec::tryDecode(m_receiveBuffer);
        if (result.status == DecodeStatus::NeedMoreData) {
            break;
        }
        if (result.status == DecodeStatus::Invalid) {
            emit protocolError(result.error);
            break;
        }
        emit messageReceived(result.message);
    }
}

} // namespace Charging

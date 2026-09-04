#include "network/clientconnection.h"

#include "protocol/packetcodec.h"

#include <QDateTime>
#include <QtGlobal>

namespace Charging {

ClientConnection::ClientConnection(QObject *parent)
    : QObject(parent)
{
    m_reconnectTimer.setSingleShot(true);
    m_heartbeatTimer.setInterval(10'000);
    m_healthTimer.setInterval(5'000);

    connect(&m_socket, &QTcpSocket::connected, this, [this] {
        m_reconnectAttempt = 0;
        m_lastReceivedAt = QDateTime::currentMSecsSinceEpoch();
        m_heartbeatTimer.start();
        m_healthTimer.start();
        emit connected();
    });
    connect(&m_socket, &QTcpSocket::disconnected, this, [this] {
        m_heartbeatTimer.stop();
        m_healthTimer.stop();
        emit disconnected();
        scheduleReconnect();
    });
    connect(&m_socket, &QTcpSocket::readyRead, this, &ClientConnection::onReadyRead);
    connect(&m_socket, &QTcpSocket::errorOccurred, this, [this](QAbstractSocket::SocketError) {
        emit socketError(m_socket.errorString());
    });
    connect(&m_reconnectTimer, &QTimer::timeout, this, [this] {
        if (!m_manualDisconnect && !m_host.isEmpty() && m_port != 0) {
            m_socket.abort();
            m_socket.connectToHost(m_host, m_port);
        }
    });
    connect(&m_heartbeatTimer, &QTimer::timeout,
            this, &ClientConnection::sendHeartbeat);
    connect(&m_healthTimer, &QTimer::timeout,
            this, &ClientConnection::checkConnectionHealth);
}

void ClientConnection::connectToServer(const QString &host, quint16 port)
{
    m_host = host.trimmed();
    m_port = port;
    m_manualDisconnect = false;
    m_receiveBuffer.clear();
    m_reconnectTimer.stop();
    m_socket.abort();
    m_socket.connectToHost(m_host, m_port);
}

void ClientConnection::disconnectFromServer()
{
    m_manualDisconnect = true;
    m_reconnectTimer.stop();
    m_heartbeatTimer.stop();
    m_healthTimer.stop();
    m_socket.disconnectFromHost();
}

bool ClientConnection::send(MessageType type, quint32 requestId, const QJsonObject &payload)
{
    if (!isConnected()) {
        emit socketError(QStringLiteral("尚未连接服务器"));
        return false;
    }
    return m_socket.write(PacketCodec::encode(type, requestId, payload)) >= 0;
}

bool ClientConnection::isConnected() const
{
    return m_socket.state() == QAbstractSocket::ConnectedState;
}

quint32 ClientConnection::nextRequestId()
{
    if (m_nextRequestId == 0) {
        m_nextRequestId = 1;
    }
    return m_nextRequestId++;
}

void ClientConnection::setAutoReconnect(bool enabled)
{
    m_autoReconnect = enabled;
    if (!enabled) {
        m_reconnectTimer.stop();
    }
}

void ClientConnection::setClientName(const QString &name)
{
    if (!name.trimmed().isEmpty()) {
        m_clientName = name.trimmed();
    }
}

void ClientConnection::onReadyRead()
{
    m_receiveBuffer.append(m_socket.readAll());
    m_lastReceivedAt = QDateTime::currentMSecsSinceEpoch();
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

void ClientConnection::scheduleReconnect()
{
    if (!m_autoReconnect || m_manualDisconnect || m_host.isEmpty() || m_port == 0
        || m_reconnectTimer.isActive()) {
        return;
    }
    ++m_reconnectAttempt;
    const int delay = qMin(30'000, 1'000 * (1 << qMin(m_reconnectAttempt - 1, 5)));
    emit reconnectScheduled(m_reconnectAttempt, delay);
    m_reconnectTimer.start(delay);
}

void ClientConnection::sendHeartbeat()
{
    send(MessageType::Ping, nextRequestId(),
         {{QStringLiteral("client"), m_clientName},
          {QStringLiteral("sentAt"), QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)}});
}

void ClientConnection::checkConnectionHealth()
{
    if (!isConnected()) {
        return;
    }
    const qint64 silence = QDateTime::currentMSecsSinceEpoch() - m_lastReceivedAt;
    if (silence > 35'000) {
        emit socketError(QStringLiteral("服务器心跳超时，正在重新连接"));
        m_socket.abort();
    }
}

} // namespace Charging

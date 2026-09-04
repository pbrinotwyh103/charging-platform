#pragma once

#include "protocol/message.h"

#include <QObject>
#include <QTcpSocket>
#include <QTimer>

namespace Charging {

class ClientConnection final : public QObject
{
    Q_OBJECT

public:
    explicit ClientConnection(QObject *parent = nullptr);

    void connectToServer(const QString &host, quint16 port);
    void disconnectFromServer();
    bool send(MessageType type, quint32 requestId, const QJsonObject &payload = {});
    bool isConnected() const;
    quint32 nextRequestId();
    void setAutoReconnect(bool enabled);
    void setClientName(const QString &name);

signals:
    void connected();
    void disconnected();
    void messageReceived(const Charging::Message &message);
    void protocolError(const QString &message);
    void socketError(const QString &message);
    void reconnectScheduled(int attempt, int delayMilliseconds);

private slots:
    void onReadyRead();
    void scheduleReconnect();
    void sendHeartbeat();
    void checkConnectionHealth();

private:
    QTcpSocket m_socket;
    QByteArray m_receiveBuffer;
    QString m_host;
    QString m_clientName = QStringLiteral("qt-client");
    quint16 m_port = 0;
    quint32 m_nextRequestId = 1;
    bool m_autoReconnect = true;
    bool m_manualDisconnect = false;
    int m_reconnectAttempt = 0;
    qint64 m_lastReceivedAt = 0;
    QTimer m_reconnectTimer;
    QTimer m_heartbeatTimer;
    QTimer m_healthTimer;
};

} // namespace Charging

Q_DECLARE_METATYPE(Charging::Message)

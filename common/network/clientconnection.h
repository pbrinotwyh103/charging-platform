#pragma once

#include "protocol/message.h"

#include <QObject>
#include <QTcpSocket>

namespace Charging {

class ClientConnection final : public QObject
{
    Q_OBJECT

public:
    explicit ClientConnection(QObject *parent = nullptr);

    void connectToServer(const QString &host, quint16 port);
    void disconnectFromServer();
    void send(MessageType type, quint32 requestId, const QJsonObject &payload = {});
    bool isConnected() const;

signals:
    void connected();
    void disconnected();
    void messageReceived(const Charging::Message &message);
    void protocolError(const QString &message);
    void socketError(const QString &message);

private slots:
    void onReadyRead();

private:
    QTcpSocket m_socket;
    QByteArray m_receiveBuffer;
};

} // namespace Charging

Q_DECLARE_METATYPE(Charging::Message)

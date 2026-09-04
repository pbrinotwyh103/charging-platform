#pragma once

#include "protocol/message.h"

#include <QObject>
#include <QTcpSocket>

class ClientSession final : public QObject
{
    Q_OBJECT

public:
    explicit ClientSession(QTcpSocket *socket, QObject *parent = nullptr);
    QString peerDescription() const;
    void send(Charging::MessageType type,
              quint32 requestId,
              const QJsonObject &payload = {},
              Charging::ErrorCode status = Charging::ErrorCode::Success);

signals:
    void messageReceived(ClientSession *session, const Charging::Message &message);
    void closed(ClientSession *session);
    void protocolError(ClientSession *session, const QString &error);

private slots:
    void onReadyRead();

private:
    QTcpSocket *m_socket = nullptr;
    QByteArray m_receiveBuffer;
};

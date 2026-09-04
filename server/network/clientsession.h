#pragma once

#include "protocol/message.h"
#include "models/role.h"

#include <QList>
#include <QObject>
#include <QSet>
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
    void close();
    void touch();
    qint64 lastActivityAt() const;

    bool isAuthenticated() const;
    Charging::Role role() const;
    qint64 principalId() const;
    QString identity() const;
    QString sessionId() const;
    void authenticate(Charging::Role role, qint64 principalId, const QString &identity);
    void clearAuthentication();

    bool markRequestStarted(quint32 requestId);
    void finishRequest(quint32 requestId);
    bool allowLoginAttempt(qint64 now);
    void registerLoginFailure(qint64 now);
    void clearLoginFailures();

signals:
    void messageReceived(ClientSession *session, const Charging::Message &message);
    void closed(ClientSession *session);
    void protocolError(ClientSession *session, const QString &error);

private slots:
    void onReadyRead();

private:
    QTcpSocket *m_socket = nullptr;
    QByteArray m_receiveBuffer;
    Charging::Role m_role = Charging::Role::Anonymous;
    qint64 m_principalId = 0;
    QString m_identity;
    QString m_sessionId;
    qint64 m_lastActivityAt = 0;
    QSet<quint32> m_inFlightRequests;
    QList<qint64> m_failedLoginTimes;
};

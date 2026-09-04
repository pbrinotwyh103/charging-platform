#pragma once

#include "network/clientsession.h"

#include <QObject>
#include <QSet>
#include <QTcpServer>

class TcpServer final : public QObject
{
    Q_OBJECT

public:
    explicit TcpServer(QObject *parent = nullptr);
    bool listen(quint16 port, QString *error);
    int connectionCount() const;
    quint16 listeningPort() const;
    void closeExpiredSessions(qint64 timeoutMilliseconds);

signals:
    void messageReceived(ClientSession *session, const Charging::Message &message);

private slots:
    void onNewConnection();

private:
    QTcpServer m_server;
    QSet<ClientSession *> m_sessions;
};

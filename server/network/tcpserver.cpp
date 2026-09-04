#include "network/tcpserver.h"

#include <QDebug>

TcpServer::TcpServer(QObject *parent)
    : QObject(parent)
{
    connect(&m_server, &QTcpServer::newConnection, this, &TcpServer::onNewConnection);
}

bool TcpServer::listen(quint16 port, QString *error)
{
    if (m_server.listen(QHostAddress::Any, port)) {
        return true;
    }
    if (error) *error = m_server.errorString();
    return false;
}

int TcpServer::connectionCount() const
{
    return m_sessions.size();
}

void TcpServer::onNewConnection()
{
    while (m_server.hasPendingConnections()) {
        auto *session = new ClientSession(m_server.nextPendingConnection(), this);
        m_sessions.insert(session);
        qInfo().noquote() << QStringLiteral("客户端已连接：%1").arg(session->peerDescription());
        connect(session, &ClientSession::messageReceived,
                this, &TcpServer::messageReceived);
        connect(session, &ClientSession::protocolError, this,
                [](ClientSession *source, const QString &error) {
            qWarning().noquote() << QStringLiteral("协议错误 %1：%2")
                                    .arg(source->peerDescription(), error);
        });
        connect(session, &ClientSession::closed, this, [this](ClientSession *closed) {
            qInfo().noquote() << QStringLiteral("客户端已断开：%1").arg(closed->peerDescription());
            m_sessions.remove(closed);
            closed->deleteLater();
        });
    }
}

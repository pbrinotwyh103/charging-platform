#include "app/serverapplication.h"

#include <QDebug>

ServerApplication::ServerApplication(QObject *parent)
    : QObject(parent),
      m_dispatcher(&m_services, this),
      m_server(this),
      m_jobs(this)
{
    connect(&m_server, &TcpServer::messageReceived,
            &m_dispatcher, &MessageDispatcher::dispatch);
    connect(&m_jobs, &JobManager::heartbeatTick, this, [this] {
        m_server.closeExpiredSessions(45'000);
    });
}

bool ServerApplication::start(quint16 port, const QString &databasePath, QString *error)
{
    if (!m_database.open(databasePath, error)) {
        return false;
    }
    if (!m_services.initialize(&m_database, error)) {
        return false;
    }
    if (!m_server.listen(port, error)) {
        return false;
    }
    m_jobs.start();
    qInfo().noquote() << QStringLiteral("服务器框架已启动，端口=%1，数据库=%2")
                         .arg(m_server.listeningPort()).arg(databasePath);
    return true;
}

quint16 ServerApplication::listeningPort() const
{
    return m_server.listeningPort();
}

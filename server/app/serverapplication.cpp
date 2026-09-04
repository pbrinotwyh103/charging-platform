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
}

bool ServerApplication::start(quint16 port, const QString &databasePath, QString *error)
{
    if (!m_database.open(databasePath, error)) {
        return false;
    }
    m_services.initialize(&m_database);
    if (!m_server.listen(port, error)) {
        return false;
    }
    m_jobs.start();
    qInfo().noquote() << QStringLiteral("服务器框架已启动，端口=%1，数据库=%2")
                         .arg(port).arg(databasePath);
    return true;
}

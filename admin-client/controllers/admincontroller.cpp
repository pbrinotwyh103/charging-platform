#include "controllers/admincontroller.h"

AdminController::AdminController(QObject *parent)
    : QObject(parent)
{
    connect(&m_connection, &Charging::ClientConnection::connected, this, [this] {
        emit statusTextChanged(QStringLiteral("已连接后台服务器"), true);
    });
    connect(&m_connection, &Charging::ClientConnection::disconnected, this, [this] {
        emit statusTextChanged(QStringLiteral("后台服务器连接已断开"), false);
    });
    connect(&m_connection, &Charging::ClientConnection::socketError, this,
            [this](const QString &message) {
        emit statusTextChanged(QStringLiteral("连接失败：%1").arg(message), false);
    });
}

void AdminController::connectToServer(const QString &host, quint16 port)
{
    emit statusTextChanged(QStringLiteral("正在连接后台服务器…"), false);
    m_connection.connectToServer(host, port);
}

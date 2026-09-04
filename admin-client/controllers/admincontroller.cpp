#include "controllers/admincontroller.h"

#include "protocol/errorcodes.h"

AdminController::AdminController(QObject *parent)
    : QObject(parent)
{
    m_connection.setClientName(QStringLiteral("admin-client"));
    connect(&m_connection, &Charging::ClientConnection::connected, this, [this] {
        emit statusTextChanged(QStringLiteral("已连接后台服务器"), true);
        sendPendingLogin();
    });
    connect(&m_connection, &Charging::ClientConnection::disconnected, this, [this] {
        emit statusTextChanged(QStringLiteral("后台服务器连接已断开，等待自动重连"), false);
        if (m_loggedIn) {
            m_loggedIn = false;
            emit loginFailed(QStringLiteral("连接中断，请重新输入管理员密码登录"));
            emit loggedOut();
        }
    });
    connect(&m_connection, &Charging::ClientConnection::socketError, this,
            [this](const QString &message) {
        emit statusTextChanged(QStringLiteral("连接失败：%1").arg(message), false);
    });
    connect(&m_connection, &Charging::ClientConnection::reconnectScheduled, this,
            [this](int attempt, int delay) {
        emit statusTextChanged(QStringLiteral("第%1次重连将在%2秒后进行")
                                   .arg(attempt).arg(delay / 1000.0, 0, 'f', 1), false);
    });
    connect(&m_connection, &Charging::ClientConnection::messageReceived, this,
            [this](const Charging::Message &message) {
        if (message.header.messageType == Charging::MessageType::AdminLoginResponse
            && message.header.requestId == m_loginRequestId) {
            emit loginBusyChanged(false);
            m_pendingPassword.clear();
            if (message.header.statusCode != Charging::ErrorCode::Success) {
                m_loggedIn = false;
                emit loginFailed(Charging::errorMessage(
                    message.header.statusCode,
                    message.payload.value(QStringLiteral("message")).toString()));
                return;
            }
            m_loggedIn = true;
            m_pendingUsername.clear();
            emit loginSucceeded(message.payload);
        } else if (message.header.messageType == Charging::MessageType::LogoutResponse) {
            m_loggedIn = false;
            emit loggedOut();
        }
    });
}

void AdminController::connectToServer(const QString &host, quint16 port)
{
    emit statusTextChanged(QStringLiteral("正在连接后台服务器…"), false);
    m_connection.connectToServer(host, port);
}

void AdminController::login(const QString &username, const QString &password,
                            const QString &host, quint16 port)
{
    m_pendingUsername = username.trimmed();
    m_pendingPassword = password;
    emit loginBusyChanged(true);
    if (m_connection.isConnected()) {
        sendPendingLogin();
    } else {
        connectToServer(host, port);
    }
}

void AdminController::logout()
{
    if (m_connection.isConnected() && m_loggedIn) {
        m_connection.send(Charging::MessageType::LogoutRequest,
                          m_connection.nextRequestId());
        return;
    }
    m_loggedIn = false;
    emit loggedOut();
}

void AdminController::sendPendingLogin()
{
    if (m_pendingUsername.isEmpty() || m_pendingPassword.isEmpty()
        || !m_connection.isConnected()) return;
    m_loginRequestId = m_connection.nextRequestId();
    if (!m_connection.send(Charging::MessageType::AdminLoginRequest,
                           m_loginRequestId,
                           {{QStringLiteral("username"), m_pendingUsername},
                            {QStringLiteral("password"), m_pendingPassword}})) {
        emit loginBusyChanged(false);
    }
}

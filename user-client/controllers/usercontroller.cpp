#include "controllers/usercontroller.h"

#include "protocol/errorcodes.h"

UserController::UserController(QObject *parent)
    : QObject(parent)
{
    m_connection.setClientName(QStringLiteral("user-client"));
    connect(&m_connection, &Charging::ClientConnection::connected, this, [this] {
        emit statusTextChanged(QStringLiteral("已连接服务器"), true);
        sendPendingLogin();
    });
    connect(&m_connection, &Charging::ClientConnection::disconnected, this, [this] {
        emit statusTextChanged(QStringLiteral("服务器连接已断开，等待自动重连"), false);
        if (m_loggedIn) {
            m_pendingPhone = m_currentPhone;
            emit loginBusyChanged(true);
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
        if (message.header.messageType == Charging::MessageType::UserLoginResponse
            && message.header.requestId == m_loginRequestId) {
            emit loginBusyChanged(false);
            if (message.header.statusCode != Charging::ErrorCode::Success) {
                m_loggedIn = false;
                m_pendingPhone.clear();
                emit loginFailed(Charging::errorMessage(
                    message.header.statusCode,
                    message.payload.value(QStringLiteral("message")).toString()));
                return;
            }
            m_loggedIn = true;
            m_currentPhone = message.payload.value(QStringLiteral("phone")).toString();
            m_pendingPhone.clear();
            emit loginSucceeded(message.payload);
        } else if (message.header.messageType == Charging::MessageType::LogoutResponse) {
            m_loggedIn = false;
            m_currentPhone.clear();
            m_pendingPhone.clear();
            emit loggedOut();
        }
    });
}

void UserController::connectToServer(const QString &host, quint16 port)
{
    emit statusTextChanged(QStringLiteral("正在连接服务器…"), false);
    m_connection.connectToServer(host, port);
}

void UserController::login(const QString &phone, const QString &host, quint16 port)
{
    m_pendingPhone = phone.trimmed();
    emit loginBusyChanged(true);
    if (m_connection.isConnected()) {
        sendPendingLogin();
    } else {
        connectToServer(host, port);
    }
}

void UserController::logout()
{
    if (m_connection.isConnected() && m_loggedIn) {
        m_connection.send(Charging::MessageType::LogoutRequest,
                          m_connection.nextRequestId());
        return;
    }
    m_loggedIn = false;
    m_currentPhone.clear();
    m_pendingPhone.clear();
    emit loggedOut();
}

void UserController::sendPendingLogin()
{
    if (m_pendingPhone.isEmpty() || !m_connection.isConnected()) return;
    m_loginRequestId = m_connection.nextRequestId();
    if (!m_connection.send(Charging::MessageType::UserLoginRequest,
                           m_loginRequestId,
                           {{QStringLiteral("phone"), m_pendingPhone}})) {
        emit loginBusyChanged(false);
    }
}

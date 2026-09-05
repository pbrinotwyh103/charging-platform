#include "controllers/admincontroller.h"

#include "protocol/errorcodes.h"

#include <QDateTime>

AdminController::AdminController(QObject *parent)
    : QObject(parent)
{
    m_connection.setClientName(QStringLiteral("admin-client"));
    m_requestTimeoutTimer.setInterval(250);
    connect(&m_requestTimeoutTimer, &QTimer::timeout,
            this, &AdminController::expireRequests);
    m_requestTimeoutTimer.start();

    connect(&m_connection, &Charging::ClientConnection::connected, this, [this] {
        emit statusTextChanged(QStringLiteral("已连接后台服务器"), true);
        sendPendingLogin();
    });
    connect(&m_connection, &Charging::ClientConnection::disconnected, this, [this] {
        emit statusTextChanged(QStringLiteral("后台服务器连接已断开，等待自动重连"), false);
        failPendingRequests(QStringLiteral("连接已断开，请稍后重试"),
                            Charging::ErrorCode::NetworkUnavailable);
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
            this, &AdminController::handleMessage);
}

bool AdminController::isLoggedIn() const
{
    return m_loggedIn;
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
    failPendingRequests(QStringLiteral("管理员已退出登录"),
                        Charging::ErrorCode::SessionExpired);
    if (m_connection.isConnected() && m_loggedIn) {
        m_connection.send(Charging::MessageType::LogoutRequest,
                          m_connection.nextRequestId());
        return;
    }
    m_loggedIn = false;
    emit loggedOut();
}

void AdminController::requestAdminCommand(const QString &action,
                                          const QJsonObject &parameters)
{
    const QString normalizedAction = action.trimmed();
    if (normalizedAction.isEmpty()) {
        emit commandFailed(action, QStringLiteral("管理员请求动作不能为空"),
                           static_cast<int>(Charging::ErrorCode::ValidationFailed));
        return;
    }
    if (!m_loggedIn) {
        emit commandFailed(normalizedAction, QStringLiteral("管理员登录已失效，请重新登录"),
                           static_cast<int>(Charging::ErrorCode::Unauthorized));
        return;
    }
    if (!m_connection.isConnected()) {
        emit commandFailed(normalizedAction, QStringLiteral("服务器连接不可用，请稍后重试"),
                           static_cast<int>(Charging::ErrorCode::NetworkUnavailable));
        return;
    }

    const auto previous = m_latestRequestByAction.constFind(normalizedAction);
    if (previous != m_latestRequestByAction.cend()) {
        m_pendingRequests.remove(previous.value());
    }

    const quint32 requestId = m_connection.nextRequestId();
    QJsonObject payload = parameters;
    payload.insert(QStringLiteral("action"), normalizedAction);
    if (!m_connection.send(Charging::MessageType::AdminCommandRequest,
                           requestId, payload)) {
        emit commandFailed(normalizedAction, QStringLiteral("请求发送失败"),
                           static_cast<int>(Charging::ErrorCode::NetworkUnavailable));
        return;
    }

    PendingRequest pending;
    pending.action = normalizedAction;
    pending.deadline = QDateTime::currentMSecsSinceEpoch() + 8'000;
    m_pendingRequests.insert(requestId, pending);
    m_latestRequestByAction.insert(normalizedAction, requestId);
    emit commandBusyChanged(normalizedAction, true);
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

void AdminController::handleMessage(const Charging::Message &message)
{
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
        return;
    }

    if (message.header.messageType == Charging::MessageType::LogoutResponse) {
        m_loggedIn = false;
        failPendingRequests(QStringLiteral("管理员已退出登录"),
                            Charging::ErrorCode::SessionExpired);
        emit loggedOut();
        return;
    }

    if (message.header.messageType == Charging::MessageType::AdminCommandResponse) {
        const auto pendingIt = m_pendingRequests.find(message.header.requestId);
        if (pendingIt == m_pendingRequests.end()) {
            return;
        }
        const QString action = pendingIt->action;
        m_pendingRequests.erase(pendingIt);
        if (m_latestRequestByAction.value(action) != message.header.requestId) {
            return;
        }
        m_latestRequestByAction.remove(action);
        emit commandBusyChanged(action, false);
        if (message.header.statusCode == Charging::ErrorCode::Success) {
            emit commandSucceeded(action, message.payload);
        } else {
            emit commandFailed(action,
                               Charging::errorMessage(
                                   message.header.statusCode,
                                   message.payload.value(QStringLiteral("message")).toString()),
                               static_cast<int>(message.header.statusCode));
        }
        return;
    }

    switch (message.header.messageType) {
    case Charging::MessageType::ChargingProgressPush:
    case Charging::MessageType::ChargingStoppedPush:
    case Charging::MessageType::AlarmPush:
    case Charging::MessageType::DeviceStatusPush:
        emit pushReceived(static_cast<quint16>(message.header.messageType), message.payload);
        break;
    default:
        break;
    }
}

void AdminController::expireRequests()
{
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    QList<quint32> expiredIds;
    for (auto it = m_pendingRequests.cbegin(); it != m_pendingRequests.cend(); ++it) {
        if (it->deadline <= now) {
            expiredIds.append(it.key());
        }
    }
    for (quint32 requestId : expiredIds) {
        const PendingRequest pending = m_pendingRequests.take(requestId);
        if (m_latestRequestByAction.value(pending.action) != requestId) {
            continue;
        }
        m_latestRequestByAction.remove(pending.action);
        emit commandBusyChanged(pending.action, false);
        emit commandFailed(pending.action, QStringLiteral("请求超时，请稍后重试"),
                           static_cast<int>(Charging::ErrorCode::RequestTimeout));
    }
}

void AdminController::failPendingRequests(const QString &message,
                                          Charging::ErrorCode errorCode)
{
    const QStringList actions = m_latestRequestByAction.keys();
    m_pendingRequests.clear();
    m_latestRequestByAction.clear();
    for (const QString &action : actions) {
        emit commandBusyChanged(action, false);
        emit commandFailed(action, message, static_cast<int>(errorCode));
    }
}

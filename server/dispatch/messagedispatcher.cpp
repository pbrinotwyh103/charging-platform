#include "dispatch/messagedispatcher.h"

#include "services/serviceregistry.h"

#include "protocol/errorcodes.h"

#include <QDateTime>
#include <QFutureWatcher>
#include <QJsonObject>
#include <QPointer>
#include <QThread>
#include <QtConcurrent>

MessageDispatcher::MessageDispatcher(ServiceRegistry *services, QObject *parent)
    : QObject(parent), m_services(services)
{
    m_workerPool.setMaxThreadCount(qMax(2, QThread::idealThreadCount()));
}

MessageDispatcher::~MessageDispatcher()
{
    m_workerPool.waitForDone();
}

void MessageDispatcher::dispatch(ClientSession *session, const Charging::Message &message)
{
    if (!session) return;
    session->touch();
    if (message.header.requestId == 0) {
        sendError(session, message.header.messageType, 0,
                  Charging::ErrorCode::InvalidPacket,
                  QStringLiteral("请求编号不能为0"));
        return;
    }

    if (message.header.messageType == Charging::MessageType::Ping) {
        session->send(Charging::MessageType::Pong,
                      message.header.requestId,
                      {{QStringLiteral("server"), QStringLiteral("charging_server")}});
        return;
    }

    switch (message.header.messageType) {
    case Charging::MessageType::UserLoginRequest:
        handleUserLogin(session, message);
        return;
    case Charging::MessageType::AdminLoginRequest:
        handleAdminLogin(session, message);
        return;
    case Charging::MessageType::UserProfileRequest:
        handleUserProfile(session, message);
        return;
    case Charging::MessageType::LogoutRequest:
        if (!session->isAuthenticated()) {
            sendError(session, Charging::MessageType::LogoutResponse,
                      message.header.requestId, Charging::ErrorCode::Unauthorized);
            return;
        }
        session->clearAuthentication();
        session->send(Charging::MessageType::LogoutResponse,
                      message.header.requestId,
                      {{QStringLiteral("message"), QStringLiteral("已安全退出")}});
        return;
    default:
        break;
    }

    if (!session->isAuthenticated()) {
        sendError(session, message.header.messageType, message.header.requestId,
                  Charging::ErrorCode::Unauthorized);
        return;
    }
    if (message.header.messageType == Charging::MessageType::AdminCommandRequest
        && session->role() != Charging::Role::Administrator) {
        sendError(session, Charging::MessageType::AdminCommandResponse,
                  message.header.requestId, Charging::ErrorCode::Forbidden);
        return;
    }

    sendError(session, message.header.messageType, message.header.requestId,
              Charging::ErrorCode::UnsupportedMessage,
              QStringLiteral("该业务处理器将在后续阶段实现"));
}

void MessageDispatcher::handleUserLogin(ClientSession *session,
                                        const Charging::Message &message)
{
    if (session->isAuthenticated() && session->role() != Charging::Role::User) {
        sendError(session, Charging::MessageType::UserLoginResponse,
                  message.header.requestId, Charging::ErrorCode::Forbidden,
                  QStringLiteral("当前连接已作为管理员登录"));
        return;
    }
    if (!session->markRequestStarted(message.header.requestId)) {
        sendError(session, Charging::MessageType::UserLoginResponse,
                  message.header.requestId, Charging::ErrorCode::DuplicateRequest);
        return;
    }

    const QString phone = message.payload.value(QStringLiteral("phone")).toString().trimmed();
    const quint32 requestId = message.header.requestId;
    QPointer<ClientSession> guard(session);
    auto *watcher = new QFutureWatcher<AuthResult>(this);
    connect(watcher, &QFutureWatcher<AuthResult>::finished, this,
            [watcher, guard, requestId] {
        const AuthResult result = watcher->result();
        watcher->deleteLater();
        if (!guard) return;
        guard->finishRequest(requestId);
        QJsonObject payload = result.payload;
        payload.insert(QStringLiteral("message"),
                       Charging::errorMessage(result.error, result.message));
        if (result.succeeded()) {
            guard->authenticate(result.role, result.principalId, result.identity);
            payload.insert(QStringLiteral("sessionId"), guard->sessionId());
        }
        guard->send(Charging::MessageType::UserLoginResponse, requestId,
                    payload, result.error);
    });
    watcher->setFuture(QtConcurrent::run(&m_workerPool, [services = m_services, phone] {
        return services->auth()->loginUser(phone);
    }));
}

void MessageDispatcher::handleAdminLogin(ClientSession *session,
                                         const Charging::Message &message)
{
    if (session->isAuthenticated() && session->role() != Charging::Role::Administrator) {
        sendError(session, Charging::MessageType::AdminLoginResponse,
                  message.header.requestId, Charging::ErrorCode::Forbidden,
                  QStringLiteral("当前连接已作为普通用户登录"));
        return;
    }
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (!session->allowLoginAttempt(now)) {
        sendError(session, Charging::MessageType::AdminLoginResponse,
                  message.header.requestId, Charging::ErrorCode::RateLimited,
                  QStringLiteral("登录失败次数过多，请1分钟后再试"));
        return;
    }
    if (!session->markRequestStarted(message.header.requestId)) {
        sendError(session, Charging::MessageType::AdminLoginResponse,
                  message.header.requestId, Charging::ErrorCode::DuplicateRequest);
        return;
    }

    const QString username = message.payload.value(QStringLiteral("username")).toString().trimmed();
    const QString password = message.payload.value(QStringLiteral("password")).toString();
    const quint32 requestId = message.header.requestId;
    QPointer<ClientSession> guard(session);
    auto *watcher = new QFutureWatcher<AuthResult>(this);
    connect(watcher, &QFutureWatcher<AuthResult>::finished, this,
            [watcher, guard, requestId] {
        const AuthResult result = watcher->result();
        watcher->deleteLater();
        if (!guard) return;
        guard->finishRequest(requestId);
        QJsonObject payload = result.payload;
        payload.insert(QStringLiteral("message"),
                       Charging::errorMessage(result.error, result.message));
        if (result.succeeded()) {
            guard->clearLoginFailures();
            guard->authenticate(result.role, result.principalId, result.identity);
            payload.insert(QStringLiteral("sessionId"), guard->sessionId());
        } else {
            guard->registerLoginFailure(QDateTime::currentMSecsSinceEpoch());
        }
        guard->send(Charging::MessageType::AdminLoginResponse, requestId,
                    payload, result.error);
    });
    watcher->setFuture(QtConcurrent::run(&m_workerPool,
        [services = m_services, username, password] {
            return services->auth()->loginAdmin(username, password);
        }));
}

void MessageDispatcher::handleUserProfile(ClientSession *session,
                                          const Charging::Message &message)
{
    if (!session->isAuthenticated()) {
        sendError(session, Charging::MessageType::UserProfileResponse,
                  message.header.requestId, Charging::ErrorCode::Unauthorized);
        return;
    }
    if (session->role() != Charging::Role::User) {
        sendError(session, Charging::MessageType::UserProfileResponse,
                  message.header.requestId, Charging::ErrorCode::Forbidden);
        return;
    }
    if (!session->markRequestStarted(message.header.requestId)) {
        sendError(session, Charging::MessageType::UserProfileResponse,
                  message.header.requestId, Charging::ErrorCode::DuplicateRequest);
        return;
    }

    const qint64 userId = session->principalId();
    const quint32 requestId = message.header.requestId;
    QPointer<ClientSession> guard(session);
    auto *watcher = new QFutureWatcher<AuthResult>(this);
    connect(watcher, &QFutureWatcher<AuthResult>::finished, this,
            [watcher, guard, requestId] {
        const AuthResult result = watcher->result();
        watcher->deleteLater();
        if (!guard) return;
        guard->finishRequest(requestId);
        QJsonObject payload = result.payload;
        payload.insert(QStringLiteral("message"),
                       Charging::errorMessage(result.error, result.message));
        guard->send(Charging::MessageType::UserProfileResponse, requestId,
                    payload, result.error);
    });
    watcher->setFuture(QtConcurrent::run(&m_workerPool, [services = m_services, userId] {
        return services->auth()->userProfile(userId);
    }));
}

void MessageDispatcher::sendError(ClientSession *session, Charging::MessageType type,
                                  quint32 requestId, Charging::ErrorCode error,
                                  const QString &detail)
{
    session->send(type, requestId,
                  {{QStringLiteral("message"), Charging::errorMessage(error, detail)}},
                  error);
}

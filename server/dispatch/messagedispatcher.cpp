#include "dispatch/messagedispatcher.h"
#include "services/serviceregistry.h"

#include <QDateTime>
#include <QFutureWatcher>
#include <QPointer>
#include <QThread>
#include <QTimer>
#include <QtConcurrent>
#include <memory>

using Charging::ErrorCode;
using Charging::MessageType;
using Charging::Role;

namespace {
MessageType responseFor(MessageType request)
{
    switch (request) {
    case MessageType::Ping: return MessageType::Pong;
    case MessageType::UserLoginRequest: return MessageType::UserLoginResponse;
    case MessageType::AdminLoginRequest: return MessageType::AdminLoginResponse;
    case MessageType::LogoutRequest: return MessageType::LogoutResponse;
    case MessageType::UserProfileRequest: return MessageType::UserProfileResponse;
    case MessageType::UserProfileUpdateRequest: return MessageType::UserProfileUpdateResponse;
    case MessageType::WalletRechargeRequest: return MessageType::WalletRechargeResponse;
    case MessageType::WalletLedgerRequest: return MessageType::WalletLedgerResponse;
    case MessageType::StationListRequest: return MessageType::StationListResponse;
    case MessageType::PileListRequest: return MessageType::PileListResponse;
    case MessageType::FavoriteToggleRequest: return MessageType::FavoriteToggleResponse;
    case MessageType::PileCodeLookupRequest: return MessageType::PileCodeLookupResponse;
    case MessageType::ReservationCreateRequest: return MessageType::ReservationCreateResponse;
    case MessageType::ReservationCancelRequest: return MessageType::ReservationCancelResponse;
    case MessageType::ChargingStartRequest: return MessageType::ChargingStartResponse;
    case MessageType::ChargingStopRequest: return MessageType::ChargingStopResponse;
    case MessageType::ActiveOrderRequest: return MessageType::ActiveOrderResponse;
    case MessageType::CustomerServiceRequest: return MessageType::CustomerServiceResponse;
    case MessageType::AdminCommandRequest: return MessageType::AdminCommandResponse;
    default: return request;
    }
}

void sendResult(ClientSession *session, MessageType type, quint32 requestId,
                const ServiceResult &result)
{
    auto payload = result.payload;
    payload.insert("message", Charging::errorMessage(result.error, result.message));
    if (!result.reason.isEmpty()) payload.insert("reason", result.reason);
    session->send(type, requestId, payload, result.error);
}

struct RequestCompletion {
    ClientSession *session;
    quint32 requestId;
    ~RequestCompletion() { if (session) session->finishRequest(requestId); }
};
}

struct MessageDispatcher::RequestResult : ServiceResult {
    RequestResult(const ServiceResult &result) : ServiceResult(result) {}
    RequestResult(const AuthResult &result)
        : principalId(result.principalId), identity(result.identity)
    {
        error = result.error;
        message = result.message;
        payload = result.payload;
        // Retain the original client's avatarPath while exposing the agreed
        // profile field in both login and profile-read responses.
        if (payload.contains("avatarPath")) payload.insert("avatar", payload.value("avatarPath"));
    }
    qint64 principalId = 0;
    QString identity;
};

MessageDispatcher::MessageDispatcher(ServiceRegistry *services, QObject *parent,
                                     int requestTimeoutMilliseconds)
    : QObject(parent), m_services(services),
      m_requestTimeoutMilliseconds(qMax(1, requestTimeoutMilliseconds))
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
    const auto type = message.header.messageType;
    const auto response = responseFor(type);
    const auto requestId = message.header.requestId;
    if (!requestId) {
        sendError(session, response, requestId, ErrorCode::InvalidPacket,
                  QStringLiteral("请求编号不能为0"));
        return;
    }
    if (!session->markRequestStarted(requestId)) {
        sendError(session, response, requestId, ErrorCode::DuplicateRequest);
        return;
    }
    RequestCompletion completion{session, requestId};
    auto run = [&](std::function<RequestResult()> operation, Role loginRole = Role::Anonymous) {
        executeAsync(session, response, requestId, std::move(operation), loginRole);
        completion.session = nullptr; // The asynchronous completion now owns finishRequest.
    };
    if (type == MessageType::Ping) {
        session->send(response, requestId, {{"server", "charging_server"},
                      {"message", Charging::errorMessage(ErrorCode::Success)}});
        return;
    }
    if (response == type) {
        sendError(session, response, requestId, ErrorCode::UnsupportedMessage);
        return;
    }
    if (!m_services || !m_services->isInitialized()) {
        sendError(session, response, requestId, ErrorCode::InternalError);
        return;
    }

    auto payload = message.payload;
    if (type == MessageType::UserLoginRequest || type == MessageType::AdminLoginRequest) {
        const auto role = type == MessageType::UserLoginRequest ? Role::User : Role::Administrator;
        if (session->isAuthenticated() && session->role() != role) {
            sendError(session, response, requestId, ErrorCode::Forbidden);
            return;
        }
        if (role == Role::User) {
            if (!payload.value("phone").isString()) {
                sendError(session, response, requestId, ErrorCode::ValidationFailed);
                return;
            }
            const auto phone = payload.value("phone").toString().trimmed();
            run([services = m_services, phone] { return services->auth()->loginUser(phone); }, role);
        } else {
            if (!session->allowLoginAttempt(QDateTime::currentMSecsSinceEpoch())) {
                sendError(session, response, requestId, ErrorCode::RateLimited,
                          QStringLiteral("登录失败次数过多，请1分钟后再试"));
                return;
            }
            if (!payload.value("username").isString() || !payload.value("password").isString()) {
                sendError(session, response, requestId, ErrorCode::ValidationFailed);
                return;
            }
            const auto username = payload.value("username").toString().trimmed();
            const auto password = payload.value("password").toString();
            run([services = m_services, username, password] {
                return services->auth()->loginAdmin(username, password);
            }, role);
        }
        return;
    }
    if (type == MessageType::LogoutRequest) {
        const bool wasAuthenticated = session->isAuthenticated();
        session->clearAuthentication();
        if (!wasAuthenticated) {
            sendError(session, response, requestId, ErrorCode::Unauthorized);
            return;
        }
        session->send(response, requestId, {{"message", QStringLiteral("已安全退出")}});
        return;
    }
    if (!session->isAuthenticated()) {
        sendError(session, response, requestId, ErrorCode::Unauthorized);
        return;
    }
    const auto requiredRole = type == MessageType::AdminCommandRequest ? Role::Administrator : Role::User;
    if (session->role() != requiredRole) {
        sendError(session, response, requestId, ErrorCode::Forbidden);
        return;
    }

    const auto principalId = session->principalId();
    // Request IDs are unique only within this authenticated session. A client
    // cannot select another session's persisted command replay namespace.
    if (type == MessageType::AdminCommandRequest)
        payload.insert("_requestScope", session->sessionId());
    run([services = m_services, type, principalId, requestId, payload]() -> RequestResult {
        switch (type) {
        case MessageType::UserProfileRequest: return services->auth()->userProfile(principalId);
        case MessageType::UserProfileUpdateRequest: return services->users()->updateProfile(principalId, payload);
        case MessageType::WalletRechargeRequest: return services->users()->recharge(principalId, payload);
        case MessageType::WalletLedgerRequest: return services->users()->walletLedger(principalId, payload);
        case MessageType::StationListRequest: return services->stations()->nearby(principalId, payload);
        case MessageType::PileListRequest: return services->piles()->listForStation(payload);
        case MessageType::PileCodeLookupRequest: return services->piles()->findByCode(principalId, payload);
        case MessageType::FavoriteToggleRequest: return services->stations()->toggleFavorite(principalId, payload);
        case MessageType::ReservationCreateRequest: return services->reservations()->create(principalId, payload);
        case MessageType::ReservationCancelRequest: return services->reservations()->cancel(principalId, payload);
        case MessageType::ChargingStartRequest: return services->charging()->start(principalId, payload);
        case MessageType::ChargingStopRequest: return services->charging()->stop(principalId, Role::User, payload);
        case MessageType::ActiveOrderRequest: return services->orders()->active(principalId);
        case MessageType::CustomerServiceRequest: return services->customerService()->ask(principalId, payload);
        case MessageType::AdminCommandRequest:
            return services->admin()->execute(principalId, requestId, payload, services->charging());
        default: {
            ServiceResult result;
            result.error = ErrorCode::UnsupportedMessage;
            return result;
        }
        }
    });
}

void MessageDispatcher::executeAsync(ClientSession *session, MessageType responseType,
    quint32 requestId, std::function<RequestResult()> operation, Role loginRole)
{
    QPointer<ClientSession> guard(session);
    const auto sessionId = session->sessionId();
    const auto generation = session->authenticationGeneration();
    struct State { bool replied = false; bool completed = false; };
    const auto state = std::make_shared<State>();
    auto *watcher = new QFutureWatcher<RequestResult>(this);
    auto *timeout = new QTimer(watcher);
    timeout->setSingleShot(true);
    connect(timeout, &QTimer::timeout, this,
            [this, guard, state, responseType, requestId, sessionId, generation] {
        if (!guard || state->replied) return;
        state->replied = true;
        sendError(guard, responseType, requestId,
                  guard->authenticationGeneration() == generation && guard->sessionId() == sessionId
                      ? ErrorCode::RequestTimeout : ErrorCode::SessionExpired);
        // A timeout does not cancel a database mutation. Keep the ID reserved
        // until the worker finishes, and suppress its eventual late response.
    });
    connect(watcher, &QObject::destroyed, session, [guard, state, requestId] {
        if (guard && !state->completed) guard->finishRequest(requestId);
    });
    connect(watcher, &QFutureWatcher<RequestResult>::finished, this,
            [this, watcher, timeout, guard, state, responseType, requestId, sessionId, generation, loginRole] {
        timeout->stop();
        auto result = watcher->result();
        watcher->deleteLater();
        state->completed = true;
        if (!guard) return;
        guard->finishRequest(requestId);
        if (state->replied) return;
        state->replied = true;
        if (guard->authenticationGeneration() != generation || guard->sessionId() != sessionId) {
            sendError(guard, responseType, requestId, ErrorCode::SessionExpired);
            return;
        }
        if (loginRole != Role::Anonymous) {
            if (result.succeeded()) {
                guard->authenticate(loginRole, result.principalId, result.identity);
                result.payload.insert("sessionId", guard->sessionId());
                if (loginRole == Role::Administrator) guard->clearLoginFailures();
            } else if (loginRole == Role::Administrator) {
                guard->registerLoginFailure(QDateTime::currentMSecsSinceEpoch());
            }
        }
        sendResult(guard, responseType, requestId, result);
    });
    watcher->setFuture(QtConcurrent::run(&m_workerPool, [operation = std::move(operation)]() -> RequestResult {
        try {
            return operation();
        } catch (...) {
            ServiceResult result;
            result.error = ErrorCode::InternalError;
            return result;
        }
    }));
    timeout->start(m_requestTimeoutMilliseconds);
}

void MessageDispatcher::sendError(ClientSession *session, MessageType type,
                                  quint32 requestId, ErrorCode error, const QString &detail)
{
    ServiceResult result;
    result.error = error;
    result.message = detail;
    sendResult(session, type, requestId, result);
}

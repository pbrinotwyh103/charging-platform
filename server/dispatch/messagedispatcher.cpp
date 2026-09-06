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
    case Charging::MessageType::WalletRechargeRequest:
        handleWalletRecharge(session, message);
        return;
    case Charging::MessageType::WalletLedgerRequest:
        handleWalletLedger(session, message);
        return;
    case Charging::MessageType::OrderHistoryRequest:
        handleOrderHistory(session, message);
        return;
    case Charging::MessageType::StationListRequest:
        handleStationList(session, message, false);
        return;
    case Charging::MessageType::FavoriteListRequest:
        handleStationList(session, message, true);
        return;
    case Charging::MessageType::FavoriteToggleRequest:
        handleFavoriteToggle(session, message);
        return;
    case Charging::MessageType::MapGeocodeRequest:
        handleMapGeocode(session, message);
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

void MessageDispatcher::handleWalletRecharge(
    ClientSession *session, const Charging::Message &message)
{
    if (!session->isAuthenticated()) {
        sendError(session, Charging::MessageType::WalletRechargeResponse,
                  message.header.requestId, Charging::ErrorCode::Unauthorized);
        return;
    }
    if (session->role() != Charging::Role::User) {
        sendError(session, Charging::MessageType::WalletRechargeResponse,
                  message.header.requestId, Charging::ErrorCode::Forbidden);
        return;
    }
    if (!session->markRequestStarted(message.header.requestId)) {
        sendError(session, Charging::MessageType::WalletRechargeResponse,
                  message.header.requestId,
                  Charging::ErrorCode::DuplicateRequest);
        return;
    }

    const qint64 userId = session->principalId();
    const quint32 requestId = message.header.requestId;
    const QJsonObject requestPayload = message.payload;
    QPointer<ClientSession> guard(session);
    auto *watcher = new QFutureWatcher<UserServiceResult>(this);
    connect(watcher, &QFutureWatcher<UserServiceResult>::finished, this,
            [watcher, guard, requestId] {
        const UserServiceResult result = watcher->result();
        watcher->deleteLater();
        if (!guard) return;
        guard->finishRequest(requestId);
        QJsonObject payload = result.payload;
        payload.insert(QStringLiteral("message"),
                       Charging::errorMessage(result.error, result.message));
        guard->send(Charging::MessageType::WalletRechargeResponse,
                    requestId, payload, result.error);
    });
    watcher->setFuture(QtConcurrent::run(
        &m_workerPool, [services = m_services, userId, requestPayload] {
            return services->users()->recharge(userId, requestPayload);
        }));
}

void MessageDispatcher::handleWalletLedger(
    ClientSession *session, const Charging::Message &message)
{
    if (!session->isAuthenticated()) {
        sendError(session, Charging::MessageType::WalletLedgerResponse,
                  message.header.requestId, Charging::ErrorCode::Unauthorized);
        return;
    }
    if (session->role() != Charging::Role::User) {
        sendError(session, Charging::MessageType::WalletLedgerResponse,
                  message.header.requestId, Charging::ErrorCode::Forbidden);
        return;
    }
    if (!session->markRequestStarted(message.header.requestId)) {
        sendError(session, Charging::MessageType::WalletLedgerResponse,
                  message.header.requestId,
                  Charging::ErrorCode::DuplicateRequest);
        return;
    }

    const qint64 userId = session->principalId();
    const quint32 requestId = message.header.requestId;
    const QJsonObject requestPayload = message.payload;
    QPointer<ClientSession> guard(session);
    auto *watcher = new QFutureWatcher<UserServiceResult>(this);
    connect(watcher, &QFutureWatcher<UserServiceResult>::finished, this,
            [watcher, guard, requestId] {
        const UserServiceResult result = watcher->result();
        watcher->deleteLater();
        if (!guard) return;
        guard->finishRequest(requestId);
        QJsonObject payload = result.payload;
        payload.insert(QStringLiteral("message"),
                       Charging::errorMessage(result.error, result.message));
        guard->send(Charging::MessageType::WalletLedgerResponse,
                    requestId, payload, result.error);
    });
    watcher->setFuture(QtConcurrent::run(
        &m_workerPool, [services = m_services, userId, requestPayload] {
            return services->users()->walletLedger(userId, requestPayload);
        }));
}

void MessageDispatcher::handleOrderHistory(ClientSession *session,
                                           const Charging::Message &message)
{
    if (!session->isAuthenticated()) {
        sendError(session, Charging::MessageType::OrderHistoryResponse,
                  message.header.requestId, Charging::ErrorCode::Unauthorized);
        return;
    }
    if (session->role() != Charging::Role::User) {
        sendError(session, Charging::MessageType::OrderHistoryResponse,
                  message.header.requestId, Charging::ErrorCode::Forbidden);
        return;
    }
    if (!session->markRequestStarted(message.header.requestId)) {
        sendError(session, Charging::MessageType::OrderHistoryResponse,
                  message.header.requestId, Charging::ErrorCode::DuplicateRequest);
        return;
    }

    const qint64 userId = session->principalId();
    const quint32 requestId = message.header.requestId;
    QPointer<ClientSession> guard(session);
    auto *watcher = new QFutureWatcher<OrderHistoryResult>(this);
    connect(watcher, &QFutureWatcher<OrderHistoryResult>::finished, this,
            [watcher, guard, requestId] {
        const OrderHistoryResult result = watcher->result();
        watcher->deleteLater();
        if (!guard) return;
        guard->finishRequest(requestId);
        QJsonObject payload{
            {QStringLiteral("items"), result.items},
            {QStringLiteral("message"),
             Charging::errorMessage(result.error, result.message)}
        };
        guard->send(Charging::MessageType::OrderHistoryResponse, requestId,
                    payload, result.error);
    });
    watcher->setFuture(QtConcurrent::run(&m_workerPool,
        [services = m_services, userId] {
        return services->orders()->historyForUser(userId);
    }));
}

void MessageDispatcher::handleStationList(ClientSession *session,
                                          const Charging::Message &message,
                                          bool favoritesOnly)
{
    const Charging::MessageType responseType = favoritesOnly
        ? Charging::MessageType::FavoriteListResponse
        : Charging::MessageType::StationListResponse;
    if (!session->isAuthenticated()) {
        sendError(session, responseType, message.header.requestId,
                  Charging::ErrorCode::Unauthorized);
        return;
    }
    if (session->role() != Charging::Role::User) {
        sendError(session, responseType, message.header.requestId,
                  Charging::ErrorCode::Forbidden);
        return;
    }
    if (!session->markRequestStarted(message.header.requestId)) {
        sendError(session, responseType, message.header.requestId,
                  Charging::ErrorCode::DuplicateRequest);
        return;
    }

    const qint64 userId = session->principalId();
    const quint32 requestId = message.header.requestId;
    QPointer<ClientSession> guard(session);
    auto *watcher = new QFutureWatcher<StationListResult>(this);
    connect(watcher, &QFutureWatcher<StationListResult>::finished, this,
            [watcher, guard, requestId, responseType] {
        const StationListResult result = watcher->result();
        watcher->deleteLater();
        if (!guard) return;
        guard->finishRequest(requestId);
        guard->send(responseType, requestId,
                    {{QStringLiteral("items"), result.items},
                     {QStringLiteral("message"),
                      Charging::errorMessage(result.error, result.message)}},
                    result.error);
    });
    watcher->setFuture(QtConcurrent::run(&m_workerPool,
        [services = m_services, userId, favoritesOnly] {
        return services->stations()->listForUser(userId, favoritesOnly);
    }));
}

void MessageDispatcher::handleFavoriteToggle(ClientSession *session,
                                              const Charging::Message &message)
{
    if (!session->isAuthenticated()) {
        sendError(session, Charging::MessageType::FavoriteToggleResponse,
                  message.header.requestId, Charging::ErrorCode::Unauthorized);
        return;
    }
    if (session->role() != Charging::Role::User) {
        sendError(session, Charging::MessageType::FavoriteToggleResponse,
                  message.header.requestId, Charging::ErrorCode::Forbidden);
        return;
    }
    if (!session->markRequestStarted(message.header.requestId)) {
        sendError(session, Charging::MessageType::FavoriteToggleResponse,
                  message.header.requestId, Charging::ErrorCode::DuplicateRequest);
        return;
    }

    const qint64 stationId =
        message.payload.value(QStringLiteral("stationId")).toVariant().toLongLong();
    const bool favorited = message.payload.value(QStringLiteral("favorited")).toBool();
    const qint64 userId = session->principalId();
    const quint32 requestId = message.header.requestId;
    QPointer<ClientSession> guard(session);
    auto *watcher = new QFutureWatcher<FavoriteToggleResult>(this);
    connect(watcher, &QFutureWatcher<FavoriteToggleResult>::finished, this,
            [watcher, guard, requestId] {
        const FavoriteToggleResult result = watcher->result();
        watcher->deleteLater();
        if (!guard) return;
        guard->finishRequest(requestId);
        guard->send(Charging::MessageType::FavoriteToggleResponse, requestId,
                    {{QStringLiteral("stationId"), result.stationId},
                     {QStringLiteral("favorited"), result.favorited},
                     {QStringLiteral("updatedAt"), result.updatedAt},
                     {QStringLiteral("message"),
                      Charging::errorMessage(result.error, result.message)}},
                    result.error);
    });
    watcher->setFuture(QtConcurrent::run(&m_workerPool,
        [services = m_services, userId, stationId, favorited] {
        return services->stations()->setFavorite(userId, stationId, favorited);
    }));
}

void MessageDispatcher::handleMapGeocode(ClientSession *session,
                                         const Charging::Message &message)
{
    if (!session->isAuthenticated()) {
        sendError(session, Charging::MessageType::MapGeocodeResponse,
                  message.header.requestId, Charging::ErrorCode::Unauthorized);
        return;
    }
    if (session->role() != Charging::Role::User) {
        sendError(session, Charging::MessageType::MapGeocodeResponse,
                  message.header.requestId, Charging::ErrorCode::Forbidden);
        return;
    }

    const QJsonValue addressValue =
        message.payload.value(QStringLiteral("address"));
    const QJsonValue regionValue =
        message.payload.value(QStringLiteral("region"));
    if (!addressValue.isString()
        || (!regionValue.isUndefined() && !regionValue.isString())) {
        sendError(session, Charging::MessageType::MapGeocodeResponse,
                  message.header.requestId, Charging::ErrorCode::InvalidPayload,
                  QStringLiteral("地址解析参数格式错误"));
        return;
    }

    const QString address = addressValue.toString().trimmed();
    const QString region = regionValue.toString().trimmed();
    if (address.isEmpty() || address.size() > 200) {
        sendError(session, Charging::MessageType::MapGeocodeResponse,
                  message.header.requestId, Charging::ErrorCode::ValidationFailed,
                  QStringLiteral("地址长度必须为 1—200 个字符"));
        return;
    }
    if (!session->markRequestStarted(message.header.requestId)) {
        sendError(session, Charging::MessageType::MapGeocodeResponse,
                  message.header.requestId, Charging::ErrorCode::DuplicateRequest);
        return;
    }

    const quint32 requestId = message.header.requestId;
    QPointer<ClientSession> guard(session);
    m_services->maps()->geocode(
        address, region,
        [guard, requestId](const MapGeocodeResult &result) {
            if (!guard) return;
            guard->finishRequest(requestId);
            QJsonObject payload{
                {QStringLiteral("message"),
                 Charging::errorMessage(result.error, result.message)}
            };
            if (result.succeeded()) {
                payload.insert(QStringLiteral("coordinate"), result.coordinate);
                payload.insert(QStringLiteral("formattedAddress"),
                               result.formattedAddress);
            }
            guard->send(Charging::MessageType::MapGeocodeResponse, requestId,
                        payload, result.error);
        });
}

void MessageDispatcher::sendError(ClientSession *session, Charging::MessageType type,
                                  quint32 requestId, Charging::ErrorCode error,
                                  const QString &detail)
{
    session->send(type, requestId,
                  {{QStringLiteral("message"), Charging::errorMessage(error, detail)}},
                  error);
}

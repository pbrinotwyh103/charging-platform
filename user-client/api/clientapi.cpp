#include "api/clientapi.h"
#include "protocol/errorcodes.h"
#include <QJsonArray>
#include <QTimer>
#include <QUuid>

#include <cmath>

namespace {

QString businessErrorText(const Charging::Message &message)
{
    const QString reason =
        message.payload.value(QStringLiteral("reason")).toString();
    if (reason == QStringLiteral("insufficient_balance"))
        return QStringLiteral("钱包余额不足，请先充值");
    if (reason == QStringLiteral("pile_unavailable"))
        return QStringLiteral("所选电桩当前不可用，请刷新后选择其他空闲桩");
    if (reason == QStringLiteral("reservation_conflict"))
        return QStringLiteral("当前已有有效预约，不能重复预约");
    if (reason == QStringLiteral("order_conflict"))
        return QStringLiteral("当前已有进行中的充电订单");
    if (reason == QStringLiteral("reservation_expired"))
        return QStringLiteral("预约已过期，请重新预约");
    if (reason == QStringLiteral("user_frozen"))
        return QStringLiteral("账号已冻结，当前操作不可用");
    if (reason == QStringLiteral("transaction_conflict"))
        return QStringLiteral("充值交易号冲突，请重新提交");
    return Charging::errorMessage(
        message.header.statusCode,
        message.payload.value(QStringLiteral("message")).toString());
}

} // namespace

ClientApi::ClientApi(QObject *parent) : QObject(parent)
{
    m_connection.setClientName(QStringLiteral("user-client"));
    connect(&m_snapshots, &SnapshotStore::updated,
            this, &ClientApi::chargingSnapshotReceived);
    connect(&m_connection, &Charging::ClientConnection::connected, this, [this] {
        emit connectionStatus(QStringLiteral("已连接服务器"), true);
        emit chargingConnectionChanged(true);
        if (!m_phone.isEmpty()) login(m_phone);
    });
    connect(&m_connection, &Charging::ClientConnection::disconnected, this, [this] {
        emit connectionStatus(QStringLiteral("服务器连接已断开，数据可能已过期"), false);
        emit chargingConnectionChanged(false);
        if (m_geocodeRequest != 0) {
            m_pendingRequests.remove(m_geocodeRequest);
            m_geocodeRequest = 0;
            emit addressResolutionFailed(
                QStringLiteral("网络已断开，地址解析已取消，请重新连接后重试"));
        }
        const QSet<qint64> pendingStations = m_pendingFavoriteStations;
        for (auto it = m_favoriteRequests.cbegin(); it != m_favoriteRequests.cend(); ++it)
            m_pendingRequests.remove(it.key());
        m_favoriteRequests.clear();
        m_pendingFavoriteStations.clear();
        for (qint64 stationId : pendingStations)
            emit favoriteUpdateFailed(stationId, QStringLiteral("网络已断开，收藏状态已恢复"));
    });
    connect(&m_connection, &Charging::ClientConnection::socketError, this,
            [this](const QString &e) { emit connectionStatus(QStringLiteral("连接失败：%1").arg(e), false); });
    connect(&m_connection, &Charging::ClientConnection::reconnectScheduled, this,
            [this](int a, int d) { emit connectionStatus(QStringLiteral("第%1次重连将在%2秒后进行").arg(a).arg(d / 1000.0, 0, 'f', 1), false); });
    connect(&m_connection, &Charging::ClientConnection::messageReceived, this,
            [this](const Charging::Message &m) {
        const auto errorText = [&m] {
            return businessErrorText(m);
        };
        if (m.header.requestId == m_loginRequest && m.header.messageType == Charging::MessageType::UserLoginResponse) {
            m_pendingRequests.remove(m.header.requestId);
            emit loginBusy(false);
            if (m.header.statusCode != Charging::ErrorCode::Success) {
                emit loginFailed(Charging::errorMessage(m.header.statusCode, m.payload.value(QStringLiteral("message")).toString()));
            } else {
                emit loginSucceeded(m.payload);
            }
        } else if (m.header.messageType == Charging::MessageType::MapGeocodeResponse) {
            if (m.header.requestId != m_geocodeRequest) return;
            m_pendingRequests.remove(m.header.requestId);
            m_geocodeRequest = 0;
            if (m.header.statusCode != Charging::ErrorCode::Success) {
                emit addressResolutionFailed(Charging::errorMessage(
                    m.header.statusCode,
                    m.payload.value(QStringLiteral("message")).toString()));
                return;
            }
            const QJsonObject coordinate =
                m.payload.value(QStringLiteral("coordinate")).toObject();
            const double latitude =
                coordinate.value(QStringLiteral("lat")).toDouble();
            const double longitude =
                coordinate.value(QStringLiteral("lng")).toDouble();
            if (!coordinate.value(QStringLiteral("lat")).isDouble()
                || !coordinate.value(QStringLiteral("lng")).isDouble()
                || coordinate.value(QStringLiteral("crs")).toString()
                       != QStringLiteral("GCJ-02")
                || !std::isfinite(latitude) || !std::isfinite(longitude)
                || latitude < -90.0 || latitude > 90.0
                || longitude < -180.0 || longitude > 180.0) {
                emit addressResolutionFailed(
                    QStringLiteral("服务器返回的地址坐标无效"));
                return;
            }
            emit addressResolved(
                latitude, longitude,
                m.payload.value(QStringLiteral("formattedAddress")).toString());
        } else if (m.header.messageType == Charging::MessageType::MapGeocodeRequest
                   && m.header.requestId == m_geocodeRequest
                   && m.header.statusCode
                          == Charging::ErrorCode::UnsupportedMessage) {
            m_pendingRequests.remove(m.header.requestId);
            m_geocodeRequest = 0;
            emit addressResolutionUnavailable();
        } else if (m.header.messageType == Charging::MessageType::StationListResponse) {
            m_pendingRequests.remove(m.header.requestId);
            if (m.header.statusCode == Charging::ErrorCode::Success) {
                emit stationsReceived(m.payload.value(QStringLiteral("items")).toArray());
            } else {
                const QString message = Charging::errorMessage(
                    m.header.statusCode,
                    m.payload.value(QStringLiteral("message")).toString());
                emit stationsFailed(QStringLiteral("站点查询失败：%1").arg(message));
                emit featureUnavailable(QStringLiteral("站点查询失败：%1").arg(message));
            }
        } else if (m.header.messageType == Charging::MessageType::PileListResponse) {
            m_pendingRequests.remove(m.header.requestId);
            if (m.header.statusCode == Charging::ErrorCode::Success) emit pilesReceived(m.payload.value(QStringLiteral("items")).toArray()); else emit featureUnavailable(QStringLiteral("电桩查询失败：%1").arg(m.payload.value(QStringLiteral("message")).toString()));
        } else if (m.header.messageType == Charging::MessageType::UserProfileResponse) {
            m_pendingRequests.remove(m.header.requestId);
            if (m.header.statusCode == Charging::ErrorCode::Success) emit profileReceived(m.payload);
            else emit featureUnavailable(QStringLiteral("资料读取失败：%1").arg(m.payload.value(QStringLiteral("message")).toString()));
        } else if (m.header.messageType == Charging::MessageType::OrderHistoryResponse) {
            m_pendingRequests.remove(m.header.requestId);
            if (m.header.statusCode == Charging::ErrorCode::Success) {
                emit orderHistoryReceived(m.payload.value(QStringLiteral("items")).toArray());
            } else {
                emit orderHistoryFailed(Charging::errorMessage(
                    m.header.statusCode,
                    m.payload.value(QStringLiteral("message")).toString()));
            }
        } else if (m.header.messageType == Charging::MessageType::FavoriteListResponse) {
            m_pendingRequests.remove(m.header.requestId);
            if (m.header.statusCode == Charging::ErrorCode::Success) {
                emit favoritesReceived(m.payload.value(QStringLiteral("items")).toArray());
            } else {
                emit favoritesFailed(Charging::errorMessage(
                    m.header.statusCode,
                    m.payload.value(QStringLiteral("message")).toString()));
            }
        } else if (m.header.messageType == Charging::MessageType::FavoriteToggleResponse) {
            const auto request = m_favoriteRequests.find(m.header.requestId);
            if (request == m_favoriteRequests.end()) return;
            const qint64 stationId = request.value();
            m_favoriteRequests.erase(request);
            m_pendingFavoriteStations.remove(stationId);
            m_pendingRequests.remove(m.header.requestId);
            if (m.header.statusCode == Charging::ErrorCode::Success) {
                emit favoriteUpdated(
                    stationId,
                    m.payload.value(QStringLiteral("favorited")).toBool(),
                    m.payload.value(QStringLiteral("updatedAt")).toString());
            } else {
                emit favoriteUpdateFailed(stationId, Charging::errorMessage(
                    m.header.statusCode,
                    m.payload.value(QStringLiteral("message")).toString()));
            }
        } else if (m.header.messageType == Charging::MessageType::UserProfileUpdateResponse) {
            m_pendingRequests.remove(m.header.requestId);
            if (m.header.statusCode == Charging::ErrorCode::Success) {
                emit profileUpdated(m.payload);
                emit profileReceived(m.payload);
            } else {
                emit profileUpdateFailed(errorText());
            }
        } else if (m.header.messageType == Charging::MessageType::WalletRechargeResponse) {
            m_pendingRequests.remove(m.header.requestId);
            if (m.header.statusCode == Charging::ErrorCode::Success)
                emit rechargeSucceeded(m.payload);
            else
                emit rechargeFailed(errorText());
        } else if (m.header.messageType == Charging::MessageType::WalletLedgerResponse) {
            m_pendingRequests.remove(m.header.requestId);
            if (m.header.statusCode == Charging::ErrorCode::Success) {
                QJsonArray rechargeRecords;
                const QJsonArray items =
                    m.payload.value(QStringLiteral("items")).toArray();
                for (const QJsonValue &value : items) {
                    if (value.toObject().value(QStringLiteral("recordType")).toString()
                        == QStringLiteral("recharge"))
                        rechargeRecords.append(value);
                }
                emit walletLedgerReceived(rechargeRecords);
            } else {
                emit walletLedgerFailed(errorText());
            }
        } else if (m.header.messageType == Charging::MessageType::ReservationCreateResponse) {
            m_pendingRequests.remove(m.header.requestId);
            if (m.header.statusCode == Charging::ErrorCode::Success)
                emit reservationCreated(m.payload);
            else
                emit reservationFailed(errorText());
        } else if (m.header.messageType == Charging::MessageType::ReservationCancelResponse) {
            m_pendingRequests.remove(m.header.requestId);
            if (m.header.statusCode == Charging::ErrorCode::Success)
                emit reservationCancelled(m.payload);
            else
                emit reservationFailed(errorText());
        } else if (m.header.messageType == Charging::MessageType::ActiveOrderResponse) {
            m_pendingRequests.remove(m.header.requestId);
            if (m.header.statusCode == Charging::ErrorCode::Success) {
                const bool active =
                    m.payload.value(QStringLiteral("active")).toBool();
                if (active) m_snapshots.apply(m.payload);
                emit activeOrderReceived(active, m.payload);
            } else {
                emit activeOrderFailed(errorText());
            }
        } else if (m.header.messageType == Charging::MessageType::ChargingStartResponse) {
            m_pendingRequests.remove(m.header.requestId);
            if (m.header.statusCode == Charging::ErrorCode::Success) {
                m_snapshots.apply(m.payload);
                emit chargingStarted(m.payload);
            } else {
                emit chargingActionFailed(errorText());
            }
        } else if (m.header.messageType == Charging::MessageType::ChargingStopResponse) {
            m_pendingRequests.remove(m.header.requestId);
            if (m.header.statusCode == Charging::ErrorCode::Success) {
                m_snapshots.apply(m.payload);
                emit chargingStopped(m.payload);
            } else {
                emit chargingActionFailed(errorText());
            }
        } else if (m.header.messageType == Charging::MessageType::ChargingProgressPush) {
            m_snapshots.apply(m.payload);
        } else if (m.header.messageType == Charging::MessageType::ChargingStoppedPush) {
            if (m_snapshots.apply(m.payload))
                emit chargingStopped(m.payload);
        } else if (m.header.messageType == Charging::MessageType::AlarmPush) {
            const QString message =
                m.payload.value(QStringLiteral("message")).toString();
            emit chargingActionFailed(message.isEmpty()
                ? QStringLiteral("充电设备发生异常，任务可能已停止")
                : message);
        } else if (m.header.messageType == Charging::MessageType::LogoutResponse) {
            m_pendingRequests.remove(m.header.requestId);
            emit loggedOut();
        }
    });
}

void ClientApi::connectToServer(const QString &host, quint16 port) { m_connection.connectToServer(host, port); }
void ClientApi::login(const QString &phone)
{
    m_phone = phone.trimmed(); emit loginBusy(true);
    if (!m_connection.isConnected()) return;
    m_loginRequest = m_connection.nextRequestId();
    m_pendingRequests.insert(m_loginRequest, QStringLiteral("登录"));
    if (!m_connection.send(Charging::MessageType::UserLoginRequest, m_loginRequest, {{QStringLiteral("phone"), m_phone}})) emit loginBusy(false);
    QTimer::singleShot(8000, this, [this] {
        if (!m_pendingRequests.contains(m_loginRequest)) return;
        m_pendingRequests.remove(m_loginRequest);
        emit loginBusy(false);
        emit requestTimedOut(QStringLiteral("登录"));
        emit loginFailed(QStringLiteral("登录请求超时（8秒）"));
    });
}
void ClientApi::requestProfile()
{
    sendRequest(Charging::MessageType::UserProfileRequest, {}, QStringLiteral("资料读取"));
}
void ClientApi::logout() { const auto id = m_connection.nextRequestId(); if (!m_connection.send(Charging::MessageType::LogoutRequest, id)) emit loggedOut(); else m_pendingRequests.insert(id, QStringLiteral("退出登录")); }
void ClientApi::requestUnsupported(const QString &feature) { emit featureUnavailable(feature + QStringLiteral("接口尚未由服务端提供，等待联调")); }
void ClientApi::geocodeAddress(const QString &address, const QString &region)
{
    const QString normalizedAddress = address.trimmed();
    if (normalizedAddress.isEmpty() || normalizedAddress.size() > 200) {
        emit addressResolutionFailed(
            QStringLiteral("地址长度必须为 1—200 个字符"));
        return;
    }
    if (m_geocodeRequest != 0) return;
    if (!m_connection.isConnected()) {
        emit addressResolutionFailed(
            QStringLiteral("当前未连接服务器，无法解析地址"));
        return;
    }

    const quint32 requestId = m_connection.nextRequestId();
    if (!m_connection.send(
            Charging::MessageType::MapGeocodeRequest, requestId,
            {{QStringLiteral("address"), normalizedAddress},
             {QStringLiteral("region"), region.trimmed()}})) {
        emit addressResolutionFailed(QStringLiteral("地址解析请求发送失败"));
        return;
    }
    m_geocodeRequest = requestId;
    m_pendingRequests.insert(requestId, QStringLiteral("地址解析"));
    QTimer::singleShot(8'000, this, [this, requestId] {
        if (m_geocodeRequest != requestId) return;
        m_pendingRequests.remove(requestId);
        m_geocodeRequest = 0;
        emit requestTimedOut(QStringLiteral("地址解析"));
        emit addressResolutionFailed(
            QStringLiteral("地址解析请求超时，请稍后重试"));
    });
}
void ClientApi::requestStations(const QString &region, const QString &address,
                                double latitude, double longitude)
{
    QJsonObject payload;
    if (std::isfinite(latitude) && std::isfinite(longitude)) {
        payload = {{QStringLiteral("latitude"), latitude},
                   {QStringLiteral("longitude"), longitude},
                   {QStringLiteral("radiusKm"), 10},
                   {QStringLiteral("sort"), QStringLiteral("distance")}};
    } else {
        payload = {{QStringLiteral("region"), region.trimmed()},
                   {QStringLiteral("address"), address.trimmed()},
                   {QStringLiteral("sort"), QStringLiteral("name")}};
    }
    sendRequest(Charging::MessageType::StationListRequest, payload,
                QStringLiteral("站点查询"));
}
void ClientApi::requestPiles(qint64 stationId){ sendRequest(Charging::MessageType::PileListRequest, {{"stationId",stationId}}, QStringLiteral("电桩查询")); }
void ClientApi::requestOrderHistory()
{
    if (!m_connection.isConnected()) {
        emit orderHistoryFailed(QStringLiteral("当前未连接服务器，请稍后重试"));
        return;
    }
    sendRequest(Charging::MessageType::OrderHistoryRequest, {},
                QStringLiteral("历史订单"));
}

void ClientApi::requestFavorites()
{
    if (!m_connection.isConnected()) {
        emit favoritesFailed(QStringLiteral("当前未连接服务器，请稍后重试"));
        return;
    }
    sendRequest(Charging::MessageType::FavoriteListRequest, {},
                QStringLiteral("收藏站点"));
}

void ClientApi::toggleFavorite(qint64 stationId, bool favorited)
{
    if (stationId <= 0) {
        emit favoriteUpdateFailed(stationId, QStringLiteral("充电站编号无效"));
        return;
    }
    if (m_pendingFavoriteStations.contains(stationId)) return;
    if (!m_connection.isConnected()) {
        emit favoriteUpdateFailed(stationId, QStringLiteral("当前未连接服务器"));
        return;
    }

    const quint32 requestId = m_connection.nextRequestId();
    if (!m_connection.send(
            Charging::MessageType::FavoriteToggleRequest, requestId,
            {{QStringLiteral("stationId"), stationId},
             {QStringLiteral("favorited"), favorited}})) {
        emit favoriteUpdateFailed(stationId, QStringLiteral("收藏请求发送失败"));
        return;
    }
    m_pendingRequests.insert(requestId, QStringLiteral("收藏状态更新"));
    m_favoriteRequests.insert(requestId, stationId);
    m_pendingFavoriteStations.insert(stationId);
    QTimer::singleShot(8000, this, [this, requestId, stationId] {
        if (!m_favoriteRequests.remove(requestId)) return;
        m_pendingRequests.remove(requestId);
        m_pendingFavoriteStations.remove(stationId);
        emit favoriteUpdateFailed(stationId,
                                  QStringLiteral("收藏请求超时，状态已恢复"));
    });
}

void ClientApi::updateProfile(const QString &nickname,
                              const QString &avatarBase64)
{
    QJsonObject payload;
    if (!nickname.trimmed().isEmpty())
        payload.insert(QStringLiteral("nickname"), nickname.trimmed());
    if (!avatarBase64.isEmpty())
        payload.insert(QStringLiteral("avatarBase64"), avatarBase64);
    if (payload.isEmpty()) {
        emit profileUpdateFailed(QStringLiteral("没有需要更新的资料"));
        return;
    }
    sendRequest(Charging::MessageType::UserProfileUpdateRequest, payload,
                QStringLiteral("资料更新"));
}

void ClientApi::recharge(qint64 amountCents)
{
    sendRequest(
        Charging::MessageType::WalletRechargeRequest,
        {{QStringLiteral("amountCents"), amountCents},
         {QStringLiteral("transactionId"),
          QUuid::createUuid().toString(QUuid::WithoutBraces)}},
        QStringLiteral("钱包充值"));
}

void ClientApi::requestWalletLedger()
{
    sendRequest(Charging::MessageType::WalletLedgerRequest,
                {{QStringLiteral("page"), 1},
                 {QStringLiteral("pageSize"), 100}},
                QStringLiteral("充值记录"));
}

void ClientApi::createReservation(qint64 stationId, qint64 pileId,
                                  int durationMinutes)
{
    sendRequest(Charging::MessageType::ReservationCreateRequest,
                {{QStringLiteral("stationId"), stationId},
                 {QStringLiteral("pileId"), pileId},
                 {QStringLiteral("durationMinutes"), durationMinutes}},
                QStringLiteral("创建预约"));
}

void ClientApi::cancelReservation(qint64 reservationId)
{
    sendRequest(Charging::MessageType::ReservationCancelRequest,
                {{QStringLiteral("reservationId"), reservationId}},
                QStringLiteral("取消预约"));
}

void ClientApi::requestActiveOrder()
{
    sendRequest(Charging::MessageType::ActiveOrderRequest, {},
                QStringLiteral("活动订单"));
}

void ClientApi::startCharging(qint64 reservationId)
{
    sendRequest(Charging::MessageType::ChargingStartRequest,
                {{QStringLiteral("reservationId"), reservationId}},
                QStringLiteral("开始充电"));
}

void ClientApi::stopCharging(qint64 orderId)
{
    sendRequest(Charging::MessageType::ChargingStopRequest,
                {{QStringLiteral("orderId"), orderId}},
                QStringLiteral("停止充电"));
}

void ClientApi::sendRequest(Charging::MessageType type, const QJsonObject &payload, const QString &feature)
{
    if (!m_connection.isConnected()) {
        emitRequestFailure(feature,
                           QStringLiteral("%1失败：当前未连接服务器").arg(feature));
        return;
    }
    const quint32 requestId = m_connection.nextRequestId();
    if (!m_connection.send(type, requestId, payload)) {
        emitRequestFailure(feature,
                           QStringLiteral("%1请求发送失败").arg(feature));
        return;
    }
    m_pendingRequests.insert(requestId, feature);
    QTimer::singleShot(8000, this, [this, requestId] {
        const auto it = m_pendingRequests.find(requestId);
        if (it == m_pendingRequests.end()) return;
        const QString feature = it.value();
        m_pendingRequests.erase(it);
        if (requestId == m_loginRequest) emit loginBusy(false);
        emit requestTimedOut(feature);
        emitRequestFailure(feature,
                           QStringLiteral("%1请求超时（8秒）").arg(feature));
    });
}

void ClientApi::emitRequestFailure(const QString &feature,
                                   const QString &message)
{
    if (feature == QStringLiteral("资料更新"))
        emit profileUpdateFailed(message);
    else if (feature == QStringLiteral("钱包充值"))
        emit rechargeFailed(message);
    else if (feature == QStringLiteral("充值记录"))
        emit walletLedgerFailed(message);
    else if (feature == QStringLiteral("创建预约")
             || feature == QStringLiteral("取消预约"))
        emit reservationFailed(message);
    else if (feature == QStringLiteral("活动订单"))
        emit activeOrderFailed(message);
    else if (feature == QStringLiteral("开始充电")
             || feature == QStringLiteral("停止充电"))
        emit chargingActionFailed(message);
    else
        emit featureUnavailable(message);
}

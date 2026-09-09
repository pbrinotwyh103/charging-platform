#include "api/clientapi.h"
#include "protocol/errorcodes.h"
#include <QJsonArray>
#include <QUuid>

ClientApi::ClientApi(QObject *parent) : QObject(parent), m_geocoder(this)
{
    m_avatarUpdateTimer.setSingleShot(true);
    m_avatarUpdateTimer.setInterval(15'000);
    m_nicknameUpdateTimer.setSingleShot(true);
    m_nicknameUpdateTimer.setInterval(15'000);
    m_rechargeTimer.setSingleShot(true);
    m_rechargeTimer.setInterval(15'000);
    m_ledgerTimer.setSingleShot(true);
    m_ledgerTimer.setInterval(15'000);
    m_orderHistoryTimer.setSingleShot(true);
    m_orderHistoryTimer.setInterval(15'000);
    m_favoriteTimer.setSingleShot(true);
    m_favoriteTimer.setInterval(15'000);
    m_favoriteListTimer.setSingleShot(true);
    m_favoriteListTimer.setInterval(15'000);
    m_stationTimer.setSingleShot(true);
    m_stationTimer.setInterval(15'000);
    m_pileTimer.setSingleShot(true);
    m_pileTimer.setInterval(15'000);
    m_connection.setClientName(QStringLiteral("user-client"));
    connect(&m_connection, &Charging::ClientConnection::connected, this, [this] {
        emit connectionStatus(QStringLiteral("已连接服务器"), true);
        if (!m_phone.isEmpty()) login(m_phone);
    });
    connect(&m_connection, &Charging::ClientConnection::disconnected, this, [this] {
        emit connectionStatus(QStringLiteral("服务器连接已断开，数据可能已过期"), false);
        if (m_avatarUpdateRequest != 0) {
            m_avatarUpdateRequest = 0;
            m_avatarUpdateTimer.stop();
            emit avatarUpdateFailed(QStringLiteral("服务器连接已断开"));
        }
        if (m_nicknameUpdateRequest != 0) {
            m_nicknameUpdateRequest = 0;
            m_nicknameUpdateTimer.stop();
            emit nicknameUpdateFailed(QStringLiteral("服务器连接已断开"));
        }
        if (m_rechargeRequest != 0) {
            m_rechargeRequest = 0;
            m_rechargeTimer.stop();
            emit rechargeFailed(QStringLiteral("服务器连接已断开"));
        }
        if (m_ledgerRequest != 0) {
            m_ledgerRequest = 0;
            m_ledgerTimer.stop();
            emit ledgerFailed(QStringLiteral("服务器连接已断开"));
        }
        if (m_orderHistoryRequest != 0) {
            m_orderHistoryRequest = 0;
            m_orderHistoryTimer.stop();
            emit orderHistoryFailed(QStringLiteral("服务器连接已断开"));
        }
        if (m_favoriteRequest != 0) {
            const qint64 stationId = m_favoriteStationId;
            m_favoriteRequest = 0;
            m_favoriteTimer.stop();
            emit favoriteUpdateFailed(stationId, QStringLiteral("服务器连接已断开"));
        }
        if (m_favoriteListRequest != 0) {
            m_favoriteListRequest = 0;
            m_favoriteListTimer.stop();
            emit favoritesFailed(QStringLiteral("服务器连接已断开"));
        }
        if (m_geocoder.activeRequestId() != 0) {
            m_geocoder.cancel();
            emit stationGeocodingFailed(QStringLiteral("服务器连接已断开"));
            emit stationSearchFailed(QStringLiteral("服务器连接已断开"));
        }
        if (m_stationRequest != 0) {
            m_stationRequest = 0;
            m_stationTimer.stop();
            emit stationSearchFailed(QStringLiteral("服务器连接已断开"));
        }
        if (m_pileRequest != 0) {
            const qint64 stationId = m_pileStationId;
            m_pileRequest = 0;
            m_pileTimer.stop();
            emit pileSearchFailed(stationId, QStringLiteral("服务器连接已断开"));
        }
    });
    connect(&m_connection, &Charging::ClientConnection::socketError, this,
            [this](const QString &e) { emit connectionStatus(QStringLiteral("连接失败：%1").arg(e), false); });
    connect(&m_connection, &Charging::ClientConnection::reconnectScheduled, this,
            [this](int a, int d) { emit connectionStatus(QStringLiteral("第%1次重连将在%2秒后进行").arg(a).arg(d / 1000.0, 0, 'f', 1), false); });
    connect(&m_connection, &Charging::ClientConnection::messageReceived, this,
            [this](const Charging::Message &m) {
        if (m.header.requestId == m_loginRequest && m.header.messageType == Charging::MessageType::UserLoginResponse) {
            emit loginBusy(false);
            if (m.header.statusCode != Charging::ErrorCode::Success) {
                emit loginFailed(Charging::errorMessage(m.header.statusCode, m.payload.value(QStringLiteral("message")).toString()));
            } else {
                emit loginSucceeded(m.payload);
            }
        } else if (m.header.messageType == Charging::MessageType::StationListResponse) {
            if (m_favoriteListRequest != 0 && m.header.requestId == m_favoriteListRequest) {
                m_favoriteListRequest = 0;
                m_favoriteListTimer.stop();
                if (m.header.statusCode == Charging::ErrorCode::Success) {
                    // Treat the server response as an allow-list. This keeps
                    // an accidentally broad station response from rendering
                    // ordinary stations as favorites in the profile page.
                    QJsonArray favorites;
                    for (const auto &value : m.payload.value(QStringLiteral("items")).toArray()) {
                        const auto station = value.toObject();
                        if (station.value(QStringLiteral("stationId")).toInteger() > 0
                            && station.value(QStringLiteral("favorited")).toBool())
                            favorites.append(station);
                    }
                    emit favoritesReceived(favorites);
                } else
                    emit favoritesFailed(Charging::errorMessage(
                        m.header.statusCode, m.payload.value(QStringLiteral("message")).toString()));
            } else if (m.header.requestId != m_stationRequest || m_stationRequest == 0) {
                return;
            } else if (m.header.statusCode == Charging::ErrorCode::Success) {
                m_stationRequest = 0;
                m_stationTimer.stop();
                emit stationsReceived(m.payload.value(QStringLiteral("items")).toArray());
            }
            else {
                m_stationRequest = 0;
                m_stationTimer.stop();
                const QString message = QStringLiteral("站点查询失败：%1").arg(
                    Charging::errorMessage(m.header.statusCode,
                        m.payload.value(QStringLiteral("message")).toString()));
                emit stationSearchFailed(message);
                emit featureUnavailable(message);
            }
        } else if (m.header.messageType == Charging::MessageType::PileListResponse) {
            if (m.header.requestId != m_pileRequest || m_pileRequest == 0) return;
            const qint64 stationId = m_pileStationId;
            m_pileRequest = 0;
            m_pileTimer.stop();
            if (m.header.statusCode == Charging::ErrorCode::Success)
                emit pilesReceived(m.payload.value(QStringLiteral("items")).toArray());
            else {
                const QString message = QStringLiteral("电桩查询失败：%1").arg(
                    Charging::errorMessage(m.header.statusCode,
                        m.payload.value(QStringLiteral("message")).toString()));
                emit pileSearchFailed(stationId, message);
                emit featureUnavailable(message);
            }
        } else if (m.header.messageType == Charging::MessageType::UserProfileResponse) {
            if (m.header.statusCode == Charging::ErrorCode::Success) emit profileReceived(m.payload);
            else emit featureUnavailable(QStringLiteral("资料读取失败：%1").arg(m.payload.value(QStringLiteral("message")).toString()));
        } else if (m.header.messageType == Charging::MessageType::UserProfileUpdateResponse) {
            const bool avatarResponse = m.header.requestId == m_avatarUpdateRequest;
            const bool nicknameResponse = m.header.requestId == m_nicknameUpdateRequest;
            if (avatarResponse) {
                m_avatarUpdateRequest = 0;
                m_avatarUpdateTimer.stop();
            }
            if (nicknameResponse) {
                m_nicknameUpdateRequest = 0;
                m_nicknameUpdateTimer.stop();
            }
            if (m.header.statusCode == Charging::ErrorCode::Success) {
                emit profileReceived(m.payload);
                if (avatarResponse) emit avatarUpdateSucceeded(m.payload);
                if (nicknameResponse) emit nicknameUpdateSucceeded(m.payload);
            } else {
                const QString message = Charging::errorMessage(
                    m.header.statusCode, m.payload.value(QStringLiteral("message")).toString());
                if (avatarResponse) emit avatarUpdateFailed(message);
                if (nicknameResponse) emit nicknameUpdateFailed(message);
                emit featureUnavailable(QStringLiteral("资料保存失败：%1").arg(message));
            }
        } else if (m.header.messageType == Charging::MessageType::WalletRechargeResponse) {
            const bool rechargeResponse = m.header.requestId == m_rechargeRequest;
            if (rechargeResponse) {
                m_rechargeRequest = 0;
                m_rechargeTimer.stop();
            }
            if (m.header.statusCode == Charging::ErrorCode::Success) {
                emit walletChanged(m.payload);
            } else {
                const QString message = Charging::errorMessage(
                    m.header.statusCode, m.payload.value(QStringLiteral("message")).toString());
                if (rechargeResponse) emit rechargeFailed(message);
                emit featureUnavailable(QStringLiteral("充值失败：%1").arg(message));
            }
        } else if (m.header.messageType == Charging::MessageType::WalletLedgerResponse) {
            const bool ledgerResponse = m.header.requestId == m_ledgerRequest;
            if (ledgerResponse) {
                m_ledgerRequest = 0;
                m_ledgerTimer.stop();
            }
            if (m.header.statusCode == Charging::ErrorCode::Success) {
                emit ledgerReceived(m.payload.value(QStringLiteral("items")).toArray());
            } else {
                const QString message = Charging::errorMessage(
                    m.header.statusCode, m.payload.value(QStringLiteral("message")).toString());
                if (ledgerResponse) emit ledgerFailed(message);
                emit featureUnavailable(QStringLiteral("流水读取失败：%1").arg(message));
            }
        } else if (m.header.messageType == Charging::MessageType::OrderHistoryResponse) {
            if (m.header.requestId != m_orderHistoryRequest) return;
            m_orderHistoryRequest = 0;
            m_orderHistoryTimer.stop();
            if (m.header.statusCode == Charging::ErrorCode::Success)
                emit orderHistoryReceived(m.payload.value(QStringLiteral("items")).toArray());
            else {
                const QString message = Charging::errorMessage(
                    m.header.statusCode, m.payload.value(QStringLiteral("message")).toString());
                emit orderHistoryFailed(message);
                emit featureUnavailable(QStringLiteral("订单读取失败：%1").arg(message));
            }
        } else if (m.header.messageType == Charging::MessageType::FavoriteToggleResponse) {
            if (m.header.requestId != m_favoriteRequest) return;
            const qint64 stationId = m_favoriteStationId;
            m_favoriteRequest = 0;
            m_favoriteTimer.stop();
            if (m.header.statusCode == Charging::ErrorCode::Success)
                emit favoriteChanged(m.payload);
            else {
                const QString message = Charging::errorMessage(
                    m.header.statusCode, m.payload.value(QStringLiteral("message")).toString());
                emit favoriteUpdateFailed(stationId, message);
                emit featureUnavailable(QStringLiteral("收藏操作失败：%1").arg(message));
            }
        } else if (m.header.messageType == Charging::MessageType::ReservationCreateResponse) {
            if (m.header.statusCode == Charging::ErrorCode::Success) emit reservationCreated(m.payload);
            else {
                QString message = m.payload.value(QStringLiteral("message")).toString().trimmed();
                const QString reason = m.payload.value(QStringLiteral("reason")).toString();
                // Older servers only returned the generic conflict text. Use
                // the stable reason field to keep the user-facing guidance
                // actionable after a mixed-version upgrade.
                if (reason == QStringLiteral("insufficient_balance"))
                    message = QStringLiteral("余额不足，请先充值后再预约");
                else if (reason == QStringLiteral("user_frozen"))
                    message = QStringLiteral("账号已被冻结，暂时无法预约");
                else if (reason == QStringLiteral("order_conflict"))
                    message = QStringLiteral("您已有进行中的充电订单，暂时无法预约");
                else if (reason == QStringLiteral("reservation_conflict"))
                    message = QStringLiteral("您已有生效中的预约，请先取消后再预约");
                else if (reason == QStringLiteral("pile_unavailable"))
                    message = QStringLiteral("该充电桩刚刚被占用，请刷新后选择其他空闲桩");
                if (message.isEmpty()) message = QStringLiteral("当前状态不允许预约操作");
                emit featureUnavailable(QStringLiteral("预约失败：%1").arg(message));
            }
        } else if (m.header.messageType == Charging::MessageType::ChargingStartResponse ||
                   m.header.messageType == Charging::MessageType::ActiveOrderResponse ||
                   m.header.messageType == Charging::MessageType::ChargingProgressPush) {
            if (m.header.statusCode != Charging::ErrorCode::Success)
                emit featureUnavailable(QStringLiteral("充电状态读取失败：%1").arg(m.payload.value(QStringLiteral("message")).toString()));
            else if (m.header.messageType != Charging::MessageType::ActiveOrderResponse ||
                     m.payload.value(QStringLiteral("active")).toBool())
                emit chargingSnapshotReceived(m.payload);
        } else if (m.header.messageType == Charging::MessageType::ChargingStopResponse ||
                   m.header.messageType == Charging::MessageType::ChargingStoppedPush) {
            if (m.header.statusCode == Charging::ErrorCode::Success) {
                emit chargingStopped(m.payload);
                if (m.payload.contains(QStringLiteral("balanceCents"))) emit walletSettled(m.payload);
            }
            else emit featureUnavailable(QStringLiteral("停止充电失败：%1").arg(m.payload.value(QStringLiteral("message")).toString()));
        } else if (m.header.messageType == Charging::MessageType::LogoutResponse) emit loggedOut();
    });
    connect(&m_geocoder, &TencentGeocoder::started, this,
            [this](quint64, const QString &address) {
        emit stationGeocodingStarted(address);
    });
    connect(&m_geocoder, &TencentGeocoder::resolved, this,
            [this](quint64, double latitude, double longitude) {
        emit stationGeocodingSucceeded();
        emit stationSearchLocationResolved(latitude, longitude);
        sendStationRequest(m_pendingStationRegion, {}, latitude, longitude, true);
    });
    connect(&m_geocoder, &TencentGeocoder::failed, this, [this](quint64, const QString &message) {
        emit stationGeocodingFailed(message);
        emit stationSearchNotice(
            QStringLiteral("%1，已改用站名/地址文本搜索。").arg(message));
        sendStationRequest(m_pendingStationRegion, m_pendingStationAddress, 0, 0, false);
    });
    connect(&m_avatarUpdateTimer, &QTimer::timeout, this, [this] {
        if (m_avatarUpdateRequest == 0) return;
        m_avatarUpdateRequest = 0;
        emit avatarUpdateFailed(QStringLiteral("请求超时，保存结果尚未确认"));
    });
    connect(&m_nicknameUpdateTimer, &QTimer::timeout, this, [this] {
        if (m_nicknameUpdateRequest == 0) return;
        m_nicknameUpdateRequest = 0;
        emit nicknameUpdateFailed(QStringLiteral("请求超时，保存结果尚未确认"));
    });
    connect(&m_rechargeTimer, &QTimer::timeout, this, [this] {
        if (m_rechargeRequest == 0) return;
        m_rechargeRequest = 0;
        emit rechargeFailed(QStringLiteral("请求超时，交易结果尚未确认，请勿立即重复支付"));
    });
    connect(&m_ledgerTimer, &QTimer::timeout, this, [this] {
        if (m_ledgerRequest == 0) return;
        m_ledgerRequest = 0;
        emit ledgerFailed(QStringLiteral("请求超时"));
    });
    connect(&m_orderHistoryTimer, &QTimer::timeout, this, [this] {
        if (m_orderHistoryRequest == 0) return;
        m_orderHistoryRequest = 0;
        emit orderHistoryFailed(QStringLiteral("请求超时"));
    });
    connect(&m_favoriteTimer, &QTimer::timeout, this, [this] {
        if (m_favoriteRequest == 0) return;
        const qint64 stationId = m_favoriteStationId;
        m_favoriteRequest = 0;
        emit favoriteUpdateFailed(stationId, QStringLiteral("请求超时"));
    });
    connect(&m_favoriteListTimer, &QTimer::timeout, this, [this] {
        if (m_favoriteListRequest == 0) return;
        m_favoriteListRequest = 0;
        emit favoritesFailed(QStringLiteral("请求超时"));
    });
    connect(&m_stationTimer, &QTimer::timeout, this, [this] {
        if (m_stationRequest == 0) return;
        m_stationRequest = 0;
        emit stationSearchFailed(QStringLiteral("站点查询超时，请重新搜索"));
    });
    connect(&m_pileTimer, &QTimer::timeout, this, [this] {
        if (m_pileRequest == 0) return;
        const qint64 stationId = m_pileStationId;
        m_pileRequest = 0;
        emit pileSearchFailed(stationId, QStringLiteral("电桩状态查询超时，请重新刷新"));
    });
}

void ClientApi::connectToServer(const QString &host, quint16 port) { m_connection.connectToServer(host, port); }
void ClientApi::login(const QString &phone)
{
    m_phone = phone.trimmed(); emit loginBusy(true);
    if (!m_connection.isConnected()) return;
    m_loginRequest = m_connection.nextRequestId();
    if (!m_connection.send(Charging::MessageType::UserLoginRequest, m_loginRequest, {{QStringLiteral("phone"), m_phone}})) emit loginBusy(false);
}
void ClientApi::requestProfile()
{
    if (!m_connection.send(Charging::MessageType::UserProfileRequest, m_connection.nextRequestId())) emit featureUnavailable(QStringLiteral("当前未连接服务器"));
}
void ClientApi::updateNickname(const QString &nickname)
{
    if (m_nicknameUpdateRequest != 0) {
        emit nicknameUpdateFailed(QStringLiteral("已有昵称保存请求正在进行"));
        return;
    }
    m_nicknameUpdateRequest = m_connection.nextRequestId();
    if (!m_connection.send(Charging::MessageType::UserProfileUpdateRequest,
                           m_nicknameUpdateRequest, {{"nickname", nickname}})) {
        m_nicknameUpdateRequest = 0;
        emit nicknameUpdateFailed(QStringLiteral("当前未连接服务器"));
        return;
    }
    m_nicknameUpdateTimer.start();
}
void ClientApi::updateAvatar(const QString &dataUrl)
{
    if (m_avatarUpdateRequest != 0) {
        emit avatarUpdateFailed(QStringLiteral("已有头像上传正在进行"));
        return;
    }
    if (!dataUrl.startsWith(QStringLiteral("data:image/png;base64,"))) {
        emit avatarUpdateFailed(QStringLiteral("头像数据格式无效"));
        return;
    }
    m_avatarUpdateRequest = m_connection.nextRequestId();
    if (!m_connection.send(Charging::MessageType::UserProfileUpdateRequest,
                           m_avatarUpdateRequest, {{"avatarBase64", dataUrl}})) {
        m_avatarUpdateRequest = 0;
        emit avatarUpdateFailed(QStringLiteral("当前未连接服务器"));
        return;
    }
    m_avatarUpdateTimer.start();
}
void ClientApi::recharge(qint64 cents)
{
    if (m_rechargeRequest != 0) {
        emit rechargeFailed(QStringLiteral("已有充值请求正在处理"));
        return;
    }
    if (cents < 100 || cents > 500000) {
        emit rechargeFailed(QStringLiteral("充值金额需为 1.00–5000.00 元"));
        return;
    }
    const QString transactionId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    m_rechargeRequest = m_connection.nextRequestId();
    if (!m_connection.send(Charging::MessageType::WalletRechargeRequest,
                           m_rechargeRequest,
                           {{"amountCents", cents}, {"transactionId", transactionId}})) {
        m_rechargeRequest = 0;
        emit rechargeFailed(QStringLiteral("当前未连接服务器"));
        return;
    }
    m_rechargeTimer.start();
}
void ClientApi::requestLedger()
{
    if (m_ledgerRequest != 0) return;
    emit ledgerLoading();
    m_ledgerRequest = m_connection.nextRequestId();
    if (!m_connection.send(Charging::MessageType::WalletLedgerRequest,
                           m_ledgerRequest, {{"page", 1}, {"pageSize", 50}})) {
        m_ledgerRequest = 0;
        emit ledgerFailed(QStringLiteral("当前未连接服务器"));
        return;
    }
    m_ledgerTimer.start();
}
void ClientApi::toggleFavorite(qint64 stationId, bool favorited)
{
    if (m_favoriteRequest != 0) {
        emit favoriteUpdateFailed(stationId, QStringLiteral("已有收藏操作正在处理"));
        return;
    }
    m_favoriteStationId = stationId;
    m_favoriteRequest = m_connection.nextRequestId();
    if (!m_connection.send(Charging::MessageType::FavoriteToggleRequest,
                           m_favoriteRequest,
                           {{"stationId", stationId}, {"favorited", favorited}})) {
        m_favoriteRequest = 0;
        emit favoriteUpdateFailed(stationId, QStringLiteral("当前未连接服务器"));
        return;
    }
    m_favoriteTimer.start();
}
void ClientApi::requestFavorites()
{
    if (m_favoriteListRequest != 0) return;
    emit favoritesLoading();
    m_favoriteListRequest = m_connection.nextRequestId();
    if (!m_connection.send(Charging::MessageType::StationListRequest,
                           m_favoriteListRequest,
                           {{"favoritesOnly", true}, {"page", 1}, {"pageSize", 50}, {"sort", "name"}})) {
        m_favoriteListRequest = 0;
        emit favoritesFailed(QStringLiteral("当前未连接服务器"));
        return;
    }
    m_favoriteListTimer.start();
}
void ClientApi::createReservation(qint64 stationId, qint64 pileId) { if (!m_connection.send(Charging::MessageType::ReservationCreateRequest, m_connection.nextRequestId(), {{"stationId", stationId}, {"pileId", pileId}})) emit featureUnavailable(QStringLiteral("当前未连接服务器")); }
void ClientApi::startCharging(qint64 reservationId) { if (!m_connection.send(Charging::MessageType::ChargingStartRequest, m_connection.nextRequestId(), {{"reservationId", reservationId}})) emit featureUnavailable(QStringLiteral("当前未连接服务器")); }
void ClientApi::stopCharging(qint64 orderId) { if (!m_connection.send(Charging::MessageType::ChargingStopRequest, m_connection.nextRequestId(), {{"orderId", orderId}})) emit featureUnavailable(QStringLiteral("当前未连接服务器")); }
void ClientApi::requestActiveOrder() { if (!m_connection.send(Charging::MessageType::ActiveOrderRequest, m_connection.nextRequestId())) emit featureUnavailable(QStringLiteral("当前未连接服务器")); }
void ClientApi::requestOrderHistory()
{
    if (m_orderHistoryRequest != 0) return;
    emit orderHistoryLoading();
    m_orderHistoryRequest = m_connection.nextRequestId();
    if (!m_connection.send(Charging::MessageType::OrderHistoryRequest, m_orderHistoryRequest,
                           {{"page", 1}, {"pageSize", 50}})) {
        m_orderHistoryRequest = 0;
        emit orderHistoryFailed(QStringLiteral("当前未连接服务器"));
        return;
    }
    m_orderHistoryTimer.start();
}
void ClientApi::logout()
{
    m_stationRequest = 0;
    m_stationTimer.stop();
    m_pileRequest = 0;
    m_pileTimer.stop();
    m_favoriteListRequest = 0;
    m_favoriteListTimer.stop();
    m_favoriteRequest = 0;
    m_favoriteTimer.stop();
    m_orderHistoryRequest = 0;
    m_orderHistoryTimer.stop();
    if (!m_connection.send(Charging::MessageType::LogoutRequest, m_connection.nextRequestId()))
        emit loggedOut();
}
void ClientApi::requestUnsupported(const QString &feature) { emit featureUnavailable(feature + QStringLiteral("接口尚未由服务端提供，等待联调")); }
void ClientApi::requestStations(const QString &region, const QString &address,
                                double latitude, double longitude)
{
    // A new search supersedes any response still in flight from the previous
    // search (including a request started before address geocoding finished).
    m_stationRequest = 0;
    m_stationTimer.stop();
    m_pendingStationRegion = region == QStringLiteral("全部区域") ? QString() : region.trimmed();
    m_pendingStationAddress = address.trimmed();
    m_geocoder.cancel();
    if (!m_pendingStationAddress.isEmpty()) {
        m_geocoder.lookup(m_pendingStationAddress, m_pendingStationRegion);
        return;
    }
    emit stationGeocodingCleared();
    emit stationSearchLocationResolved(latitude, longitude);
    sendStationRequest(m_pendingStationRegion, {}, latitude, longitude, true);
}

void ClientApi::retryStationGeocoding()
{
    if (m_pendingStationAddress.isEmpty()) {
        emit stationGeocodingFailed(QStringLiteral("没有可重试的地址"));
        return;
    }
    m_geocoder.lookup(m_pendingStationAddress, m_pendingStationRegion);
}

void ClientApi::sendStationRequest(const QString &region, const QString &address,
                                   double latitude, double longitude, bool includeCoordinates)
{
    QJsonObject payload{{"region", region}, {"address", address}, {"radiusKm", 10},
                        {"sort", includeCoordinates ? "distance" : "name"}};
    if (includeCoordinates) {
        payload.insert(QStringLiteral("latitude"), latitude);
        payload.insert(QStringLiteral("longitude"), longitude);
    }
    m_stationRequest = m_connection.nextRequestId();
    if (!m_connection.send(Charging::MessageType::StationListRequest,
                           m_stationRequest, payload)) {
        m_stationRequest = 0;
        const QString message = QStringLiteral("站点查询接口暂不可用");
        emit stationSearchFailed(message);
        emit featureUnavailable(message);
        return;
    }
    m_stationTimer.start();
}
void ClientApi::requestPiles(qint64 stationId)
{
    if (stationId <= 0) {
        emit pileSearchFailed(stationId, QStringLiteral("充电站标识无效"));
        return;
    }
    m_pileRequest = m_connection.nextRequestId();
    m_pileStationId = stationId;
    m_pileTimer.stop();
    if (!m_connection.send(Charging::MessageType::PileListRequest, m_pileRequest,
                           {{"stationId", stationId}})) {
        m_pileRequest = 0;
        const QString message = QStringLiteral("电桩详情接口暂不可用");
        emit pileSearchFailed(stationId, message);
        emit featureUnavailable(message);
        return;
    }
    m_pileTimer.start();
}

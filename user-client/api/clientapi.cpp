#include "api/clientapi.h"
#include "protocol/errorcodes.h"
#include <QFile>
#include <QJsonArray>
#include <QUuid>

ClientApi::ClientApi(QObject *parent) : QObject(parent)
{
    m_connection.setClientName(QStringLiteral("user-client"));
    connect(&m_connection, &Charging::ClientConnection::connected, this, [this] {
        emit connectionStatus(QStringLiteral("已连接服务器"), true);
        if (!m_phone.isEmpty()) login(m_phone);
    });
    connect(&m_connection, &Charging::ClientConnection::disconnected, this, [this] {
        emit connectionStatus(QStringLiteral("服务器连接已断开，数据可能已过期"), false);
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
            if (m.header.statusCode == Charging::ErrorCode::Success) emit stationsReceived(m.payload.value(QStringLiteral("items")).toArray()); else emit featureUnavailable(QStringLiteral("站点查询失败：%1").arg(m.payload.value(QStringLiteral("message")).toString()));
        } else if (m.header.messageType == Charging::MessageType::PileListResponse) {
            if (m.header.statusCode == Charging::ErrorCode::Success) emit pilesReceived(m.payload.value(QStringLiteral("items")).toArray()); else emit featureUnavailable(QStringLiteral("电桩查询失败：%1").arg(m.payload.value(QStringLiteral("message")).toString()));
        } else if (m.header.messageType == Charging::MessageType::UserProfileResponse) {
            if (m.header.statusCode == Charging::ErrorCode::Success) emit profileReceived(m.payload);
            else emit featureUnavailable(QStringLiteral("资料读取失败：%1").arg(m.payload.value(QStringLiteral("message")).toString()));
        } else if (m.header.messageType == Charging::MessageType::UserProfileUpdateResponse) {
            if (m.header.statusCode == Charging::ErrorCode::Success) emit profileReceived(m.payload);
            else emit featureUnavailable(QStringLiteral("资料保存失败：%1").arg(m.payload.value(QStringLiteral("message")).toString()));
        } else if (m.header.messageType == Charging::MessageType::WalletRechargeResponse) {
            if (m.header.statusCode == Charging::ErrorCode::Success) emit walletChanged(m.payload);
            else emit featureUnavailable(QStringLiteral("充值失败：%1").arg(m.payload.value(QStringLiteral("message")).toString()));
        } else if (m.header.messageType == Charging::MessageType::WalletLedgerResponse) {
            if (m.header.statusCode == Charging::ErrorCode::Success) emit ledgerReceived(m.payload.value(QStringLiteral("items")).toArray());
            else emit featureUnavailable(QStringLiteral("流水读取失败：%1").arg(m.payload.value(QStringLiteral("message")).toString()));
        } else if (m.header.messageType == Charging::MessageType::FavoriteToggleResponse) {
            if (m.header.statusCode != Charging::ErrorCode::Success) emit featureUnavailable(QStringLiteral("收藏操作失败：%1").arg(m.payload.value(QStringLiteral("message")).toString()));
        } else if (m.header.messageType == Charging::MessageType::ReservationCreateResponse) {
            if (m.header.statusCode == Charging::ErrorCode::Success) emit reservationCreated(m.payload);
            else emit featureUnavailable(QStringLiteral("预约失败：%1").arg(m.payload.value(QStringLiteral("message")).toString()));
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
            if (m.header.statusCode == Charging::ErrorCode::Success) emit chargingStopped(m.payload);
            else emit featureUnavailable(QStringLiteral("停止充电失败：%1").arg(m.payload.value(QStringLiteral("message")).toString()));
        } else if (m.header.messageType == Charging::MessageType::LogoutResponse) emit loggedOut();
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
void ClientApi::updateNickname(const QString &nickname) { if (!m_connection.send(Charging::MessageType::UserProfileUpdateRequest, m_connection.nextRequestId(), {{"nickname", nickname}})) emit featureUnavailable(QStringLiteral("当前未连接服务器")); }
void ClientApi::updateAvatar(const QString &path) { QFile file(path); if (!file.open(QIODevice::ReadOnly)) { emit featureUnavailable(QStringLiteral("头像文件读取失败")); return; } const QByteArray bytes = file.readAll(); if (bytes.size() > 3 * 1024 * 1024) { emit featureUnavailable(QStringLiteral("头像文件不能超过3 MB")); return; } const QString mime = path.endsWith(QStringLiteral(".png"), Qt::CaseInsensitive) ? QStringLiteral("image/png") : QStringLiteral("image/jpeg"); const QString data = QStringLiteral("data:%1;base64,%2").arg(mime, QString::fromLatin1(bytes.toBase64())); if (!m_connection.send(Charging::MessageType::UserProfileUpdateRequest, m_connection.nextRequestId(), {{"avatarBase64", data}})) emit featureUnavailable(QStringLiteral("当前未连接服务器")); }
void ClientApi::recharge(qint64 cents) { const QString transactionId = QUuid::createUuid().toString(QUuid::WithoutBraces); if (!m_connection.send(Charging::MessageType::WalletRechargeRequest, m_connection.nextRequestId(), {{"amountCents", cents}, {"transactionId", transactionId}})) emit featureUnavailable(QStringLiteral("当前未连接服务器")); }
void ClientApi::requestLedger() { if (!m_connection.send(Charging::MessageType::WalletLedgerRequest, m_connection.nextRequestId(), {{"page", 1}, {"pageSize", 50}})) emit featureUnavailable(QStringLiteral("当前未连接服务器")); }
void ClientApi::toggleFavorite(qint64 stationId, bool favorited) { if (!m_connection.send(Charging::MessageType::FavoriteToggleRequest, m_connection.nextRequestId(), {{"stationId", stationId}, {"favorited", favorited}})) emit featureUnavailable(QStringLiteral("当前未连接服务器")); }
void ClientApi::createReservation(qint64 stationId, qint64 pileId) { if (!m_connection.send(Charging::MessageType::ReservationCreateRequest, m_connection.nextRequestId(), {{"stationId", stationId}, {"pileId", pileId}})) emit featureUnavailable(QStringLiteral("当前未连接服务器")); }
void ClientApi::startCharging(qint64 reservationId) { if (!m_connection.send(Charging::MessageType::ChargingStartRequest, m_connection.nextRequestId(), {{"reservationId", reservationId}})) emit featureUnavailable(QStringLiteral("当前未连接服务器")); }
void ClientApi::stopCharging(qint64 orderId) { if (!m_connection.send(Charging::MessageType::ChargingStopRequest, m_connection.nextRequestId(), {{"orderId", orderId}})) emit featureUnavailable(QStringLiteral("当前未连接服务器")); }
void ClientApi::requestActiveOrder() { if (!m_connection.send(Charging::MessageType::ActiveOrderRequest, m_connection.nextRequestId())) emit featureUnavailable(QStringLiteral("当前未连接服务器")); }
void ClientApi::logout() { if (!m_connection.send(Charging::MessageType::LogoutRequest, m_connection.nextRequestId())) emit loggedOut(); }
void ClientApi::requestUnsupported(const QString &feature) { emit featureUnavailable(feature + QStringLiteral("接口尚未由服务端提供，等待联调")); }
void ClientApi::requestStations(const QString &region,const QString &address,double latitude,double longitude){ if(!m_connection.send(Charging::MessageType::StationListRequest,m_connection.nextRequestId(),{{"region",region},{"address",address},{"latitude",latitude},{"longitude",longitude},{"radiusKm",10},{"sort","distance"}})) emit featureUnavailable(QStringLiteral("站点查询接口暂不可用")); }
void ClientApi::requestPiles(qint64 stationId){ if(!m_connection.send(Charging::MessageType::PileListRequest,m_connection.nextRequestId(),{{"stationId",stationId}})) emit featureUnavailable(QStringLiteral("电桩详情接口暂不可用")); }

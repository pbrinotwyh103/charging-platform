#pragma once

#include "network/clientconnection.h"
#include "map/tencentgeocoder.h"

#include <QObject>
#include <QJsonObject>
#include <QTimer>

class ClientApi final : public QObject
{
    Q_OBJECT
public:
    explicit ClientApi(QObject *parent = nullptr);
    void connectToServer(const QString &host, quint16 port);
    void login(const QString &phone);
    void requestProfile();
    void updateNickname(const QString &nickname);
    void updateAvatar(const QString &dataUrl);
    void recharge(qint64 cents);
    void requestLedger();
    void toggleFavorite(qint64 stationId, bool favorited);
    void requestFavorites();
    void createReservation(qint64 stationId, qint64 pileId);
    void startCharging(qint64 reservationId);
    void stopCharging(qint64 orderId);
    void requestActiveOrder();
    void requestOrderHistory();
    void logout();
    void requestUnsupported(const QString &feature);
    void requestStations(const QString &region, const QString &address, double latitude, double longitude);
    void retryStationGeocoding();
    void requestPiles(qint64 stationId);
    void requestPileByCode(const QString &pileCode);
    void askCustomerService(const QString &question);
    bool connected() const { return m_connection.isConnected(); }

signals:
    void connectionStatus(const QString &, bool);
    void loginBusy(bool);
    void loginSucceeded(const QJsonObject &);
    void loginFailed(const QString &);
    void profileReceived(const QJsonObject &);
    void avatarUpdateSucceeded(const QJsonObject &);
    void avatarUpdateFailed(const QString &);
    void nicknameUpdateSucceeded(const QJsonObject &);
    void nicknameUpdateFailed(const QString &);
    void rechargeFailed(const QString &);
    void ledgerLoading();
    void ledgerFailed(const QString &);
    void loggedOut();
    void featureUnavailable(const QString &);
    void stationSearchFailed(const QString &);
    void stationSearchNotice(const QString &);
    void stationSearchLocationResolved(double latitude, double longitude);
    void stationGeocodingStarted(const QString &address);
    void stationGeocodingSucceeded();
    void stationGeocodingFailed(const QString &message);
    void stationGeocodingCleared();
    void stationsReceived(const QJsonArray &);
    void pilesReceived(const QJsonArray &);
    void pileSearchFailed(qint64 stationId, const QString &message);
    void pileCodeResolved(const QJsonObject &station, const QJsonObject &pile);
    void pileCodeLookupFailed(const QString &message);
    void walletChanged(const QJsonObject &);
    void walletSettled(const QJsonObject &);
    void orderHistoryLoading();
    void orderHistoryReceived(const QJsonArray &);
    void orderHistoryFailed(const QString &);
    void ledgerReceived(const QJsonArray &);
    void favoriteChanged(const QJsonObject &);
    void favoriteUpdateFailed(qint64 stationId, const QString &message);
    void favoritesLoading();
    void favoritesReceived(const QJsonArray &);
    void favoritesFailed(const QString &);
    void reservationCreated(const QJsonObject &);
    void chargingSnapshotReceived(const QJsonObject &);
    void activeOrderChecked(bool active, const QJsonObject &snapshot);
    void chargingStopped(const QJsonObject &);
    void customerServiceAnswered(const QString &answer, const QString &model,
                                 bool modelAvailable);
    void customerServiceFailed(const QString &message);

private:
    void sendStationRequest(const QString &region, const QString &address,
                            double latitude, double longitude, bool includeCoordinates);

    Charging::ClientConnection m_connection;
    TencentGeocoder m_geocoder;
    QString m_pendingStationRegion;
    QString m_pendingStationAddress;
    QString m_phone;
    quint32 m_loginRequest = 0;
    quint32 m_avatarUpdateRequest = 0;
    QTimer m_avatarUpdateTimer;
    quint32 m_nicknameUpdateRequest = 0;
    QTimer m_nicknameUpdateTimer;
    quint32 m_rechargeRequest = 0;
    QTimer m_rechargeTimer;
    quint32 m_ledgerRequest = 0;
    QTimer m_ledgerTimer;
    quint32 m_orderHistoryRequest = 0;
    QTimer m_orderHistoryTimer;
    quint32 m_favoriteRequest = 0;
    qint64 m_favoriteStationId = 0;
    QTimer m_favoriteTimer;
    quint32 m_favoriteListRequest = 0;
    QTimer m_favoriteListTimer;
    quint32 m_stationRequest = 0;
    QTimer m_stationTimer;
    quint32 m_pileRequest = 0;
    qint64 m_pileStationId = 0;
    QTimer m_pileTimer;
};

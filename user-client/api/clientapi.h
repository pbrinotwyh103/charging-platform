#pragma once

#include "network/clientconnection.h"
#include "stores/snapshotstore.h"

#include <QObject>
#include <QJsonArray>
#include <QJsonObject>
#include <QHash>
#include <QSet>

class ClientApi final : public QObject
{
    Q_OBJECT
public:
    explicit ClientApi(QObject *parent = nullptr);
    void connectToServer(const QString &host, quint16 port);
    void login(const QString &phone);
    void requestProfile();
    void logout();
    void requestUnsupported(const QString &feature);
    void geocodeAddress(const QString &address, const QString &region);
    void requestStations(const QString &region, const QString &address, double latitude, double longitude);
    void requestPiles(qint64 stationId);
    void requestOrderHistory();
    void requestFavorites();
    void toggleFavorite(qint64 stationId, bool favorited);
    void updateProfile(const QString &nickname, const QString &avatarBase64 = {});
    void recharge(qint64 amountCents);
    void requestWalletLedger();
    void createReservation(qint64 stationId, qint64 pileId,
                           int durationMinutes = 15);
    void cancelReservation(qint64 reservationId);
    void requestActiveOrder();
    void startCharging(qint64 reservationId);
    void stopCharging(qint64 orderId);
    bool connected() const { return m_connection.isConnected(); }

signals:
    void connectionStatus(const QString &, bool);
    void loginBusy(bool);
    void loginSucceeded(const QJsonObject &);
    void loginFailed(const QString &);
    void profileReceived(const QJsonObject &);
    void loggedOut();
    void featureUnavailable(const QString &);
    void addressResolved(double latitude, double longitude,
                         const QString &formattedAddress);
    void addressResolutionFailed(const QString &message);
    void addressResolutionUnavailable();
    void stationsReceived(const QJsonArray &);
    void stationsFailed(const QString &message);
    void pilesReceived(const QJsonArray &);
    void orderHistoryReceived(const QJsonArray &);
    void orderHistoryFailed(const QString &message);
    void favoritesReceived(const QJsonArray &);
    void favoritesFailed(const QString &message);
    void favoriteUpdated(qint64 stationId, bool favorited,
                         const QString &updatedAt);
    void favoriteUpdateFailed(qint64 stationId, const QString &message);
    void profileUpdated(const QJsonObject &profile);
    void profileUpdateFailed(const QString &message);
    void rechargeSucceeded(const QJsonObject &record);
    void rechargeFailed(const QString &message);
    void walletLedgerReceived(const QJsonArray &records);
    void walletLedgerFailed(const QString &message);
    void reservationCreated(const QJsonObject &reservation);
    void reservationCancelled(const QJsonObject &reservation);
    void reservationFailed(const QString &message);
    void activeOrderReceived(bool active, const QJsonObject &snapshot);
    void activeOrderFailed(const QString &message);
    void chargingStarted(const QJsonObject &snapshot);
    void chargingStopped(const QJsonObject &snapshot);
    void chargingSnapshotReceived(const QJsonObject &snapshot);
    void chargingActionFailed(const QString &message);
    void chargingConnectionChanged(bool connected);
    void requestTimedOut(const QString &feature);

private:
    Charging::ClientConnection m_connection;
    SnapshotStore m_snapshots;
    QString m_phone;
    quint32 m_loginRequest = 0;
    quint32 m_geocodeRequest = 0;
    QHash<quint32, QString> m_pendingRequests;
    QHash<quint32, qint64> m_favoriteRequests;
    QSet<qint64> m_pendingFavoriteStations;
    void sendRequest(Charging::MessageType type, const QJsonObject &payload, const QString &feature);
    void emitRequestFailure(const QString &feature, const QString &message);
};

#pragma once

#include "network/clientconnection.h"

#include <QObject>
#include <QJsonObject>

class ClientApi final : public QObject
{
    Q_OBJECT
public:
    explicit ClientApi(QObject *parent = nullptr);
    void connectToServer(const QString &host, quint16 port);
    void login(const QString &phone);
    void requestProfile();
    void updateNickname(const QString &nickname);
    void updateAvatar(const QString &path);
    void recharge(qint64 cents);
    void requestLedger();
    void toggleFavorite(qint64 stationId, bool favorited);
    void createReservation(qint64 stationId, qint64 pileId);
    void startCharging(qint64 reservationId);
    void stopCharging(qint64 orderId);
    void requestActiveOrder();
    void logout();
    void requestUnsupported(const QString &feature);
    void requestStations(const QString &region, const QString &address, double latitude, double longitude);
    void requestPiles(qint64 stationId);
    bool connected() const { return m_connection.isConnected(); }

signals:
    void connectionStatus(const QString &, bool);
    void loginBusy(bool);
    void loginSucceeded(const QJsonObject &);
    void loginFailed(const QString &);
    void profileReceived(const QJsonObject &);
    void loggedOut();
    void featureUnavailable(const QString &);
    void stationsReceived(const QJsonArray &);
    void pilesReceived(const QJsonArray &);
    void walletChanged(const QJsonObject &);
    void ledgerReceived(const QJsonArray &);
    void favoriteChanged(const QJsonObject &);
    void reservationCreated(const QJsonObject &);
    void chargingSnapshotReceived(const QJsonObject &);
    void chargingStopped(const QJsonObject &);

private:
    Charging::ClientConnection m_connection;
    QString m_phone;
    quint32 m_loginRequest = 0;
};

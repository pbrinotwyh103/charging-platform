#pragma once

#include "network/clientconnection.h"

#include <QHash>
#include <QObject>
#include <QJsonObject>
#include <QTimer>

class AdminController final : public QObject
{
    Q_OBJECT

public:
    explicit AdminController(QObject *parent = nullptr);
    bool isLoggedIn() const;

public slots:
    void connectToServer(const QString &host, quint16 port);
    void login(const QString &username, const QString &password,
               const QString &host, quint16 port);
    void logout();
    void requestAdminCommand(const QString &action,
                             const QJsonObject &parameters = {});

signals:
    void statusTextChanged(const QString &text, bool connected);
    void loginBusyChanged(bool busy);
    void loginSucceeded(const QJsonObject &admin);
    void loginFailed(const QString &message);
    void loggedOut();
    void commandBusyChanged(const QString &action, bool busy);
    void commandSucceeded(const QString &action, const QJsonObject &payload);
    void commandFailed(const QString &action, const QString &message,
                       int errorCode);
    void pushReceived(quint16 messageType, const QJsonObject &payload);

private:
    struct PendingRequest {
        QString action;
        qint64 deadline = 0;
    };

    void sendPendingLogin();
    void handleMessage(const Charging::Message &message);
    void expireRequests();
    void failPendingRequests(const QString &message,
                             Charging::ErrorCode errorCode);

    Charging::ClientConnection m_connection;
    QString m_pendingUsername;
    QString m_pendingPassword;
    quint32 m_loginRequestId = 0;
    bool m_loggedIn = false;
    QHash<quint32, PendingRequest> m_pendingRequests;
    QHash<QString, quint32> m_latestRequestByAction;
    QTimer m_requestTimeoutTimer;
};

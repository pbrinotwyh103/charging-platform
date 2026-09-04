#pragma once

#include "network/clientconnection.h"

#include <QObject>
#include <QJsonObject>

class AdminController final : public QObject
{
    Q_OBJECT

public:
    explicit AdminController(QObject *parent = nullptr);

public slots:
    void connectToServer(const QString &host, quint16 port);
    void login(const QString &username, const QString &password,
               const QString &host, quint16 port);
    void logout();

signals:
    void statusTextChanged(const QString &text, bool connected);
    void loginBusyChanged(bool busy);
    void loginSucceeded(const QJsonObject &admin);
    void loginFailed(const QString &message);
    void loggedOut();

private:
    void sendPendingLogin();

    Charging::ClientConnection m_connection;
    QString m_pendingUsername;
    QString m_pendingPassword;
    quint32 m_loginRequestId = 0;
    bool m_loggedIn = false;
};

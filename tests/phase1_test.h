#pragma once

#include "app/serverapplication.h"
#include "network/clientconnection.h"

#include <QObject>
#include <QTemporaryDir>

class Phase1Test final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void pingAndUnauthorizedGuard();
    void invalidPhoneIsRejected();
    void userAutoRegistrationAndProfile();
    void userCannotUseAdminCommand();
    void adminLoginAndPasswordStorage();

private:
    void connectClient(Charging::ClientConnection &client);
    Charging::Message request(Charging::ClientConnection &client,
                              Charging::MessageType requestType,
                              Charging::MessageType responseType,
                              const QJsonObject &payload = {});

    QTemporaryDir m_temporaryDirectory;
    QString m_databasePath;
    ServerApplication m_server;
};

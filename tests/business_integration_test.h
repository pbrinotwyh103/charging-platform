#pragma once

#include "app/serverapplication.h"
#include "network/clientconnection.h"
#include <QSignalSpy>
#include <QTemporaryDir>
#include <functional>
#include <memory>

class BusinessIntegrationTest final : public QObject
{
    Q_OBJECT
private slots:
    void init();
    void cleanup();
    void unauthorizedResponses_data();
    void unauthorizedResponses();
    void roleGuards();
    void invalidFields_data();
    void invalidFields();
    void userWorkflowAndPushes();
    void faultsReachOwnerAndAdministrators();
    void reservationExpiryJob();
    void duplicateRequestLifecycle();
    void adminScopeComesFromSession();
    void logoutDiscardsPendingProfile();
    void directRequestMappingsAndLifecycle();
    void directTimeoutKeepsRequestReserved();
    void directAdminScopeAndDisconnect();
    void directSessionReportsQueueFailure();
    void directLoginCannotSurviveAuthenticationAba();
    void directAnonymousClearInvalidatesLogin_data();
    void directAnonymousClearInvalidatesLogin();
    void directOutboundEnvelopeBoundaries_data();
    void directOutboundEnvelopeBoundaries();
    void directPaddedAdminRequestIsBounded();
    void directOversizedPushIsRejected();
    void paddedAdminRequestIsBounded();
    void oversizedChargingPushIsAudited();
    void directPeerStreamRejectsReordering();
    void directPeerStreamAcceptsZeroSecondAndFinal();

private:
    void connectClient(Charging::ClientConnection &client);
    Charging::Message request(Charging::ClientConnection &client,
        Charging::MessageType type, Charging::MessageType response,
        const QJsonObject &payload = {}, Charging::ErrorCode status = Charging::ErrorCode::Success,
        quint32 requestId = 0);
    Charging::Message login(Charging::ClientConnection &client, const QString &phone = "13800138801");
    Charging::Message loginAdmin(Charging::ClientConnection &client);
    Charging::Message take(QSignalSpy &spy, Charging::MessageType type,
                           quint32 requestId, int timeout = 3000,
                           const std::function<bool(const Charging::Message &)> &matches = {});
    Charging::Message takeMatching(QSignalSpy &spy,
        const std::function<bool(const Charging::Message &)> &matches, int timeout);
    Charging::Message takeChargingFrame(QSignalSpy &spy, const QJsonValue &orderId, int timeout);
    QString compareNextPeerFrame(QSignalSpy &peer, const Charging::Message &owner,
                                 qint64 *previousSequence, int timeout = 3000);
    QVariant sql(const QString &statement);
    void triggerJob(const char *signal);

    std::unique_ptr<QTemporaryDir> m_directory;
    std::unique_ptr<ServerApplication> m_server;
    QString m_databasePath;
};

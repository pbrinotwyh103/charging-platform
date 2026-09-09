#include "app/appinfo.h"
#include "api/clientapi.h"
#include "ui/usermainwindow.h"
#include "pages/homepage.h"
#include "pages/chargingpage.h"
#include "pages/stationdetailpage.h"
#include "pages/profilepage.h"

#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>

int main(int argc, char *argv[])
{
    QApplication application(argc, argv);
    application.setApplicationName(QStringLiteral("charging_user_client"));
    application.setApplicationVersion(Charging::AppInfo::Version);

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("充电平台用户手机客户端"));
    parser.addHelpOption();
    parser.addVersionOption();
    QCommandLineOption demoOption(
        QStringLiteral("demo"),
        QStringLiteral("使用样例数据体验找桩、预约、充电和钱包完整流程"));
    parser.addOption(demoOption);
    parser.process(application);

    ClientApi controller;
    UserMainWindow window;
    if (!parser.isSet(demoOption)) {
        QObject::connect(&window, &UserMainWindow::loginRequested, &controller,
                         [&controller](const QString &phone, const QString &host, quint16 port) {
            controller.connectToServer(host, port); controller.login(phone);
        });
        QObject::connect(&window, &UserMainWindow::logoutRequested, &controller, &ClientApi::logout);
    }
    QObject::connect(&controller, &ClientApi::connectionStatus,
                     &window, &UserMainWindow::setConnectionStatus);
    QObject::connect(&controller, &ClientApi::loginBusy,
                     &window, &UserMainWindow::setLoginBusy);
    QObject::connect(&controller, &ClientApi::loginSucceeded,
                     &window, &UserMainWindow::showProfile);
    QObject::connect(&controller, &ClientApi::profileReceived,
                     window.findChild<ProfilePage *>(), &ProfilePage::setProfile);
    QObject::connect(&controller, &ClientApi::loginFailed,
                     &window, &UserMainWindow::showLoginError);
    QObject::connect(&controller, &ClientApi::loggedOut,
                     &window, &UserMainWindow::showLoginPage);
    QObject::connect(&controller, &ClientApi::featureUnavailable,
                     &window, &UserMainWindow::showFeatureMessage);
    if (!parser.isSet(demoOption)) {
        QObject::connect(window.findChild<HomePage *>(), &HomePage::stationsRequested, &controller, &ClientApi::requestStations);
        QObject::connect(window.findChild<HomePage *>(), &HomePage::pileCodeRequested,
                         &controller, &ClientApi::requestPileByCode);
        QObject::connect(window.findChild<StationDetailPage *>(), &StationDetailPage::pilesRequested, &controller, &ClientApi::requestPiles);
        QObject::connect(&window, &UserMainWindow::reservationRequested, &controller, &ClientApi::createReservation);
        QObject::connect(window.findChild<ChargingPage *>(), &ChargingPage::startChargingRequested, &controller, &ClientApi::startCharging);
        QObject::connect(window.findChild<ChargingPage *>(), &ChargingPage::stopChargingRequested, &controller, &ClientApi::stopCharging);
        QObject::connect(&window, &UserMainWindow::activeOrderCheckRequested,
                         &controller, &ClientApi::requestActiveOrder);
    }
    QObject::connect(&controller, &ClientApi::stationsReceived, window.findChild<HomePage *>(), &HomePage::setStations);
    QObject::connect(&controller, &ClientApi::pilesReceived, window.findChild<StationDetailPage *>(), &StationDetailPage::setPiles);
    QObject::connect(&controller, &ClientApi::pileCodeResolved,
                     &window, &UserMainWindow::showDirectPile);
    QObject::connect(&controller, &ClientApi::pileCodeLookupFailed,
                     window.findChild<HomePage *>(), &HomePage::showPileLookupError);
    if (!parser.isSet(demoOption)) {
        QObject::connect(window.findChild<StationDetailPage *>(), &StationDetailPage::favoriteRequested,
                         &controller, &ClientApi::toggleFavorite);
    }
    auto *profile = window.findChild<ProfilePage *>();
    if (!parser.isSet(demoOption)) {
        QObject::connect(profile, &ProfilePage::nicknameUpdateRequested, &controller, &ClientApi::updateNickname);
        QObject::connect(profile, &ProfilePage::avatarUpdateRequested, &controller, &ClientApi::updateAvatar);
        QObject::connect(profile, &ProfilePage::rechargeRequested, &controller, &ClientApi::recharge);
        QObject::connect(&controller, &ClientApi::walletChanged, profile, &ProfilePage::applyWalletResult);
        QObject::connect(&controller, &ClientApi::walletChanged, &controller, [&controller] { controller.requestLedger(); });
        QObject::connect(&controller, &ClientApi::ledgerReceived, profile, &ProfilePage::setLedger);
        QObject::connect(&controller, &ClientApi::reservationCreated, &window, &UserMainWindow::showReservationCreated);
        QObject::connect(&controller, &ClientApi::chargingSnapshotReceived, &window, &UserMainWindow::showChargingSnapshot);
        QObject::connect(&controller, &ClientApi::activeOrderChecked,
                         &window, &UserMainWindow::handleActiveOrderCheck);
        QObject::connect(&controller, &ClientApi::chargingStopped, &window, &UserMainWindow::showChargingStopped);
        QObject::connect(&controller, &ClientApi::loginSucceeded, &controller, [&controller] {
            controller.requestLedger();
            controller.requestActiveOrder();
        });
    }
    if (parser.isSet(demoOption))
        window.showDemoWorkspace();
    window.show();
    return application.exec();
}

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
namespace {
void configureLinuxInputMethod()
{
#ifdef Q_OS_LINUX
    if (qEnvironmentVariableIsEmpty("QT_IM_MODULE")) qputenv("QT_IM_MODULE", "ibus");
    if (qEnvironmentVariableIsEmpty("GTK_IM_MODULE")) qputenv("GTK_IM_MODULE", "ibus");
    if (qEnvironmentVariableIsEmpty("XMODIFIERS")) qputenv("XMODIFIERS", "@im=ibus");

    if (!qEnvironmentVariableIsEmpty("WAYLAND_DISPLAY")) {
        // Keep the window and input method on the desktop's native Wayland path.
        // The portal resolves the live session IBus service through D-Bus and
        // avoids stale display-specific IBus socket files. XWayland focus changes
        // otherwise cancel the preedit after nearly every pinyin key press.
        if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM"))
            qputenv("QT_QPA_PLATFORM", "wayland;xcb");
        qputenv("IBUS_USE_PORTAL", "1");
    }
#endif
}
}

int main(int argc, char *argv[])
{
    configureLinuxInputMethod();
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
    QObject::connect(&controller, &ClientApi::stationSearchFailed,
                     window.findChild<HomePage *>(), &HomePage::showError);
    QObject::connect(&controller, &ClientApi::stationSearchNotice,
                     window.findChild<HomePage *>(), &HomePage::showTextSearchFallback);
    QObject::connect(&controller, &ClientApi::stationSearchLocationResolved,
                     window.findChild<HomePage *>(), &HomePage::setSearchLocation);
    QObject::connect(&controller, &ClientApi::stationGeocodingStarted,
                     window.findChild<HomePage *>(), &HomePage::showGeocodingLoading);
    QObject::connect(&controller, &ClientApi::stationGeocodingSucceeded,
                     window.findChild<HomePage *>(), &HomePage::showGeocodingSucceeded);
    QObject::connect(&controller, &ClientApi::stationGeocodingFailed,
                     window.findChild<HomePage *>(), &HomePage::showGeocodingFailed);
    QObject::connect(&controller, &ClientApi::stationGeocodingCleared,
                     window.findChild<HomePage *>(), &HomePage::clearGeocodingStatus);
    QObject::connect(&controller, &ClientApi::favoriteChanged,
                     &window, &UserMainWindow::showFavoriteChanged);
    QObject::connect(&controller, &ClientApi::favoriteUpdateFailed,
                     &window, &UserMainWindow::showFavoriteUpdateFailed);
    if (!parser.isSet(demoOption)) {
        QObject::connect(window.findChild<HomePage *>(), &HomePage::stationsRequested, &controller, &ClientApi::requestStations);
        QObject::connect(window.findChild<HomePage *>(), &HomePage::geocodingRetryRequested,
                         &controller, &ClientApi::retryStationGeocoding);
        QObject::connect(window.findChild<StationDetailPage *>(), &StationDetailPage::pilesRequested, &controller, &ClientApi::requestPiles);
        QObject::connect(&window, &UserMainWindow::reservationRequested, &controller, &ClientApi::createReservation);
        QObject::connect(window.findChild<ChargingPage *>(), &ChargingPage::startChargingRequested, &controller, &ClientApi::startCharging);
        QObject::connect(window.findChild<ChargingPage *>(), &ChargingPage::stopChargingRequested, &controller, &ClientApi::stopCharging);
    }
    QObject::connect(&controller, &ClientApi::stationsReceived, window.findChild<HomePage *>(), &HomePage::setStations);
    QObject::connect(&controller, &ClientApi::pilesReceived, window.findChild<StationDetailPage *>(), &StationDetailPage::setPiles);
    QObject::connect(&controller, &ClientApi::pileSearchFailed,
                     &window, [&window](qint64 stationId, const QString &message) {
        auto *page = window.findChild<StationDetailPage *>();
        if (page && page->stationId() == stationId) page->showError(message);
    });
    if (!parser.isSet(demoOption)) {
        QObject::connect(window.findChild<StationDetailPage *>(), &StationDetailPage::favoriteRequested,
                         &controller, &ClientApi::toggleFavorite);
    }
    auto *profile = window.findChild<ProfilePage *>();
    if (!parser.isSet(demoOption)) {
        QObject::connect(profile, &ProfilePage::nicknameUpdateRequested, &controller, &ClientApi::updateNickname);
        QObject::connect(&controller, &ClientApi::nicknameUpdateSucceeded,
                         profile, &ProfilePage::nicknameUpdateSucceeded);
        QObject::connect(&controller, &ClientApi::nicknameUpdateFailed,
                         profile, &ProfilePage::nicknameUpdateFailed);
        QObject::connect(profile, &ProfilePage::avatarUpdateRequested, &controller, &ClientApi::updateAvatar);
        QObject::connect(&controller, &ClientApi::avatarUpdateSucceeded,
                         profile, &ProfilePage::avatarUpdateSucceeded);
        QObject::connect(&controller, &ClientApi::avatarUpdateFailed,
                         profile, &ProfilePage::avatarUpdateFailed);
        QObject::connect(profile, &ProfilePage::rechargeRequested, &controller, &ClientApi::recharge);
        QObject::connect(&controller, &ClientApi::walletChanged, profile, &ProfilePage::applyWalletResult);
        QObject::connect(&controller, &ClientApi::walletSettled, profile, &ProfilePage::applySettlementResult);
        QObject::connect(&controller, &ClientApi::rechargeFailed,
                         profile, &ProfilePage::rechargeFailed);
        QObject::connect(&controller, &ClientApi::walletChanged, &controller, [&controller] { controller.requestLedger(); });
        QObject::connect(&controller, &ClientApi::ledgerReceived, profile, &ProfilePage::setLedger);
        QObject::connect(&controller, &ClientApi::ledgerLoading,
                         profile, &ProfilePage::showLedgerLoading);
        QObject::connect(&controller, &ClientApi::ledgerFailed,
                         profile, &ProfilePage::showLedgerError);
        QObject::connect(&controller, &ClientApi::orderHistoryLoading,
                         profile, &ProfilePage::showOrderHistoryLoading);
        QObject::connect(&controller, &ClientApi::orderHistoryReceived,
                         profile, &ProfilePage::setOrderHistory);
        QObject::connect(&controller, &ClientApi::orderHistoryFailed,
                         profile, &ProfilePage::showOrderHistoryError);
        QObject::connect(profile, &ProfilePage::ledgerRefreshRequested,
                         &controller, &ClientApi::requestLedger);
        QObject::connect(&controller, &ClientApi::favoritesLoading,
                         profile, &ProfilePage::showFavoritesLoading);
        QObject::connect(&controller, &ClientApi::favoritesReceived,
                         profile, &ProfilePage::setFavoriteStations);
        QObject::connect(&controller, &ClientApi::favoritesFailed,
                         profile, &ProfilePage::showFavoritesError);
        QObject::connect(profile, &ProfilePage::favoritesRefreshRequested,
                         &controller, &ClientApi::requestFavorites);
        QObject::connect(profile, &ProfilePage::orderHistoryRefreshRequested,
                         &controller, &ClientApi::requestOrderHistory);
        QObject::connect(&controller, &ClientApi::favoriteChanged,
                         &controller, [&controller](const QJsonObject &) { controller.requestFavorites(); });
        QObject::connect(&controller, &ClientApi::reservationCreated, &window, &UserMainWindow::showReservationCreated);
        QObject::connect(&controller, &ClientApi::chargingSnapshotReceived, &window, &UserMainWindow::showChargingSnapshot);
        QObject::connect(&controller, &ClientApi::chargingStopped, &window, &UserMainWindow::showChargingStopped);
        QObject::connect(&controller, &ClientApi::loginSucceeded, &controller, [&controller] {
            controller.requestLedger();
            controller.requestActiveOrder();
            controller.requestFavorites();
            controller.requestOrderHistory();
        });
        QObject::connect(&controller, &ClientApi::walletSettled, &controller,
                         [&controller](const QJsonObject &) {
            controller.requestLedger();
            controller.requestOrderHistory();
        });
    }
    if (parser.isSet(demoOption))
        window.showDemoWorkspace();
    window.show();
    return application.exec();
}

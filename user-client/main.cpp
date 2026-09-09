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
#include <QProcess>
#include <QProcessEnvironment>
#include <QStandardPaths>

namespace {
void configureLinuxInputMethod()
{
#ifdef Q_OS_LINUX
    if (qEnvironmentVariableIsEmpty("QT_IM_MODULE")) qputenv("QT_IM_MODULE", "ibus");
    if (qEnvironmentVariableIsEmpty("GTK_IM_MODULE")) qputenv("GTK_IM_MODULE", "ibus");
    if (qEnvironmentVariableIsEmpty("XMODIFIERS")) qputenv("XMODIFIERS", "@im=ibus");
    // Qt's IBus plugin treats the mere presence of IBUS_USE_PORTAL as true;
    // setting it to "0" still enables the portal path.
    qunsetenv("IBUS_USE_PORTAL");
    qputenv("IBUS_ENABLE_SYNC_MODE", "1");

    // This desktop exposes both Wayland and X11. Qt Wayland and ibus-x11 can
    // otherwise attach to different session buses, causing every preedit update
    // to be cancelled immediately. Keep both sides on X11 in that situation.
    if (!qEnvironmentVariableIsEmpty("WAYLAND_DISPLAY")
        && !qEnvironmentVariableIsEmpty("DISPLAY")
        && qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM")) {
        qputenv("QT_QPA_PLATFORM", "xcb");
        qunsetenv("WAYLAND_DISPLAY");
    }

    if (qgetenv("QT_IM_MODULE") != QByteArrayLiteral("ibus")) return;
    const QString ibus = QStandardPaths::findExecutable(QStringLiteral("ibus"));
    const QString daemon = QStandardPaths::findExecutable(QStringLiteral("ibus-daemon"));
    if (ibus.isEmpty() || daemon.isEmpty()) return;

    QProcessEnvironment ibusEnvironment = QProcessEnvironment::systemEnvironment();
    ibusEnvironment.remove(QStringLiteral("IBUS_USE_PORTAL"));
    ibusEnvironment.insert(QStringLiteral("IBUS_ENABLE_SYNC_MODE"), QStringLiteral("1"));
    ibusEnvironment.remove(QStringLiteral("WAYLAND_DISPLAY"));

    QProcess check;
    check.setProcessEnvironment(ibusEnvironment);
    check.start(ibus, {QStringLiteral("address")});
    check.waitForFinished(800);
    const QByteArray address = check.readAllStandardOutput().trimmed();
    if (!address.isEmpty() && address != QByteArrayLiteral("(null)")) return;
    QProcess startDaemon;
    startDaemon.setProcessEnvironment(ibusEnvironment);
    // Qt's ibus platform plugin talks to the daemon over D-Bus directly. Starting
    // the legacy XIM bridge as well creates a second key-event path on X11.
    startDaemon.start(daemon, {QStringLiteral("--daemonize")});
    startDaemon.waitForFinished(1500);

    // A freshly started private session has no active engine. Prefer the Chinese
    // engine shipped with the desktop image; failure is harmless on systems that
    // use another IBus engine and it can still be selected through the tray.
    QProcess selectEngine;
    selectEngine.setProcessEnvironment(ibusEnvironment);
    selectEngine.start(ibus, {QStringLiteral("engine"), QStringLiteral("libpinyin")});
    selectEngine.waitForFinished(1500);
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
    QObject::connect(&controller, &ClientApi::favoriteChanged,
                     &window, &UserMainWindow::showFavoriteChanged);
    if (!parser.isSet(demoOption)) {
        QObject::connect(window.findChild<HomePage *>(), &HomePage::stationsRequested, &controller, &ClientApi::requestStations);
        QObject::connect(window.findChild<StationDetailPage *>(), &StationDetailPage::pilesRequested, &controller, &ClientApi::requestPiles);
        QObject::connect(&window, &UserMainWindow::reservationRequested, &controller, &ClientApi::createReservation);
        QObject::connect(window.findChild<ChargingPage *>(), &ChargingPage::startChargingRequested, &controller, &ClientApi::startCharging);
        QObject::connect(window.findChild<ChargingPage *>(), &ChargingPage::stopChargingRequested, &controller, &ClientApi::stopCharging);
    }
    QObject::connect(&controller, &ClientApi::stationsReceived, window.findChild<HomePage *>(), &HomePage::setStations);
    QObject::connect(&controller, &ClientApi::pilesReceived, window.findChild<StationDetailPage *>(), &StationDetailPage::setPiles);
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

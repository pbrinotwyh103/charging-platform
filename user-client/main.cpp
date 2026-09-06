#include "app/appinfo.h"
#include "api/clientapi.h"
#include "ui/usermainwindow.h"
#include "pages/homepage.h"
#include "pages/favoritespage.h"
#include "pages/stationdetailpage.h"
#include "pages/profilepage.h"
#include "pages/rechargerecordspage.h"
#include "pages/chargingpage.h"

#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QDateTime>
#include <QFileInfo>
#include <QLibraryInfo>
#include <QProcess>
#include <QTimer>

#include <cmath>

namespace {

bool configureLinuxInputMethod()
{
#ifdef Q_OS_LINUX
    const bool runningInWsl = qEnvironmentVariableIsSet("WSL_DISTRO_NAME");

    // Respect an explicitly configured input method. On a default Ubuntu
    // desktop Qt does not always auto-select its installed IBus bridge when
    // the application is started directly from a terminal or IDE.
    if (!qEnvironmentVariableIsSet("QT_IM_MODULE")) {
        const QString ibusPlugin =
            QLibraryInfo::path(QLibraryInfo::PluginsPath)
            + QStringLiteral("/platforminputcontexts/libibusplatforminputcontextplugin.so");
        if (QFileInfo::exists(ibusPlugin))
            qputenv("QT_IM_MODULE", QByteArrayLiteral("ibus"));
    }

    // WSLg's Wayland input-method protocol can repeatedly dismiss the IBus
    // candidate window. Use its XWayland server instead, and ensure both the
    // application and IBus resolve the same (non-Wayland) session address.
    if (runningInWsl && !qEnvironmentVariableIsSet("QT_QPA_PLATFORM")) {
        qunsetenv("WAYLAND_DISPLAY");
        qputenv("QT_QPA_PLATFORM", QByteArrayLiteral("xcb"));
        qputenv("GDK_BACKEND", QByteArrayLiteral("x11"));
        qputenv("IBUS_USE_PORTAL", QByteArrayLiteral("0"));
        if (!qEnvironmentVariableIsSet("GTK_IM_MODULE"))
            qputenv("GTK_IM_MODULE", QByteArrayLiteral("ibus"));
        if (!qEnvironmentVariableIsSet("XMODIFIERS"))
            qputenv("XMODIFIERS", QByteArrayLiteral("@im=ibus"));
        return true;
    }
#endif
    return false;
}

void ensureWslIbus()
{
#ifdef Q_OS_LINUX
    QProcess addressCheck;
    addressCheck.start(QStringLiteral("ibus"),
                       {QStringLiteral("address")});
    const bool addressFinished = addressCheck.waitForFinished(1000);
    const QByteArray address = addressCheck.readAllStandardOutput().trimmed();
    const bool addressAvailable = addressFinished
        && addressCheck.exitCode() == 0
        && !address.isEmpty()
        && address != QByteArrayLiteral("(null)");

    if (!addressAvailable) {
        QProcess daemon;
        daemon.start(QStringLiteral("ibus-daemon"),
                     {QStringLiteral("--daemonize"),
                      QStringLiteral("--replace"),
                      QStringLiteral("--xim")});
        if (!daemon.waitForFinished(3000)) {
            daemon.kill();
            daemon.waitForFinished();
        }
    }

    // The charging client is Chinese-first. Activating libpinyin avoids WSL
    // intercepting the desktop's usual Super+Space switching shortcut.
    QProcess engine;
    engine.start(QStringLiteral("ibus"),
                 {QStringLiteral("engine"), QStringLiteral("libpinyin")});
    if (!engine.waitForFinished(1500)) {
        engine.kill();
        engine.waitForFinished();
    }
#endif
}

} // namespace

int main(int argc, char *argv[])
{
    const bool useWslIbus = configureLinuxInputMethod();
    if (useWslIbus) ensureWslIbus();

    QApplication application(argc, argv);
    application.setApplicationName(QStringLiteral("charging_user_client"));
    application.setApplicationVersion(Charging::AppInfo::Version);

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("电动汽车充电用户端"));
    parser.addHelpOption();
    parser.addVersionOption();
    QCommandLineOption chargingDemoOption(
        QStringLiteral("demo-charging"),
        QStringLiteral("无需服务器，直接预览并模拟进行中的充电任务"));
    parser.addOption(chargingDemoOption);
    parser.process(application);

    ClientApi controller;
    UserMainWindow window;
    QObject::connect(&window, &UserMainWindow::loginRequested, &controller,
                     [&controller](const QString &phone, const QString &host, quint16 port) {
        controller.connectToServer(host, port); controller.login(phone);
    });
    QObject::connect(&window, &UserMainWindow::logoutRequested, &controller, &ClientApi::logout);
    QObject::connect(&controller, &ClientApi::connectionStatus,
                     &window, &UserMainWindow::setConnectionStatus);
    QObject::connect(&controller, &ClientApi::loginBusy,
                     &window, &UserMainWindow::setLoginBusy);
    QObject::connect(&controller, &ClientApi::loginSucceeded,
                     &window, &UserMainWindow::showProfile);
    QObject::connect(&controller, &ClientApi::loginSucceeded,
                     &controller, [&controller] {
        controller.requestActiveOrder();
    });
    QObject::connect(&controller, &ClientApi::loginFailed,
                     &window, &UserMainWindow::showLoginError);
    QObject::connect(&controller, &ClientApi::loggedOut,
                     &window, &UserMainWindow::showLoginPage);
    QObject::connect(&controller, &ClientApi::featureUnavailable,
                     &window, &UserMainWindow::showFeatureMessage);
    QObject::connect(&controller, &ClientApi::requestTimedOut,
                     &window, [&window](const QString &feature) {
        window.showFeatureMessage(QStringLiteral("%1请求超时，请检查网络后重试").arg(feature));
    });
    auto *home = window.findChild<HomePage *>();
    QObject::connect(home, &HomePage::geocodeRequested,
                     &controller, &ClientApi::geocodeAddress);
    QObject::connect(&controller, &ClientApi::addressResolved,
                     home, &HomePage::setResolvedLocation);
    QObject::connect(&controller, &ClientApi::addressResolutionFailed,
                     home, &HomePage::showAddressResolutionError);
    QObject::connect(&controller, &ClientApi::addressResolutionUnavailable,
                     home, &HomePage::useTextSearchFallback);
    QObject::connect(home, &HomePage::stationsRequested,
                     &controller, &ClientApi::requestStations);
    QObject::connect(window.findChild<StationDetailPage *>(), &StationDetailPage::pilesRequested, &controller, &ClientApi::requestPiles);
    QObject::connect(&controller, &ClientApi::stationsReceived,
                     home, &HomePage::setStations);
    QObject::connect(&controller, &ClientApi::stationsFailed,
                     home, &HomePage::showError);
    QObject::connect(&controller, &ClientApi::pilesReceived, window.findChild<StationDetailPage *>(), &StationDetailPage::setPiles);
    auto *stationDetail = window.findChild<StationDetailPage *>();
    auto *favorites = window.findChild<FavoritesPage *>();
    QObject::connect(stationDetail, &StationDetailPage::favoriteRequested,
                     &controller, &ClientApi::toggleFavorite);
    QObject::connect(&controller, &ClientApi::favoriteUpdated,
                     stationDetail, &StationDetailPage::favoriteUpdated);
    QObject::connect(&controller, &ClientApi::favoriteUpdateFailed,
                     stationDetail, &StationDetailPage::favoriteUpdateFailed);
    QObject::connect(&controller, &ClientApi::favoriteUpdated,
                     home, &HomePage::applyFavorite);
    QObject::connect(&controller, &ClientApi::favoriteUpdated,
                     favorites, &FavoritesPage::applyFavorite);
    QObject::connect(&window, &UserMainWindow::favoriteListRequested,
                     &controller, &ClientApi::requestFavorites);
    QObject::connect(&controller, &ClientApi::favoritesReceived,
                     favorites, &FavoritesPage::setStations);
    QObject::connect(&controller, &ClientApi::favoritesFailed,
                     favorites, &FavoritesPage::showError);
    auto *profile = window.findChild<ProfilePage *>();
    QObject::connect(profile, &ProfilePage::nicknameUpdateRequested,
                     &controller, [&controller](const QString &nickname) {
        controller.updateProfile(nickname);
    });
    QObject::connect(profile, &ProfilePage::avatarPrepared,
                     &controller, [&controller](const QString &base64,
                                                const QString &mimeType) {
        controller.updateProfile({},
            QStringLiteral("data:%1;base64,%2").arg(mimeType, base64));
    });
    QObject::connect(profile, &ProfilePage::rechargeRequested,
                     &controller, &ClientApi::recharge);
    QObject::connect(&controller, &ClientApi::profileReceived,
                     profile, &ProfilePage::setProfile);
    QObject::connect(&controller, &ClientApi::profileUpdateFailed,
                     &window, &UserMainWindow::showFeatureMessage);
    QObject::connect(&controller, &ClientApi::rechargeSucceeded,
                     profile, [profile, &window](const QJsonObject &record) {
        profile->setBalanceCents(
            record.value(QStringLiteral("balanceCents"))
                .toVariant().toLongLong());
        window.showFeatureMessage(QStringLiteral("充值成功，钱包余额已更新"));
    });
    QObject::connect(&controller, &ClientApi::rechargeFailed,
                     &window, &UserMainWindow::showFeatureMessage);
    QObject::connect(profile, &ProfilePage::orderHistoryRequested,
                     &controller, &ClientApi::requestOrderHistory);
    QObject::connect(&window, &UserMainWindow::orderHistoryRequested,
                     &controller, &ClientApi::requestOrderHistory);
    QObject::connect(&controller, &ClientApi::orderHistoryReceived,
                     profile, &ProfilePage::setOrders);
    QObject::connect(&controller, &ClientApi::orderHistoryFailed,
                     profile, &ProfilePage::showOrderError);
    QObject::connect(&controller, &ClientApi::requestTimedOut,
                     profile, [profile](const QString &feature) {
        if (feature == QStringLiteral("历史订单"))
            profile->showOrderError(QStringLiteral("历史订单请求超时，请稍后重试"));
    });
    QObject::connect(&controller, &ClientApi::requestTimedOut,
                     favorites, [favorites](const QString &feature) {
        if (feature == QStringLiteral("收藏站点"))
            favorites->showError(QStringLiteral("收藏站点请求超时，请稍后重试"));
    });
    QObject::connect(&controller, &ClientApi::requestTimedOut,
                     home, [home](const QString &feature) {
        if (feature == QStringLiteral("站点查询"))
            home->showError(QStringLiteral("站点查询超时，请稍后重试"));
    });
    auto *rechargeRecords = window.findChild<RechargeRecordsPage *>();
    QObject::connect(&window, &UserMainWindow::rechargeRecordsRequested,
                     &controller, &ClientApi::requestWalletLedger);
    QObject::connect(&controller, &ClientApi::walletLedgerReceived,
                     rechargeRecords, &RechargeRecordsPage::setRecords);
    QObject::connect(&controller, &ClientApi::walletLedgerFailed,
                     rechargeRecords, &RechargeRecordsPage::showError);

    auto *charging = window.findChild<ChargingPage *>();
    QObject::connect(stationDetail, &StationDetailPage::reservationRequested,
                     &controller, &ClientApi::createReservation);
    QObject::connect(&controller, &ClientApi::reservationCreated,
                     &window, [stationDetail, charging, &window](
                                  const QJsonObject &reservation) {
        stationDetail->reservationAccepted();
        charging->setReservation(reservation);
        window.showChargingPage();
    });
    QObject::connect(&controller, &ClientApi::reservationFailed,
                     stationDetail, &StationDetailPage::reservationFailed);
    QObject::connect(&controller, &ClientApi::reservationFailed,
                     charging, &ChargingPage::showActionError);
    QObject::connect(charging, &ChargingPage::startRequested,
                     &controller, &ClientApi::startCharging);
    QObject::connect(charging, &ChargingPage::cancelRequested,
                     &controller, &ClientApi::cancelReservation);
    QObject::connect(charging, &ChargingPage::stopRequested,
                     &controller, &ClientApi::stopCharging);
    QObject::connect(&controller, &ClientApi::reservationCancelled,
                     charging, [charging](const QJsonObject &) {
        charging->setNoActiveTask();
    });
    QObject::connect(&controller, &ClientApi::chargingSnapshotReceived,
                     charging, &ChargingPage::setSnapshot);
    QObject::connect(&controller, &ClientApi::chargingStarted,
                     &window, [&window](const QJsonObject &) {
        window.showChargingPage();
    });
    QObject::connect(&controller, &ClientApi::chargingActionFailed,
                     charging, &ChargingPage::showActionError);
    QObject::connect(&controller, &ClientApi::activeOrderReceived,
                     charging, [charging, &window](bool active,
                                                   const QJsonObject &snapshot) {
        if (!active) return;
        charging->setSnapshot(snapshot);
        window.showChargingPage();
    });
    QObject::connect(&controller, &ClientApi::activeOrderFailed,
                     &window, &UserMainWindow::showFeatureMessage);
    QObject::connect(&controller, &ClientApi::chargingConnectionChanged,
                     charging, [charging](bool connected) {
        charging->setDisconnected(!connected);
    });

    if (parser.isSet(chargingDemoOption)) {
        window.showProfile({
            {QStringLiteral("nickname"), QStringLiteral("演示用户")},
            {QStringLiteral("phone"), QStringLiteral("13800000000")},
            {QStringLiteral("balanceCents"), 10000}
        });

        int durationSec = 20 * 60;
        qint64 seq = 1;
        double energyKwh = 12.35;
        auto publishDemoSnapshot = [&window, durationSec, seq, energyKwh]() mutable {
            const double powerKw = 58.0 + 3.0 * std::sin(durationSec / 8.0);
            if (seq > 1) energyKwh += powerKw / 3600.0;
            window.setChargingSnapshot({
                {QStringLiteral("orderId"), QStringLiteral("DEMO-001")},
                {QStringLiteral("seq"), seq++},
                {QStringLiteral("status"), QStringLiteral("充电中（演示）")},
                {QStringLiteral("energyKwh"), energyKwh},
                {QStringLiteral("powerKw"), powerKw},
                {QStringLiteral("durationSec"), durationSec++},
                {QStringLiteral("feeCents"), qRound64(energyKwh * 120.0)},
                {QStringLiteral("updatedAt"),
                 QDateTime::currentDateTime().toString(
                     QStringLiteral("yyyy-MM-dd HH:mm:ss"))}
            });
        };
        publishDemoSnapshot();
        window.showChargingPage();

        auto *demoTimer = new QTimer(&window);
        demoTimer->setInterval(1000);
        QObject::connect(demoTimer, &QTimer::timeout, &window,
                         std::move(publishDemoSnapshot));
        demoTimer->start();
    }
    window.show();
    return application.exec();
}

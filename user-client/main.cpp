#include "app/appinfo.h"
#include "api/clientapi.h"
#include "ui/usermainwindow.h"
#include "pages/homepage.h"
#include "pages/stationdetailpage.h"
#include "pages/profilepage.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication application(argc, argv);
    application.setApplicationName(QStringLiteral("charging_user_client"));
    application.setApplicationVersion(Charging::AppInfo::Version);

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
    QObject::connect(&controller, &ClientApi::loginFailed,
                     &window, &UserMainWindow::showLoginError);
    QObject::connect(&controller, &ClientApi::loggedOut,
                     &window, &UserMainWindow::showLoginPage);
    QObject::connect(&controller, &ClientApi::featureUnavailable,
                     &window, &UserMainWindow::showFeatureMessage);
    QObject::connect(window.findChild<HomePage *>(), &HomePage::stationsRequested, &controller, &ClientApi::requestStations);
    QObject::connect(window.findChild<StationDetailPage *>(), &StationDetailPage::pilesRequested, &controller, &ClientApi::requestPiles);
    QObject::connect(&controller, &ClientApi::stationsReceived, window.findChild<HomePage *>(), &HomePage::setStations);
    QObject::connect(&controller, &ClientApi::pilesReceived, window.findChild<StationDetailPage *>(), &StationDetailPage::setPiles);
    QObject::connect(window.findChild<StationDetailPage *>(), &StationDetailPage::favoriteRequested,
                     &controller, [&controller](qint64, bool) {
        controller.requestUnsupported(QStringLiteral("收藏切换"));
    });
    auto *profile = window.findChild<ProfilePage *>();
    QObject::connect(profile, &ProfilePage::nicknameUpdateRequested, &controller, [&controller](const QString &) { controller.requestUnsupported(QStringLiteral("昵称修改")); });
    QObject::connect(profile, &ProfilePage::avatarUpdateRequested, &controller, [&controller](const QString &) { controller.requestUnsupported(QStringLiteral("头像上传")); });
    QObject::connect(profile, &ProfilePage::rechargeRequested, &controller, [&controller](qint64) { controller.requestUnsupported(QStringLiteral("钱包充值")); });
    window.show();
    return application.exec();
}

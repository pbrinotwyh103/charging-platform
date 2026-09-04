#include "app/appinfo.h"
#include "controllers/usercontroller.h"
#include "ui/usermainwindow.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication application(argc, argv);
    application.setApplicationName(QStringLiteral("charging_user_client"));
    application.setApplicationVersion(Charging::AppInfo::Version);

    UserController controller;
    UserMainWindow window;
    QObject::connect(&window, &UserMainWindow::loginRequested,
                     &controller, &UserController::login);
    QObject::connect(&window, &UserMainWindow::logoutRequested,
                     &controller, &UserController::logout);
    QObject::connect(&controller, &UserController::statusTextChanged,
                     &window, &UserMainWindow::setConnectionStatus);
    QObject::connect(&controller, &UserController::loginBusyChanged,
                     &window, &UserMainWindow::setLoginBusy);
    QObject::connect(&controller, &UserController::loginSucceeded,
                     &window, &UserMainWindow::showProfile);
    QObject::connect(&controller, &UserController::loginFailed,
                     &window, &UserMainWindow::showLoginError);
    QObject::connect(&controller, &UserController::loggedOut,
                     &window, &UserMainWindow::showLoginPage);
    window.show();
    return application.exec();
}

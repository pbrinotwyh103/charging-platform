#include "app/appinfo.h"
#include "controllers/admincontroller.h"
#include "ui/adminmainwindow.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication application(argc, argv);
    application.setApplicationName(QStringLiteral("charging_admin_client"));
    application.setApplicationVersion(Charging::AppInfo::Version);

    AdminController controller;
    AdminMainWindow window;
    QObject::connect(&window, &AdminMainWindow::loginRequested,
                     &controller, &AdminController::login);
    QObject::connect(&window, &AdminMainWindow::logoutRequested,
                     &controller, &AdminController::logout);
    QObject::connect(&controller, &AdminController::statusTextChanged,
                     &window, &AdminMainWindow::setConnectionStatus);
    QObject::connect(&controller, &AdminController::loginBusyChanged,
                     &window, &AdminMainWindow::setLoginBusy);
    QObject::connect(&controller, &AdminController::loginSucceeded,
                     &window, &AdminMainWindow::showAdminHome);
    QObject::connect(&controller, &AdminController::loginFailed,
                     &window, &AdminMainWindow::showLoginError);
    QObject::connect(&controller, &AdminController::loggedOut,
                     &window, &AdminMainWindow::showLoginPage);
    window.show();
    return application.exec();
}

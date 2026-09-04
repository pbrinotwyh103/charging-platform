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
    QObject::connect(&window, &UserMainWindow::connectionRequested,
                     &controller, &UserController::connectToServer);
    QObject::connect(&controller, &UserController::statusTextChanged,
                     &window, &UserMainWindow::setConnectionStatus);
    window.show();
    return application.exec();
}

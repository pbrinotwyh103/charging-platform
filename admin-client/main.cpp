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
    QObject::connect(&window, &AdminMainWindow::connectionRequested,
                     &controller, &AdminController::connectToServer);
    QObject::connect(&controller, &AdminController::statusTextChanged,
                     &window, &AdminMainWindow::setConnectionStatus);
    window.show();
    return application.exec();
}

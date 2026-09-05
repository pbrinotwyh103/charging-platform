#include "app/appinfo.h"
#include "controllers/admincontroller.h"
#include "ui/adminmainwindow.h"

#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>

int main(int argc, char *argv[]) {
  QApplication application(argc, argv);
  application.setApplicationName(QStringLiteral("charging_admin_client"));
  application.setApplicationVersion(Charging::AppInfo::Version);

  QCommandLineParser parser;
  parser.setApplicationDescription(QStringLiteral("充电平台管理员手机客户端"));
  parser.addHelpOption();
  parser.addVersionOption();
  QCommandLineOption demoOption(
      QStringLiteral("demo"),
      QStringLiteral("使用只读样例数据预览全部管理页面"));
  parser.addOption(demoOption);
  parser.process(application);

  AdminController controller;
  AdminMainWindow window;
  QObject::connect(&window, &AdminMainWindow::loginRequested, &controller,
                   &AdminController::login);
  QObject::connect(&window, &AdminMainWindow::logoutRequested, &controller,
                   &AdminController::logout);
  if (!parser.isSet(demoOption)) {
    QObject::connect(&window, &AdminMainWindow::adminCommandRequested,
                     &controller, &AdminController::requestAdminCommand);
  }
  QObject::connect(&controller, &AdminController::statusTextChanged, &window,
                   &AdminMainWindow::setConnectionStatus);
  QObject::connect(&controller, &AdminController::loginBusyChanged, &window,
                   &AdminMainWindow::setLoginBusy);
  QObject::connect(&controller, &AdminController::loginSucceeded, &window,
                   &AdminMainWindow::showAdminHome);
  QObject::connect(&controller, &AdminController::loginFailed, &window,
                   &AdminMainWindow::showLoginError);
  QObject::connect(&controller, &AdminController::loggedOut, &window,
                   &AdminMainWindow::showLoginPage);
  QObject::connect(&controller, &AdminController::commandBusyChanged, &window,
                   &AdminMainWindow::setCommandBusy);
  QObject::connect(&controller, &AdminController::commandSucceeded, &window,
                   &AdminMainWindow::handleCommandSucceeded);
  QObject::connect(&controller, &AdminController::commandFailed, &window,
                   &AdminMainWindow::handleCommandFailed);
  QObject::connect(&controller, &AdminController::pushReceived, &window,
                   &AdminMainWindow::handlePush);
  if (parser.isSet(demoOption))
    window.showDemoWorkspace();
  window.show();
  return application.exec();
}

#pragma once

#include <QJsonObject>
#include <QMainWindow>

class AlarmsPage;
class AssetsPage;
class MonitorPage;
class OverviewPage;
class RecordsPage;
class QLabel;
class QLineEdit;
class QSpinBox;
class QPushButton;
class QStackedWidget;
class QTabBar;
class QTimer;

class AdminMainWindow final : public QMainWindow {
  Q_OBJECT

public:
  explicit AdminMainWindow(QWidget *parent = nullptr);

public slots:
  void setConnectionStatus(const QString &text, bool connected);
  void setLoginBusy(bool busy);
  void showLoginError(const QString &message);
  void showAdminHome(const QJsonObject &admin);
  void showDemoWorkspace();
  void showLoginPage();
  void setCommandBusy(const QString &action, bool busy);
  void handleCommandSucceeded(const QString &action,
                              const QJsonObject &payload);
  void handleCommandFailed(const QString &action, const QString &message,
                           int errorCode);
  void handlePush(quint16 messageType, const QJsonObject &payload);

signals:
  void loginRequested(const QString &username, const QString &password,
                      const QString &host, quint16 port);
  void logoutRequested();
  void adminCommandRequested(const QString &action,
                             const QJsonObject &parameters);

private:
  QWidget *createLoginPage();
  QWidget *createWorkspacePage();
  void refreshCurrentPage();
  void setNotice(const QString &message, bool error = false);

  QLabel *m_statusLabel = nullptr;
  QLabel *m_appTitleLabel = nullptr;
  QLabel *m_workspaceTitleLabel = nullptr;
  QLabel *m_workspaceStatusLabel = nullptr;
  QLineEdit *m_hostEdit = nullptr;
  QSpinBox *m_portSpin = nullptr;
  QLineEdit *m_usernameEdit = nullptr;
  QLineEdit *m_passwordEdit = nullptr;
  QPushButton *m_loginButton = nullptr;
  QStackedWidget *m_pages = nullptr;
  QLabel *m_loginErrorLabel = nullptr;
  QLabel *m_adminNameLabel = nullptr;
  QLabel *m_permissionLabel = nullptr;
  QLabel *m_noticeLabel = nullptr;
  QStackedWidget *m_contentPages = nullptr;
  QTabBar *m_navigation = nullptr;
  QTimer *m_refreshTimer = nullptr;
  OverviewPage *m_overviewPage = nullptr;
  MonitorPage *m_monitorPage = nullptr;
  AlarmsPage *m_alarmsPage = nullptr;
  AssetsPage *m_assetsPage = nullptr;
  RecordsPage *m_recordsPage = nullptr;
};

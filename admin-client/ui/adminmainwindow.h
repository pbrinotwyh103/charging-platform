#pragma once

#include <QMainWindow>
#include <QJsonObject>

class QLabel;
class QLineEdit;
class QSpinBox;
class QPushButton;
class QStackedWidget;

class AdminMainWindow final : public QMainWindow
{
    Q_OBJECT

public:
    explicit AdminMainWindow(QWidget *parent = nullptr);

public slots:
    void setConnectionStatus(const QString &text, bool connected);
    void setLoginBusy(bool busy);
    void showLoginError(const QString &message);
    void showAdminHome(const QJsonObject &admin);
    void showLoginPage();

signals:
    void connectionRequested(const QString &host, quint16 port);
    void loginRequested(const QString &username, const QString &password,
                        const QString &host, quint16 port);
    void logoutRequested();

private:
    QLabel *m_statusLabel = nullptr;
    QLineEdit *m_hostEdit = nullptr;
    QSpinBox *m_portSpin = nullptr;
    QLineEdit *m_usernameEdit = nullptr;
    QLineEdit *m_passwordEdit = nullptr;
    QPushButton *m_loginButton = nullptr;
    QStackedWidget *m_pages = nullptr;
    QLabel *m_loginErrorLabel = nullptr;
    QLabel *m_adminNameLabel = nullptr;
    QLabel *m_permissionLabel = nullptr;
};

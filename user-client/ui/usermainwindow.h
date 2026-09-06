#pragma once

#include <QMainWindow>
#include <QJsonObject>

class QLabel;
class QLineEdit;
class QSpinBox;
class QPushButton;
class QStackedWidget;
class QWidget;
class HomePage;
class ChargingPage;
class ProfilePage;
class StationDetailPage;

class UserMainWindow final : public QMainWindow
{
    Q_OBJECT

public:
    explicit UserMainWindow(QWidget *parent = nullptr);

public slots:
    void setConnectionStatus(const QString &text, bool connected);
    void setLoginBusy(bool busy);
    void showLoginError(const QString &message);
    void showProfile(const QJsonObject &profile);
    void showLoginPage();
    void showFeatureMessage(const QString &message);

signals:
    void connectionRequested(const QString &host, quint16 port);
    void loginRequested(const QString &phone, const QString &host, quint16 port);
    void logoutRequested();

private:
    QLabel *m_statusLabel = nullptr;
    QLineEdit *m_hostEdit = nullptr;
    QSpinBox *m_portSpin = nullptr;
    QLineEdit *m_phoneEdit = nullptr;
    QPushButton *m_loginButton = nullptr;
    QStackedWidget *m_pages = nullptr;
    QLabel *m_loginErrorLabel = nullptr;
    QWidget *m_navWidget = nullptr;
    HomePage *m_homePage = nullptr;
    ChargingPage *m_chargingPage = nullptr;
    ProfilePage *m_profilePage = nullptr;
    StationDetailPage *m_stationDetailPage = nullptr;
};

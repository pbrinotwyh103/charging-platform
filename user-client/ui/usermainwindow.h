#pragma once

#include <QMainWindow>
#include <QJsonArray>
#include <QJsonObject>

class QLabel;
class QLineEdit;
class QSpinBox;
class QPushButton;
class QStackedWidget;
class QWidget;
class QFrame;
class QVBoxLayout;
class QResizeEvent;
class HomePage;
class ChargingPage;
class CustomerServicePage;
class ProfilePage;
class StationDetailPage;
class NavigationPage;

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
    void showFavoriteChanged(const QJsonObject &result);
    void showFavoriteUpdateFailed(qint64 stationId, const QString &message);
    void showDemoWorkspace();
    void showReservationCreated(const QJsonObject &reservation);
    void showChargingSnapshot(const QJsonObject &snapshot);
    void handleActiveOrderCheck(bool active, const QJsonObject &snapshot);
    void showChargingStopped(const QJsonObject &result);
    void showDirectPile(const QJsonObject &station, const QJsonObject &pile);

signals:
    void connectionRequested(const QString &host, quint16 port);
    void loginRequested(const QString &phone, const QString &host, quint16 port);
    void logoutRequested();
    void reservationRequested(qint64 stationId, qint64 pileId);
    void activeOrderCheckRequested();

protected:
    void resizeEvent(QResizeEvent *event) override;

private:
    void updateResponsiveLayout();
    QLineEdit *m_hostEdit = nullptr;
    QSpinBox *m_portSpin = nullptr;
    QLineEdit *m_phoneEdit = nullptr;
    QPushButton *m_loginButton = nullptr;
    QStackedWidget *m_pages = nullptr;
    QLabel *m_loginErrorLabel = nullptr;
    QWidget *m_navWidget = nullptr;
    QWidget *m_contentColumn = nullptr;
    QFrame *m_loginCard = nullptr;
    QVBoxLayout *m_rootLayout = nullptr;
    HomePage *m_homePage = nullptr;
    ChargingPage *m_chargingPage = nullptr;
    CustomerServicePage *m_customerServicePage = nullptr;
    ProfilePage *m_profilePage = nullptr;
    StationDetailPage *m_stationDetailPage = nullptr;
    NavigationPage *m_navigationPage = nullptr;
    bool m_demoMode = false;
    bool m_waitingForChargingEntry = false;
    bool m_responsiveInitialized = false;
    bool m_compactLayout = false;
    QJsonArray m_demoStations;
    QJsonObject m_pendingStation;
    QJsonObject m_pendingPile;
};

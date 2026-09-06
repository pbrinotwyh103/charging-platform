#pragma once

#include <QJsonArray>
#include <QWidget>
#include <QJsonObject>

class QLabel;
class QListWidget;
class QPushButton;
class QPixmap;

class ProfilePage final : public QWidget
{
    Q_OBJECT

public:
    explicit ProfilePage(QWidget *parent = nullptr);
    ~ProfilePage() override;

    void setProfile(const QJsonObject &profile);

public slots:
    void setBalanceCents(qint64 balanceCents);
    void applyProfileUpdate(const QJsonObject &profile);
    void showProfileUpdateError(const QString &message);
    void setRechargeBusy(bool busy);
    void showRechargeResult(const QJsonObject &record);
    void showRechargeError(const QString &message);
    void showOrderLoading();
    void showOrderError(const QString &message);
    void setOrders(const QJsonArray &orders);

signals:
    void logoutRequested();
    void rechargeRecordsRequested();
    void favoritesRequested();
    void orderHistoryRequested();
    void nicknameUpdateRequested(const QString &nickname);
    void avatarUpdateRequested(const QString &path);
    void avatarPrepared(const QString &base64, const QString &mimeType);
    void rechargeRequested(qint64 cents);

private:
    QPushButton *m_avatarLabel = nullptr;
    QLabel *m_nicknameLabel = nullptr;
    QLabel *m_phoneLabel = nullptr;
    QLabel *m_balanceLabel = nullptr;
    QLabel *m_orderSummaryLabel = nullptr;
    QLabel *m_orderStateLabel = nullptr;
    QListWidget *m_ordersList = nullptr;
    QPushButton *m_orderRefreshButton = nullptr;

    QPushButton *m_logoutButton = nullptr;
    QLabel *m_profileStatusLabel = nullptr;
    QString m_currentNickname;
    qint64 m_balanceFen = 0;
    QPixmap *m_avatarBeforeEdit = nullptr;
    bool m_avatarUpdatePending = false;
};

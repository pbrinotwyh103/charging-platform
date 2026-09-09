#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QPixmap>
#include <QWidget>

class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QBoxLayout;
class QVBoxLayout;

class ProfilePage final : public QWidget
{
    Q_OBJECT

public:
    explicit ProfilePage(QWidget *parent = nullptr);
    void setProfile(const QJsonObject &profile);
    void setDemoMode(bool enabled);
    void setFavoriteStation(const QString &name, bool favorited);
    void setFavoriteStations(const QJsonArray &stations);
    void showFavoritesLoading();
    void showFavoritesError(const QString &message);
    void applyWalletResult(const QJsonObject &result);
    void applySettlementResult(const QJsonObject &result);
    void setLedger(const QJsonArray &items);
    void showLedgerLoading();
    void showLedgerError(const QString &message);
    void setOrderHistory(const QJsonArray &items);
    void showOrderHistoryLoading();
    void showOrderHistoryError(const QString &message);
    bool updateAvatarFromFile(const QString &path);
    void setCompactLayout(bool compact);

public slots:
    void avatarUpdateSucceeded(const QJsonObject &profile);
    void avatarUpdateFailed(const QString &message);
    void nicknameUpdateSucceeded(const QJsonObject &profile);
    void nicknameUpdateFailed(const QString &message);
    void rechargeFailed(const QString &message);

signals:
    void logoutRequested();
    void nicknameUpdateRequested(const QString &nickname);
    void avatarUpdateRequested(const QString &dataUrl);
    void rechargeRequested(qint64 cents);
    void ledgerRefreshRequested();
    void favoritesRefreshRequested();
    void orderHistoryRefreshRequested();

private:
    void updateBalance();
    void showAvatar(const QPixmap &pixmap);
    void showDefaultAvatar();

    QLabel *m_avatarLabel = nullptr;
    QLabel *m_nicknameLabel = nullptr;
    QLabel *m_phoneLabel = nullptr;
    QLabel *m_balanceLabel = nullptr;
    QLabel *m_accountNoteLabel = nullptr;
    QPushButton *m_logoutButton = nullptr;
    QPushButton *m_avatarButton = nullptr;
    QPushButton *m_saveNicknameButton = nullptr;
    QPushButton *m_rechargeButton = nullptr;
    QPushButton *m_ledgerRetryButton = nullptr;
    QPushButton *m_favoriteRetryButton = nullptr;
    QLineEdit *m_nicknameEdit = nullptr;
    QDoubleSpinBox *m_rechargeAmount = nullptr;
    QListWidget *m_ledgerList = nullptr;
    QListWidget *m_favoriteList = nullptr;
    QListWidget *m_orderList = nullptr;
    QPushButton *m_orderRetryButton = nullptr;
    QVBoxLayout *m_rootLayout = nullptr;
    QBoxLayout *m_rechargeLayout = nullptr;
    qint64 m_balanceCents = 0;
    bool m_demoMode = false;
    bool m_avatarUpdatePending = false;
    bool m_nicknameUpdatePending = false;
    bool m_rechargePending = false;
    QPixmap m_confirmedAvatar;
    QPixmap m_pendingAvatar;
    QString m_confirmedAvatarPath;
    QString m_confirmedNickname;
    QString m_pendingNickname;
};

#pragma once

#include <QJsonObject>
#include <QWidget>

class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;

class ProfilePage final : public QWidget
{
    Q_OBJECT

public:
    explicit ProfilePage(QWidget *parent = nullptr);
    void setProfile(const QJsonObject &profile);
    void setDemoMode(bool enabled);
    void setFavoriteStation(const QString &name, bool favorited);

signals:
    void logoutRequested();
    void nicknameUpdateRequested(const QString &nickname);
    void avatarUpdateRequested(const QString &path);
    void rechargeRequested(qint64 cents);

private:
    void updateBalance();

    QLabel *m_avatarLabel = nullptr;
    QLabel *m_nicknameLabel = nullptr;
    QLabel *m_phoneLabel = nullptr;
    QLabel *m_balanceLabel = nullptr;
    QLabel *m_accountNoteLabel = nullptr;
    QPushButton *m_logoutButton = nullptr;
    QLineEdit *m_nicknameEdit = nullptr;
    QDoubleSpinBox *m_rechargeAmount = nullptr;
    QListWidget *m_ledgerList = nullptr;
    QListWidget *m_favoriteList = nullptr;
    qint64 m_balanceCents = 0;
    bool m_demoMode = false;
};

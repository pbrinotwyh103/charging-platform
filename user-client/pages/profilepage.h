#pragma once

#include <QWidget>
#include <QJsonObject>

class QLabel;
class QPushButton;
class QLineEdit;

class ProfilePage final : public QWidget
{
    Q_OBJECT

public:
    explicit ProfilePage(QWidget *parent = nullptr);

    void setProfile(const QJsonObject &profile);

signals:
    void logoutRequested();
    void nicknameUpdateRequested(const QString &nickname);
    void avatarUpdateRequested(const QString &path);
    void rechargeRequested(qint64 cents);

private:
    QLabel *m_avatarLabel = nullptr;
    QLabel *m_nicknameLabel = nullptr;
    QLabel *m_phoneLabel = nullptr;
    QLabel *m_balanceLabel = nullptr;
    QLabel *m_accountNoteLabel = nullptr;

    QPushButton *m_logoutButton = nullptr;
    QLineEdit *m_nicknameEdit = nullptr;
};

#pragma once

#include <QDialog>

class QLabel;
class QLineEdit;

class NicknameDialog final : public QDialog
{
    Q_OBJECT

public:
    explicit NicknameDialog(const QString &currentNickname,
                            QWidget *parent = nullptr);

    QString nickname() const { return m_nickname; }

private:
    void confirmNickname();

    QString m_currentNickname;
    QString m_nickname;
    QLineEdit *m_nicknameEdit = nullptr;
    QLabel *m_errorLabel = nullptr;
};

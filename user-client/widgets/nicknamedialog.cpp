#include "widgets/nicknamedialog.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QVBoxLayout>

NicknameDialog::NicknameDialog(const QString &currentNickname, QWidget *parent)
    : QDialog(parent),
      m_currentNickname(currentNickname.trimmed())
{
    setObjectName(QStringLiteral("nicknameDialog"));
    setWindowTitle(QStringLiteral("修改昵称"));
    setModal(true);
    setMinimumWidth(330);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(22, 22, 22, 22);
    layout->setSpacing(14);

    auto *titleLabel = new QLabel(QStringLiteral("修改昵称"), this);
    QFont titleFont = titleLabel->font();
    titleFont.setPointSize(17);
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);

    auto *descriptionLabel = new QLabel(
        QStringLiteral("昵称长度为 2—20 个字符，提交后以服务端结果为准。"), this);
    descriptionLabel->setWordWrap(true);
    descriptionLabel->setStyleSheet(QStringLiteral("color:#64748b;"));

    m_nicknameEdit = new QLineEdit(m_currentNickname, this);
    m_nicknameEdit->setObjectName(QStringLiteral("nicknameEdit"));
    m_nicknameEdit->setMaxLength(20);
    m_nicknameEdit->setClearButtonEnabled(true);
    m_nicknameEdit->setMinimumHeight(42);
    m_nicknameEdit->setStyleSheet(QStringLiteral(
        "QLineEdit{padding:0 12px;border:1px solid #cbd5e1;border-radius:9px;background:white;}"
        "QLineEdit:focus{border:1px solid #2563eb;}"));

    m_errorLabel = new QLabel(this);
    m_errorLabel->setObjectName(QStringLiteral("nicknameErrorLabel"));
    m_errorLabel->setWordWrap(true);
    m_errorLabel->setStyleSheet(QStringLiteral("color:#b91c1c;"));

    auto *actions = new QHBoxLayout;
    actions->setSpacing(10);
    auto *cancelButton = new QPushButton(QStringLiteral("取消"), this);
    auto *confirmButton = new QPushButton(QStringLiteral("保存"), this);
    confirmButton->setObjectName(QStringLiteral("nicknameConfirmButton"));
    cancelButton->setMinimumHeight(42);
    confirmButton->setMinimumHeight(42);
    cancelButton->setStyleSheet(QStringLiteral(
        "QPushButton{background:#f8fafc;color:#475569;border:1px solid #cbd5e1;border-radius:9px;font-weight:600;}"));
    confirmButton->setStyleSheet(QStringLiteral(
        "QPushButton{background:#2563eb;color:white;border:0;border-radius:9px;font-weight:600;}"
        "QPushButton:pressed{background:#1d4ed8;}"));
    actions->addWidget(cancelButton, 1);
    actions->addWidget(confirmButton, 1);

    layout->addWidget(titleLabel);
    layout->addWidget(descriptionLabel);
    layout->addWidget(m_nicknameEdit);
    layout->addWidget(m_errorLabel);
    layout->addLayout(actions);

    connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);
    connect(confirmButton, &QPushButton::clicked,
            this, &NicknameDialog::confirmNickname);
    connect(m_nicknameEdit, &QLineEdit::returnPressed,
            this, &NicknameDialog::confirmNickname);
    connect(m_nicknameEdit, &QLineEdit::textChanged,
            m_errorLabel, &QLabel::clear);

    m_nicknameEdit->setFocus();
    m_nicknameEdit->selectAll();
}

void NicknameDialog::confirmNickname()
{
    const QString nickname = m_nicknameEdit->text().trimmed();
    if (nickname.size() < 2 || nickname.size() > 20) {
        m_errorLabel->setText(QStringLiteral("昵称长度需为 2—20 个字符"));
        return;
    }

    static const QRegularExpression illegalCharacters(
        QStringLiteral("[\\x{0000}-\\x{001f}\\x{007f}/\\\\:*?\"<>|]"));
    if (nickname.contains(illegalCharacters)) {
        m_errorLabel->setText(QStringLiteral("昵称不能包含控制字符或 / \\ : * ? \" < > |"));
        return;
    }
    if (nickname == m_currentNickname) {
        m_errorLabel->setText(QStringLiteral("新昵称与当前昵称相同"));
        return;
    }

    m_nickname = nickname;
    accept();
}

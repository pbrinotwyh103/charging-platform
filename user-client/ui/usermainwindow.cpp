#include "ui/usermainwindow.h"

#include "app/appinfo.h"

#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QSpinBox>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <QWidget>

UserMainWindow::UserMainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("充电用户端"));
    resize(390, 780);
    setMinimumSize(360, 640);

    auto *central = new QWidget(this);
    auto *root = new QVBoxLayout(central);
    root->setContentsMargins(22, 22, 22, 22);
    root->setSpacing(14);

    auto *title = new QLabel(QStringLiteral("电动汽车充电"), central);
    QFont titleFont = title->font();
    titleFont.setPointSize(20);
    titleFont.setBold(true);
    title->setFont(titleFont);
    auto *subtitle = new QLabel(QStringLiteral("用户手机客户端"), central);
    subtitle->setStyleSheet(QStringLiteral("color:#64748b;"));

    m_statusLabel = new QLabel(QStringLiteral("尚未连接服务器"), central);
    m_statusLabel->setWordWrap(true);
    m_statusLabel->setStyleSheet(QStringLiteral(
        "padding:9px;background:#f1f5f9;color:#475569;border-radius:8px;"));

    m_pages = new QStackedWidget(central);
    auto *loginPage = new QWidget(m_pages);
    auto *loginLayout = new QVBoxLayout(loginPage);
    loginLayout->setContentsMargins(0, 12, 0, 0);
    loginLayout->setSpacing(14);

    auto *loginHint = new QLabel(
        QStringLiteral("手机号免密登录\n首次登录将自动注册账号"), loginPage);
    loginHint->setWordWrap(true);
    loginHint->setStyleSheet(QStringLiteral("font-size:16px;font-weight:600;color:#0f172a;"));
    m_phoneEdit = new QLineEdit(loginPage);
    m_phoneEdit->setPlaceholderText(QStringLiteral("请输入11位手机号"));
    m_phoneEdit->setMaxLength(11);
    m_phoneEdit->setInputMethodHints(Qt::ImhDigitsOnly);
    m_phoneEdit->setValidator(new QRegularExpressionValidator(
        QRegularExpression(QStringLiteral("[0-9]{0,11}")), m_phoneEdit));

    auto *serverBox = new QGroupBox(QStringLiteral("服务器设置"), loginPage);
    auto *serverForm = new QFormLayout(serverBox);
    m_hostEdit = new QLineEdit(QStringLiteral("127.0.0.1"), serverBox);
    m_portSpin = new QSpinBox(serverBox);
    m_portSpin->setRange(1, 65535);
    m_portSpin->setValue(Charging::AppInfo::DefaultServerPort);
    serverForm->addRow(QStringLiteral("地址"), m_hostEdit);
    serverForm->addRow(QStringLiteral("端口"), m_portSpin);

    m_loginButton = new QPushButton(QStringLiteral("登录 / 自动注册"), loginPage);
    m_loginButton->setMinimumHeight(44);
    m_loginButton->setStyleSheet(QStringLiteral(
        "QPushButton{background:#2563eb;color:white;border:0;border-radius:9px;font-weight:600;}"
        "QPushButton:disabled{background:#94a3b8;}"));
    m_loginErrorLabel = new QLabel(loginPage);
    m_loginErrorLabel->setWordWrap(true);
    m_loginErrorLabel->setStyleSheet(QStringLiteral("color:#b91c1c;"));

    connect(m_loginButton, &QPushButton::clicked, this, [this] {
        const QString phone = m_phoneEdit->text().trimmed();
        static const QRegularExpression pattern(QStringLiteral("^1\\d{10}$"));
        if (!pattern.match(phone).hasMatch()) {
            showLoginError(QStringLiteral("请输入以1开头的11位手机号"));
            return;
        }
        if (m_hostEdit->text().trimmed().isEmpty()) {
            showLoginError(QStringLiteral("服务器地址不能为空"));
            return;
        }
        showLoginError({});
        emit loginRequested(phone, m_hostEdit->text().trimmed(),
                            static_cast<quint16>(m_portSpin->value()));
    });
    connect(m_phoneEdit, &QLineEdit::returnPressed, m_loginButton, &QPushButton::click);

    loginLayout->addWidget(loginHint);
    loginLayout->addWidget(m_phoneEdit);
    loginLayout->addWidget(serverBox);
    loginLayout->addWidget(m_loginButton);
    loginLayout->addWidget(m_loginErrorLabel);
    loginLayout->addStretch();

    auto *profilePage = new QWidget(m_pages);
    auto *profileLayout = new QVBoxLayout(profilePage);
    profileLayout->setContentsMargins(0, 12, 0, 0);
    profileLayout->setSpacing(13);
    m_avatarLabel = new QLabel(QStringLiteral("用户"), profilePage);
    m_avatarLabel->setFixedSize(72, 72);
    m_avatarLabel->setAlignment(Qt::AlignCenter);
    m_avatarLabel->setStyleSheet(QStringLiteral(
        "background:#cbd5e1;color:#475569;border-radius:36px;font-weight:700;"));
    m_nicknameLabel = new QLabel(profilePage);
    QFont nameFont = m_nicknameLabel->font();
    nameFont.setPointSize(17);
    nameFont.setBold(true);
    m_nicknameLabel->setFont(nameFont);
    m_phoneLabel = new QLabel(profilePage);
    m_balanceLabel = new QLabel(profilePage);
    m_balanceLabel->setStyleSheet(QStringLiteral(
        "padding:16px;background:#eff6ff;color:#1e3a8a;border-radius:10px;font-size:16px;"));
    m_accountNoteLabel = new QLabel(profilePage);
    m_accountNoteLabel->setWordWrap(true);
    m_accountNoteLabel->setStyleSheet(QStringLiteral("color:#166534;"));
    auto *phaseNote = new QLabel(
        QStringLiteral("第一阶段已完成登录、账号建立与资料读取。\n找桩、预约和充电将在后续阶段开放。"),
        profilePage);
    phaseNote->setWordWrap(true);
    phaseNote->setStyleSheet(QStringLiteral(
        "padding:14px;border:1px dashed #94a3b8;border-radius:10px;color:#475569;"));
    auto *logoutButton = new QPushButton(QStringLiteral("退出登录"), profilePage);
    connect(logoutButton, &QPushButton::clicked, this, &UserMainWindow::logoutRequested);
    profileLayout->addWidget(m_avatarLabel, 0, Qt::AlignHCenter);
    profileLayout->addWidget(m_nicknameLabel, 0, Qt::AlignHCenter);
    profileLayout->addWidget(m_phoneLabel, 0, Qt::AlignHCenter);
    profileLayout->addWidget(m_balanceLabel);
    profileLayout->addWidget(m_accountNoteLabel);
    profileLayout->addWidget(phaseNote);
    profileLayout->addStretch();
    profileLayout->addWidget(logoutButton);

    m_pages->addWidget(loginPage);
    m_pages->addWidget(profilePage);
    root->addWidget(title);
    root->addWidget(subtitle);
    root->addWidget(m_statusLabel);
    root->addWidget(m_pages, 1);
    setCentralWidget(central);
}

void UserMainWindow::setConnectionStatus(const QString &text, bool connected)
{
    m_statusLabel->setText(text);
    m_statusLabel->setStyleSheet(connected
        ? QStringLiteral("padding:9px;background:#dcfce7;color:#166534;border-radius:8px;")
        : QStringLiteral("padding:9px;background:#fef2f2;color:#991b1b;border-radius:8px;"));
}

void UserMainWindow::setLoginBusy(bool busy)
{
    m_loginButton->setDisabled(busy);
    m_loginButton->setText(busy ? QStringLiteral("正在登录…")
                                : QStringLiteral("登录 / 自动注册"));
}

void UserMainWindow::showLoginError(const QString &message)
{
    m_loginErrorLabel->setText(message);
}

void UserMainWindow::showProfile(const QJsonObject &profile)
{
    m_nicknameLabel->setText(profile.value(QStringLiteral("nickname")).toString());
    m_phoneLabel->setText(profile.value(QStringLiteral("phone")).toString());
    const qint64 cents = static_cast<qint64>(
        profile.value(QStringLiteral("balanceCents")).toDouble());
    m_balanceLabel->setText(QStringLiteral("钱包余额：¥ %1").arg(cents / 100.0, 0, 'f', 2));
    m_accountNoteLabel->setText(profile.value(QStringLiteral("created")).toBool()
        ? QStringLiteral("首次登录成功，系统已自动创建账号和默认资料。")
        : QStringLiteral("登录成功，用户资料已从服务端同步。"));
    m_pages->setCurrentIndex(1);
}

void UserMainWindow::showLoginPage()
{
    setLoginBusy(false);
    showLoginError({});
    m_pages->setCurrentIndex(0);
}

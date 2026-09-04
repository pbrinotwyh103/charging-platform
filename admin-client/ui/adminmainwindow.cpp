#include "ui/adminmainwindow.h"

#include "app/appinfo.h"

#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <QWidget>

AdminMainWindow::AdminMainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("充电管理端"));
    resize(390, 780);
    setMinimumSize(360, 640);

    auto *central = new QWidget(this);
    auto *root = new QVBoxLayout(central);
    root->setContentsMargins(22, 22, 22, 22);
    root->setSpacing(14);

    auto *title = new QLabel(QStringLiteral("充电运营管理"), central);
    QFont titleFont = title->font();
    titleFont.setPointSize(20);
    titleFont.setBold(true);
    title->setFont(titleFont);
    auto *subtitle = new QLabel(QStringLiteral("管理员手机客户端"), central);
    subtitle->setStyleSheet(QStringLiteral("color:#64748b;"));

    m_statusLabel = new QLabel(QStringLiteral("尚未连接后台服务器"), central);
    m_statusLabel->setWordWrap(true);
    m_statusLabel->setStyleSheet(QStringLiteral(
        "padding:9px;background:#f1f5f9;color:#475569;border-radius:8px;"));

    m_pages = new QStackedWidget(central);
    auto *loginPage = new QWidget(m_pages);
    auto *loginLayout = new QVBoxLayout(loginPage);
    loginLayout->setContentsMargins(0, 12, 0, 0);
    loginLayout->setSpacing(13);
    auto *loginHint = new QLabel(QStringLiteral("管理员身份验证"), loginPage);
    loginHint->setStyleSheet(QStringLiteral("font-size:16px;font-weight:600;color:#0f172a;"));

    auto *credentials = new QFormLayout;
    m_usernameEdit = new QLineEdit(QStringLiteral("admin"), loginPage);
    m_usernameEdit->setMaxLength(32);
    m_passwordEdit = new QLineEdit(loginPage);
    m_passwordEdit->setEchoMode(QLineEdit::Password);
    m_passwordEdit->setMaxLength(128);
    m_passwordEdit->setPlaceholderText(QStringLiteral("请输入管理员密码"));
    credentials->addRow(QStringLiteral("账号"), m_usernameEdit);
    credentials->addRow(QStringLiteral("密码"), m_passwordEdit);

    auto *defaultHint = new QLabel(QStringLiteral("首次运行默认账号：admin / 123456"), loginPage);
    defaultHint->setStyleSheet(QStringLiteral("color:#64748b;"));

    auto *serverBox = new QGroupBox(QStringLiteral("服务器设置"), loginPage);
    auto *serverForm = new QFormLayout(serverBox);
    m_hostEdit = new QLineEdit(QStringLiteral("127.0.0.1"), serverBox);
    m_portSpin = new QSpinBox(serverBox);
    m_portSpin->setRange(1, 65535);
    m_portSpin->setValue(Charging::AppInfo::DefaultServerPort);
    serverForm->addRow(QStringLiteral("地址"), m_hostEdit);
    serverForm->addRow(QStringLiteral("端口"), m_portSpin);

    m_loginButton = new QPushButton(QStringLiteral("管理员登录"), loginPage);
    m_loginButton->setMinimumHeight(44);
    m_loginButton->setStyleSheet(QStringLiteral(
        "QPushButton{background:#1d4ed8;color:white;border:0;border-radius:9px;font-weight:600;}"
        "QPushButton:disabled{background:#94a3b8;}"));
    m_loginErrorLabel = new QLabel(loginPage);
    m_loginErrorLabel->setWordWrap(true);
    m_loginErrorLabel->setStyleSheet(QStringLiteral("color:#b91c1c;"));
    connect(m_loginButton, &QPushButton::clicked, this, [this] {
        const QString username = m_usernameEdit->text().trimmed();
        if (username.isEmpty() || m_passwordEdit->text().isEmpty()) {
            showLoginError(QStringLiteral("管理员账号和密码不能为空"));
            return;
        }
        if (m_hostEdit->text().trimmed().isEmpty()) {
            showLoginError(QStringLiteral("服务器地址不能为空"));
            return;
        }
        showLoginError({});
        emit loginRequested(username, m_passwordEdit->text(),
                            m_hostEdit->text().trimmed(),
                            static_cast<quint16>(m_portSpin->value()));
    });
    connect(m_passwordEdit, &QLineEdit::returnPressed,
            m_loginButton, &QPushButton::click);

    loginLayout->addWidget(loginHint);
    loginLayout->addLayout(credentials);
    loginLayout->addWidget(defaultHint);
    loginLayout->addWidget(serverBox);
    loginLayout->addWidget(m_loginButton);
    loginLayout->addWidget(m_loginErrorLabel);
    loginLayout->addStretch();

    auto *homePage = new QWidget(m_pages);
    auto *homeLayout = new QVBoxLayout(homePage);
    homeLayout->setContentsMargins(0, 12, 0, 0);
    homeLayout->setSpacing(12);
    m_adminNameLabel = new QLabel(homePage);
    QFont adminFont = m_adminNameLabel->font();
    adminFont.setPointSize(17);
    adminFont.setBold(true);
    m_adminNameLabel->setFont(adminFont);
    m_permissionLabel = new QLabel(homePage);
    m_permissionLabel->setStyleSheet(QStringLiteral("color:#166534;"));

    auto *moduleGrid = new QGridLayout;
    const QStringList modules = {
        QStringLiteral("实时监控"), QStringLiteral("营收统计"),
        QStringLiteral("电桩管理"), QStringLiteral("电站管理"),
        QStringLiteral("用户管理"), QStringLiteral("告警中心")
    };
    for (int i = 0; i < modules.size(); ++i) {
        auto *card = new QLabel(modules.at(i), homePage);
        card->setAlignment(Qt::AlignCenter);
        card->setMinimumHeight(76);
        card->setStyleSheet(QStringLiteral(
            "background:#eff6ff;border:1px solid #bfdbfe;border-radius:10px;"
            "color:#1e3a8a;font-weight:600;"));
        moduleGrid->addWidget(card, i / 2, i % 2);
    }
    auto *phaseNote = new QLabel(
        QStringLiteral("第一阶段只开放管理员认证和权限会话，业务模块将在后续阶段逐项实现。"),
        homePage);
    phaseNote->setWordWrap(true);
    phaseNote->setStyleSheet(QStringLiteral("color:#64748b;"));
    auto *logoutButton = new QPushButton(QStringLiteral("退出登录"), homePage);
    connect(logoutButton, &QPushButton::clicked, this, &AdminMainWindow::logoutRequested);

    homeLayout->addWidget(m_adminNameLabel);
    homeLayout->addWidget(m_permissionLabel);
    homeLayout->addLayout(moduleGrid);
    homeLayout->addWidget(phaseNote);
    homeLayout->addStretch();
    homeLayout->addWidget(logoutButton);

    m_pages->addWidget(loginPage);
    m_pages->addWidget(homePage);
    root->addWidget(title);
    root->addWidget(subtitle);
    root->addWidget(m_statusLabel);
    root->addWidget(m_pages, 1);
    setCentralWidget(central);
}

void AdminMainWindow::setConnectionStatus(const QString &text, bool connected)
{
    m_statusLabel->setText(text);
    m_statusLabel->setStyleSheet(connected
        ? QStringLiteral("padding:9px;background:#dcfce7;color:#166534;border-radius:8px;")
        : QStringLiteral("padding:9px;background:#fef2f2;color:#991b1b;border-radius:8px;"));
}

void AdminMainWindow::setLoginBusy(bool busy)
{
    m_loginButton->setDisabled(busy);
    m_loginButton->setText(busy ? QStringLiteral("正在验证…")
                                : QStringLiteral("管理员登录"));
}

void AdminMainWindow::showLoginError(const QString &message)
{
    m_loginErrorLabel->setText(message);
}

void AdminMainWindow::showAdminHome(const QJsonObject &admin)
{
    m_passwordEdit->clear();
    m_adminNameLabel->setText(QStringLiteral("管理员：%1")
        .arg(admin.value(QStringLiteral("username")).toString()));
    m_permissionLabel->setText(QStringLiteral("权限：%1（由服务端会话控制）")
        .arg(admin.value(QStringLiteral("permissions")).toString()));
    m_pages->setCurrentIndex(1);
}

void AdminMainWindow::showLoginPage()
{
    m_passwordEdit->clear();
    setLoginBusy(false);
    m_pages->setCurrentIndex(0);
}

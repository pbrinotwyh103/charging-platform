#include "ui/usermainwindow.h"

#include "app/appinfo.h"
#include "pages/homepage.h"
#include "pages/chargingpage.h"
#include "pages/profilepage.h"
#include "pages/stationdetailpage.h"

#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
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

    m_homePage = new HomePage(m_pages);
    m_chargingPage = new ChargingPage(m_pages);
    m_profilePage = new ProfilePage(m_pages);
    m_stationDetailPage = new StationDetailPage(m_pages);
    connect(m_profilePage, &ProfilePage::logoutRequested, this, &UserMainWindow::logoutRequested);
    m_navWidget = new QWidget(central);
    auto *nav = new QHBoxLayout(m_navWidget);
    nav->setContentsMargins(0, 0, 0, 0);
    auto *homeButton = new QPushButton(QStringLiteral("首页"), m_navWidget);
    auto *chargingButton = new QPushButton(QStringLiteral("充电"), m_navWidget);
    auto *mineButton = new QPushButton(QStringLiteral("我的"), m_navWidget);
    nav->addWidget(homeButton); nav->addWidget(chargingButton); nav->addWidget(mineButton);

    m_pages->addWidget(loginPage);
    m_pages->addWidget(m_homePage);
    m_pages->addWidget(m_chargingPage);
    m_pages->addWidget(m_profilePage);
    m_pages->addWidget(m_stationDetailPage);
    connect(homeButton, &QPushButton::clicked, this, [this] { m_pages->setCurrentWidget(m_homePage); });
    connect(chargingButton, &QPushButton::clicked, this, [this] { m_pages->setCurrentWidget(m_chargingPage); });
    connect(mineButton, &QPushButton::clicked, this, [this] { m_pages->setCurrentWidget(m_profilePage); });
    connect(m_homePage, &HomePage::stationSelected, this, [this](const QJsonObject &s) { m_stationDetailPage->setStation(s); m_pages->setCurrentWidget(m_stationDetailPage); });
    connect(m_stationDetailPage, &StationDetailPage::backRequested, this, [this] { m_pages->setCurrentWidget(m_homePage); });
    root->addWidget(title);
    root->addWidget(subtitle);
    root->addWidget(m_statusLabel);
    root->addWidget(m_pages, 1);
    root->addWidget(m_navWidget);
    m_navWidget->hide();
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
    m_profilePage->setProfile(profile);
    m_navWidget->show();
    m_pages->setCurrentWidget(m_profilePage);
}

void UserMainWindow::showLoginPage()
{
    setLoginBusy(false);
    showLoginError({});
    m_navWidget->hide();
    m_pages->setCurrentIndex(0);
}

void UserMainWindow::showFeatureMessage(const QString &message)
{
    m_statusLabel->setText(message);
}

#include "ui/usermainwindow.h"

#include "app/appinfo.h"
#include "pages/homepage.h"
#include "pages/chargingpage.h"
#include "pages/profilepage.h"
#include "pages/stationdetailpage.h"
#include "map/mapnavigator.h"

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

namespace {
QJsonArray demoStations()
{
    return {
        QJsonObject{{"stationId", 101}, {"name", QStringLiteral("人民广场超级充电站")},
                    {"address", QStringLiteral("黄浦区西藏中路268号")}, {"latitude", 31.2323},
                    {"longitude", 121.4751}, {"priceCentsPerKwh", 168}, {"totalPiles", 18},
                    {"availablePiles", 7}, {"distanceKm", 0.8}, {"favorited", true}},
        QJsonObject{{"stationId", 102}, {"name", QStringLiteral("静安大悦城充电站")},
                    {"address", QStringLiteral("静安区西藏北路198号")}, {"latitude", 31.2487},
                    {"longitude", 121.4692}, {"priceCentsPerKwh", 142}, {"totalPiles", 12},
                    {"availablePiles", 4}, {"distanceKm", 2.4}, {"favorited", false}},
        QJsonObject{{"stationId", 103}, {"name", QStringLiteral("陆家嘴中心充电站")},
                    {"address", QStringLiteral("浦东新区浦东南路899号")}, {"latitude", 31.2397},
                    {"longitude", 121.5056}, {"priceCentsPerKwh", 186}, {"totalPiles", 24},
                    {"availablePiles", 11}, {"distanceKm", 3.6}, {"favorited", false}},
        QJsonObject{{"stationId", 104}, {"name", QStringLiteral("徐家汇绿色能源站")},
                    {"address", QStringLiteral("徐汇区虹桥路1号")}, {"latitude", 31.1952},
                    {"longitude", 121.4368}, {"priceCentsPerKwh", 135}, {"totalPiles", 16},
                    {"availablePiles", 2}, {"distanceKm", 5.7}, {"favorited", false}}
    };
}

QJsonArray demoPiles(qint64 stationId)
{
    const QString prefix = QStringLiteral("SH-%1-").arg(stationId);
    return {
        QJsonObject{{"pileId", stationId * 100 + 1}, {"pileCode", prefix + QStringLiteral("01")},
                    {"type", QStringLiteral("fast")}, {"powerKw", 60.0}, {"status", QStringLiteral("available")}},
        QJsonObject{{"pileId", stationId * 100 + 2}, {"pileCode", prefix + QStringLiteral("02")},
                    {"type", QStringLiteral("fast")}, {"powerKw", 120.0}, {"status", QStringLiteral("charging")}},
        QJsonObject{{"pileId", stationId * 100 + 3}, {"pileCode", prefix + QStringLiteral("03")},
                    {"type", QStringLiteral("slow")}, {"powerKw", 7.0}, {"status", QStringLiteral("available")}},
        QJsonObject{{"pileId", stationId * 100 + 4}, {"pileCode", prefix + QStringLiteral("04")},
                    {"type", QStringLiteral("fast")}, {"powerKw", 60.0}, {"status", QStringLiteral("fault")}}
    };
}
}

UserMainWindow::UserMainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("充电用户端"));
    resize(390, 780);
    setMinimumSize(360, 640);
    setStyleSheet(QStringLiteral(
        "QMainWindow,QWidget{background:#f8fafc;color:#0f172a;}"
        "QLineEdit,QSpinBox,QDoubleSpinBox,QComboBox,QListWidget,QTabWidget::pane{background:white;border:1px solid #dbe3ee;border-radius:8px;padding:6px;}"
        "QPushButton{background:#e2e8f0;border:0;border-radius:8px;padding:9px 12px;font-weight:600;}"
        "QPushButton:hover{background:#cbd5e1;}"
        "QPushButton:disabled{color:#94a3b8;background:#f1f5f9;}"
        "QTabBar::tab{padding:8px 14px;}QTabBar::tab:selected{color:#2563eb;font-weight:700;}"));

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
    m_pages->setObjectName(QStringLiteral("userPages"));
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
    m_navWidget->setObjectName(QStringLiteral("userBottomNavigation"));
    auto *nav = new QHBoxLayout(m_navWidget);
    nav->setContentsMargins(0, 0, 0, 0);
    auto *homeButton = new QPushButton(QStringLiteral("首页"), m_navWidget);
    auto *chargingButton = new QPushButton(QStringLiteral("充电"), m_navWidget);
    auto *mineButton = new QPushButton(QStringLiteral("我的"), m_navWidget);
    homeButton->setObjectName(QStringLiteral("homeNavigationButton"));
    chargingButton->setObjectName(QStringLiteral("chargingNavigationButton"));
    mineButton->setObjectName(QStringLiteral("profileNavigationButton"));
    nav->addWidget(homeButton); nav->addWidget(chargingButton); nav->addWidget(mineButton);

    m_pages->addWidget(loginPage);
    m_pages->addWidget(m_homePage);
    m_pages->addWidget(m_chargingPage);
    m_pages->addWidget(m_profilePage);
    m_pages->addWidget(m_stationDetailPage);
    connect(homeButton, &QPushButton::clicked, this, [this] { m_pages->setCurrentWidget(m_homePage); });
    connect(chargingButton, &QPushButton::clicked, this, [this] {
        m_pages->setCurrentWidget(m_chargingPage);
        if (m_demoMode && m_chargingPage->hasActiveOrder())
            setConnectionStatus(QStringLiteral("检测到未完成订单，已进入充电状态页"), true);
    });
    connect(mineButton, &QPushButton::clicked, this, [this] { m_pages->setCurrentWidget(m_profilePage); });
    connect(m_homePage, &HomePage::stationSelected, this, [this](const QJsonObject &s) { m_stationDetailPage->setStation(s); m_pages->setCurrentWidget(m_stationDetailPage); });
    connect(m_stationDetailPage, &StationDetailPage::backRequested, this, [this] { m_pages->setCurrentWidget(m_homePage); });
    connect(m_homePage, &HomePage::stationsRequested, this,
            [this](const QString &region, const QString &, double, double) {
        if (!m_demoMode) return;
        QJsonArray filtered;
        for (const auto &value : m_demoStations) {
            const QJsonObject station = value.toObject();
            if (region == QStringLiteral("全部区域")
                || station.value(QStringLiteral("address")).toString().contains(region.left(2)))
                filtered.append(station);
        }
        m_homePage->setStations(filtered);
        setConnectionStatus(QStringLiteral("演示数据已刷新 · 站点状态更新于刚刚"), true);
    });
    connect(m_stationDetailPage, &StationDetailPage::pilesRequested, this,
            [this](qint64 stationId) {
        if (m_demoMode) m_stationDetailPage->setPiles(demoPiles(stationId));
    });
    connect(m_stationDetailPage, &StationDetailPage::favoriteRequested, this,
            [this](qint64 stationId, bool favorited) {
        if (!m_demoMode) return;
        for (qsizetype i = 0; i < m_demoStations.size(); ++i) {
            QJsonObject station = m_demoStations.at(i).toObject();
            if (station.value(QStringLiteral("stationId")).toInteger() != stationId) continue;
            station.insert(QStringLiteral("favorited"), favorited);
            m_demoStations.replace(i, station);
            m_profilePage->setFavoriteStation(station.value(QStringLiteral("name")).toString(), favorited);
            break;
        }
        setConnectionStatus(favorited ? QStringLiteral("已加入常用充电站")
                                      : QStringLiteral("已取消收藏"), true);
    });
    connect(m_stationDetailPage, &StationDetailPage::navigationRequested, this,
            [this](const QJsonObject &station, const QString &mode) {
        if (MapNavigator::isConfigured()) {
            setConnectionStatus(QStringLiteral("已在页面内加载腾讯地图%1路线")
                                    .arg(mode == QStringLiteral("walking") ? QStringLiteral("步行")
                                                                          : QStringLiteral("驾车")), true);
        } else {
            setConnectionStatus(QStringLiteral("%1路线已生成：人民广场 → %2（演示预览；配置地图Key后打开腾讯地图）")
                                    .arg(mode == QStringLiteral("walking") ? QStringLiteral("步行")
                                                                          : QStringLiteral("驾车"),
                                         station.value(QStringLiteral("name")).toString()), true);
        }
    });
    connect(m_stationDetailPage, &StationDetailPage::reservationRequested, this,
            [this](const QJsonObject &station, const QJsonObject &pile) {
        if (m_demoMode) {
            m_chargingPage->setReservation(station, pile);
            m_pages->setCurrentWidget(m_chargingPage);
            setConnectionStatus(QStringLiteral("预约成功，已为您锁定电桩15分钟"), true);
            return;
        }
        m_pendingStation = station;
        m_pendingPile = pile;
        emit reservationRequested(
            station.value(QStringLiteral("stationId")).toInteger(),
            pile.value(QStringLiteral("pileId")).toInteger());
        setConnectionStatus(QStringLiteral("正在向服务端提交预约…"), true);
    });
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

void UserMainWindow::showReservationCreated(const QJsonObject &reservation)
{
    m_pendingPile.insert(QStringLiteral("reservationId"),
                         reservation.value(QStringLiteral("reservationId")));
    m_pendingPile.insert(QStringLiteral("expiresAt"),
                         reservation.value(QStringLiteral("expiresAt")));
    m_chargingPage->setReservation(m_pendingStation, m_pendingPile);
    m_pages->setCurrentWidget(m_chargingPage);
    setConnectionStatus(QStringLiteral("预约成功，电桩已锁定"), true);
}

void UserMainWindow::showChargingSnapshot(const QJsonObject &snapshot)
{
    m_chargingPage->setSnapshot(snapshot);
    m_pages->setCurrentWidget(m_chargingPage);
}

void UserMainWindow::showChargingStopped(const QJsonObject &result)
{
    m_chargingPage->setSnapshot(result);
    m_pages->setCurrentWidget(m_chargingPage);
    setConnectionStatus(QStringLiteral("充电已停止，费用结算完成"), true);
}

void UserMainWindow::showDemoWorkspace()
{
    m_demoMode = true;
    m_demoStations = demoStations();
    m_profilePage->setDemoMode(true);
    m_chargingPage->setDemoMode(true);
    m_profilePage->setProfile({
        {QStringLiteral("nickname"), QStringLiteral("绿色出行者")},
        {QStringLiteral("phone"), QStringLiteral("138****2026")},
        {QStringLiteral("balanceCents"), 28650},
        {QStringLiteral("created"), false}});
    m_profilePage->setFavoriteStation(QStringLiteral("人民广场超级充电站"), true);
    m_homePage->setStations(m_demoStations);
    m_navWidget->show();
    m_pages->setCurrentWidget(m_homePage);
    setConnectionStatus(QStringLiteral("演示模式 · 使用本地样例数据，不依赖服务端"), true);
}

#include "ui/usermainwindow.h"

#include "app/appinfo.h"
#include "pages/homepage.h"
#include "pages/chargingpage.h"
#include "pages/profilepage.h"
#include "pages/stationdetailpage.h"
#include "pages/navigationpage.h"
#include "map/mapnavigator.h"

#include <QFormLayout>
#include <QFrame>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QResizeEvent>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QSpinBox>
#include <QStatusBar>
#include <QStackedWidget>
#include <QStyle>
#include <QVBoxLayout>
#include <QWidget>

namespace {
QJsonArray demoStations()
{
    return {
        QJsonObject{{"stationId", 101}, {"name", QStringLiteral("市民中心超级充电站")},
                    {"address", QStringLiteral("深圳市福田区福中三路市民中心")}, {"latitude", 22.5431},
                    {"longitude", 114.0579}, {"priceCentsPerKwh", 168}, {"totalPiles", 18},
                    {"availablePiles", 7}, {"distanceKm", 0.8}, {"favorited", true}},
        QJsonObject{{"stationId", 102}, {"name", QStringLiteral("罗湖万象城充电站")},
                    {"address", QStringLiteral("深圳市罗湖区宝安南路1881号")}, {"latitude", 22.5390},
                    {"longitude", 114.1178}, {"priceCentsPerKwh", 142}, {"totalPiles", 12},
                    {"availablePiles", 4}, {"distanceKm", 2.4}, {"favorited", false}},
        QJsonObject{{"stationId", 103}, {"name", QStringLiteral("南山科技园充电站")},
                    {"address", QStringLiteral("深圳市南山区科苑路15号")}, {"latitude", 22.5405},
                    {"longitude", 113.9538}, {"priceCentsPerKwh", 186}, {"totalPiles", 24},
                    {"availablePiles", 11}, {"distanceKm", 3.6}, {"favorited", false}},
        QJsonObject{{"stationId", 104}, {"name", QStringLiteral("宝安中心绿色能源站")},
                    {"address", QStringLiteral("深圳市宝安区新湖路99号")}, {"latitude", 22.5547},
                    {"longitude", 113.8831}, {"priceCentsPerKwh", 135}, {"totalPiles", 16},
                    {"availablePiles", 2}, {"distanceKm", 5.7}, {"favorited", false}},
        QJsonObject{{"stationId", 105}, {"name", QStringLiteral("龙岗大运中心充电站")},
                    {"address", QStringLiteral("深圳市龙岗区龙翔大道3001号")}, {"latitude", 22.6970},
                    {"longitude", 114.2195}, {"priceCentsPerKwh", 149}, {"totalPiles", 20},
                    {"availablePiles", 8}, {"favorited", false}},
        QJsonObject{{"stationId", 106}, {"name", QStringLiteral("深圳北站充电站")},
                    {"address", QStringLiteral("深圳市龙华区致远中路28号")}, {"latitude", 22.6096},
                    {"longitude", 114.0292}, {"priceCentsPerKwh", 156}, {"totalPiles", 28},
                    {"availablePiles", 13}, {"favorited", false}},
        QJsonObject{{"stationId", 107}, {"name", QStringLiteral("盐田海山充电站")},
                    {"address", QStringLiteral("深圳市盐田区深盐路2088号")}, {"latitude", 22.5574},
                    {"longitude", 114.2376}, {"priceCentsPerKwh", 139}, {"totalPiles", 10},
                    {"availablePiles", 5}, {"favorited", false}},
        QJsonObject{{"stationId", 108}, {"name", QStringLiteral("坪山中心充电站")},
                    {"address", QStringLiteral("深圳市坪山区坪山大道2007号")}, {"latitude", 22.6901},
                    {"longitude", 114.3463}, {"priceCentsPerKwh", 132}, {"totalPiles", 14},
                    {"availablePiles", 6}, {"favorited", false}},
        QJsonObject{{"stationId", 109}, {"name", QStringLiteral("光明科学城充电站")},
                    {"address", QStringLiteral("深圳市光明区光侨路科学城")}, {"latitude", 22.7428},
                    {"longitude", 113.9359}, {"priceCentsPerKwh", 145}, {"totalPiles", 18},
                    {"availablePiles", 9}, {"favorited", false}},
        QJsonObject{{"stationId", 110}, {"name", QStringLiteral("大鹏中心充电站")},
                    {"address", QStringLiteral("深圳市大鹏新区迎宾路")}, {"latitude", 22.5966},
                    {"longitude", 114.4796}, {"priceCentsPerKwh", 128}, {"totalPiles", 8},
                    {"availablePiles", 3}, {"favorited", false}}
    };
}

QJsonArray demoPiles(qint64 stationId)
{
    const QString prefix = QStringLiteral("SZ-%1-").arg(stationId);
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
    resize(430, 780);
    setMinimumSize(360, 640);
    setStyleSheet(QStringLiteral(
        "QMainWindow,QWidget{background:#f5fbff;color:#102033;font-family:\"PingFang SC\",\"Microsoft YaHei\",\"Arial\";font-size:14px;}"
        "QWidget#userAppShell{background:#f5fbff;}"
        "QWidget#userContentColumn{background:#f5fbff;}"
        "QFrame#userLoginCard{background:#ffffff;border:1px solid #dceafe;border-radius:24px;}"
        "QLabel#userLoginHero{font-size:18px;font-weight:900;color:#102033;}"
        "QLabel#userLoginHint{font-size:13px;color:#64748b;}"
        "QLineEdit,QSpinBox,QDoubleSpinBox,QComboBox,QListWidget,QTabWidget::pane{background:white;border:1px solid #dbeafe;border-radius:14px;padding:8px;selection-background-color:#99f6e4;}"
        "QLineEdit:focus,QSpinBox:focus,QDoubleSpinBox:focus,QComboBox:focus{border:2px solid #00CBBF;padding:7px;}"
        "QGroupBox{background:#f8fcff;border:1px solid #dbeafe;border-radius:16px;margin-top:12px;padding-top:14px;color:#526580;font-weight:700;}"
        "QGroupBox::title{subcontrol-origin:margin;left:12px;padding:0 6px;}"
        "QListWidget{outline:0;}"
        "QListWidget::item{background:#ffffff;border:1px solid #e2edff;border-radius:16px;margin:5px 1px;padding:12px;}"
        "QListWidget::item:selected{background:#e6fffb;border-color:#00CBBF;color:#073b4c;}"
        "QPushButton{background:#ecf4ff;border:0;border-radius:14px;padding:10px 14px;font-weight:800;color:#1e3a5f;}"
        "QPushButton:hover{background:#dceafe;}"
        "QPushButton:pressed{background:#cfe0ff;}"
        "QPushButton:disabled{color:#94a3b8;background:#f1f5f9;}"
        "QPushButton#userPrimaryButton{background:#00CBBF;color:white;border:0;border-radius:16px;font-weight:900;}"
        "QPushButton[class=\"userBottomNavButton\"]{background:white;color:#526580;border:1px solid #dbeafe;border-radius:16px;}"
        "QWidget#userBottomNavigation{background:#ffffff;border:1px solid #dbeafe;border-radius:22px;}"
        "QWidget#userContentColumn[compact=\"true\"] QPushButton{padding:8px 10px;border-radius:11px;}"
        "QWidget#userContentColumn[compact=\"true\"] QListWidget{padding:5px;border-radius:11px;}"
        "QWidget#userContentColumn[compact=\"true\"] QListWidget::item{padding:9px;margin:3px 1px;border-radius:12px;}"));

    auto *central = new QWidget(this);
    central->setObjectName(QStringLiteral("userAppShell"));
    auto *outer = new QHBoxLayout(central);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);
    m_contentColumn = new QWidget(central);
    m_contentColumn->setObjectName(QStringLiteral("userContentColumn"));
    m_contentColumn->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    outer->addWidget(m_contentColumn, 0, Qt::AlignHCenter);
    m_rootLayout = new QVBoxLayout(m_contentColumn);
    m_rootLayout->setContentsMargins(12, 12, 12, 12);
    m_rootLayout->setSpacing(12);

    m_pages = new QStackedWidget(m_contentColumn);
    m_pages->setObjectName(QStringLiteral("userPages"));
    auto *loginPage = new QWidget(m_pages);
    auto *loginLayout = new QVBoxLayout(loginPage);
    loginLayout->setContentsMargins(0, 12, 0, 0);
    loginLayout->setSpacing(14);
    m_loginCard = new QFrame(loginPage);
    m_loginCard->setObjectName(QStringLiteral("userLoginCard"));
    m_loginCard->setMinimumHeight(380);
    auto *loginCardLayout = new QVBoxLayout(m_loginCard);
    loginCardLayout->setContentsMargins(22, 22, 22, 22);
    loginCardLayout->setSpacing(14);

    auto *loginHint = new QLabel(
        QStringLiteral("手机号免密登录\n首次登录将自动注册账号"), loginPage);
    loginHint->setObjectName(QStringLiteral("userLoginHero"));
    loginHint->setWordWrap(true);
    auto *loginSubHint = new QLabel(
        QStringLiteral("连接附近充电站，预约空闲电桩，实时查看充电费用"), loginPage);
    loginSubHint->setObjectName(QStringLiteral("userLoginHint"));
    loginSubHint->setWordWrap(true);
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
    m_loginButton->setObjectName(QStringLiteral("userPrimaryButton"));
    m_loginButton->setMinimumHeight(44);
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

    loginCardLayout->addWidget(loginHint);
    loginCardLayout->addWidget(loginSubHint);
    loginCardLayout->addWidget(m_phoneEdit);
    loginCardLayout->addWidget(serverBox);
    loginCardLayout->addWidget(m_loginButton);
    loginCardLayout->addWidget(m_loginErrorLabel);
    loginCardLayout->addStretch();
    loginLayout->addWidget(m_loginCard, 0, Qt::AlignHCenter);
    loginLayout->addStretch();

    m_homePage = new HomePage(m_pages);
    m_chargingPage = new ChargingPage(m_pages);
    m_profilePage = new ProfilePage(m_pages);
    m_stationDetailPage = new StationDetailPage(m_pages);
    m_navigationPage = new NavigationPage(m_pages);
    connect(m_profilePage, &ProfilePage::logoutRequested, this, &UserMainWindow::logoutRequested);
    m_navWidget = new QWidget(m_contentColumn);
    m_navWidget->setObjectName(QStringLiteral("userBottomNavigation"));
    auto *nav = new QHBoxLayout(m_navWidget);
    nav->setContentsMargins(0, 0, 0, 0);
    auto *homeButton = new QPushButton(QStringLiteral("首页"), m_navWidget);
    auto *chargingButton = new QPushButton(QStringLiteral("充电"), m_navWidget);
    auto *mineButton = new QPushButton(QStringLiteral("我的"), m_navWidget);
    homeButton->setObjectName(QStringLiteral("homeNavigationButton"));
    chargingButton->setObjectName(QStringLiteral("chargingNavigationButton"));
    mineButton->setObjectName(QStringLiteral("profileNavigationButton"));
    homeButton->setProperty("class", QStringLiteral("userBottomNavButton"));
    chargingButton->setProperty("class", QStringLiteral("userBottomNavButton"));
    mineButton->setProperty("class", QStringLiteral("userBottomNavButton"));
    nav->addWidget(homeButton); nav->addWidget(chargingButton); nav->addWidget(mineButton);

    m_pages->addWidget(loginPage);
    m_pages->addWidget(m_homePage);
    m_pages->addWidget(m_chargingPage);
    m_pages->addWidget(m_profilePage);
    m_pages->addWidget(m_stationDetailPage);
    m_pages->addWidget(m_navigationPage);
    connect(homeButton, &QPushButton::clicked, this, [this] { m_pages->setCurrentWidget(m_homePage); });
    connect(chargingButton, &QPushButton::clicked, this, [this] {
        m_pages->setCurrentWidget(m_chargingPage);
        if (m_demoMode && m_chargingPage->hasActiveOrder())
            setConnectionStatus(QStringLiteral("检测到未完成订单，已进入充电状态页"), true);
    });
    connect(mineButton, &QPushButton::clicked, this, [this] { m_pages->setCurrentWidget(m_profilePage); });
    connect(m_homePage, &HomePage::stationSelected, this, [this](const QJsonObject &s) { m_stationDetailPage->setStation(s); m_pages->setCurrentWidget(m_stationDetailPage); });
    connect(m_stationDetailPage, &StationDetailPage::backRequested, this, [this] { m_pages->setCurrentWidget(m_homePage); });
    connect(m_navigationPage, &NavigationPage::backRequested, this, [this] {
        m_pages->setCurrentWidget(m_stationDetailPage);
        m_navWidget->show();
    });
    connect(m_homePage, &HomePage::stationsRequested, this,
            [this](const QString &region, const QString &address, double, double) {
        if (!m_demoMode) return;
        QJsonArray filtered;
        for (const auto &value : m_demoStations) {
            const QJsonObject station = value.toObject();
            const bool regionMatches = region == QStringLiteral("全部区域")
                || station.value(QStringLiteral("address")).toString().contains(region.left(2));
            const bool addressMatches = address.trimmed().isEmpty()
                || station.value(QStringLiteral("name")).toString().contains(address.trimmed(), Qt::CaseInsensitive)
                || station.value(QStringLiteral("address")).toString().contains(address.trimmed(), Qt::CaseInsensitive);
            if (regionMatches && addressMatches)
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
        m_stationDetailPage->favoriteUpdateSucceeded(favorited);
        m_homePage->updateFavoriteState(stationId, favorited);
        setConnectionStatus(favorited ? QStringLiteral("已加入常用充电站")
                                      : QStringLiteral("已取消收藏"), true);
    });
    connect(m_stationDetailPage, &StationDetailPage::navigationRequested, this,
            [this](const QJsonObject &station, const QString &mode) {
        double latitude = 0.0;
        double longitude = 0.0;
        const bool hasOrigin = m_homePage->searchLocation(&latitude, &longitude);
        m_navigationPage->showRoute(station, mode, latitude, longitude, hasOrigin);
        m_pages->setCurrentWidget(m_navigationPage);
        m_navWidget->hide();
        if (MapNavigator::isConfigured()) {
            setConnectionStatus(QStringLiteral("已打开腾讯地图%1导航页")
                                    .arg(mode == QStringLiteral("walking") ? QStringLiteral("步行")
                                                                          : QStringLiteral("驾车")), true);
        } else {
            setConnectionStatus(QStringLiteral("已打开%1导航页：当前位置 → %2（演示预览）")
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
    m_rootLayout->addWidget(m_pages, 1);
    m_rootLayout->addWidget(m_navWidget);
    m_navWidget->hide();
    setCentralWidget(central);
    updateResponsiveLayout();
}

void UserMainWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    updateResponsiveLayout();
}

void UserMainWindow::updateResponsiveLayout()
{
    if (!m_contentColumn || !m_rootLayout) return;
    const int availableWidth = qMax(320, width());
    const int contentWidth = qMin(availableWidth, 1080);
    m_contentColumn->setFixedWidth(contentWidth);

    const bool compact = contentWidth < 560;
    const bool shortWindow = height() < 720;
    const int horizontalMargin = compact ? (contentWidth <= 400 ? 8 : 12) : 24;
    const int verticalMargin = shortWindow ? 6 : (compact ? 12 : 20);
    m_rootLayout->setContentsMargins(horizontalMargin, verticalMargin,
                                     horizontalMargin, verticalMargin);
    m_rootLayout->setSpacing(shortWindow ? 8 : 14);
    m_loginCard->setFixedWidth(qMax(280, qMin(520, contentWidth - horizontalMargin * 2)));
    m_loginCard->setMinimumHeight(shortWindow ? 330 : 380);

    m_homePage->setCompactLayout(compact);
    m_chargingPage->setCompactLayout(compact || shortWindow);
    m_profilePage->setCompactLayout(compact);
    m_stationDetailPage->setCompactLayout(compact);
    m_navigationPage->setCompactLayout(compact || shortWindow);

    if (!m_responsiveInitialized || compact != m_compactLayout) {
        m_compactLayout = compact;
        m_responsiveInitialized = true;
        m_contentColumn->setProperty("compact", compact);
        style()->unpolish(m_contentColumn);
        style()->polish(m_contentColumn);
    }
}

void UserMainWindow::setConnectionStatus(const QString &text, bool connected)
{
    const QString statusStyle = connected
        ? QStringLiteral("QStatusBar{color:#166534;background:#dcfce7;}"
                         "QStatusBar::item{border:0;}")
        : QStringLiteral("QStatusBar{color:#991b1b;background:#fef2f2;}"
                         "QStatusBar::item{border:0;}");
    statusBar()->setStyleSheet(statusStyle);
    statusBar()->showMessage(text);
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
    // Station objects contain account-scoped `favorited` flags. Never carry
    // those objects across a login, otherwise a newly logged-in account can
    // see the previous account's favorite markers before its first search.
    m_homePage->clearStations();
    m_stationDetailPage->clearStation();
    m_profilePage->setFavoriteStations({});
    m_profilePage->setProfile(profile);
    m_navWidget->show();
    // 登录成功后的默认入口是首页；用户资料仍保存在“我的”页面中，
    // 用户点击底部导航后即可查看。
    m_pages->setCurrentWidget(m_homePage);
}

void UserMainWindow::showLoginPage()
{
    m_homePage->clearStations();
    m_stationDetailPage->clearStation();
    m_profilePage->setFavoriteStations({});
    setLoginBusy(false);
    showLoginError({});
    m_navWidget->hide();
    m_pages->setCurrentIndex(0);
}

void UserMainWindow::showFeatureMessage(const QString &message)
{
    statusBar()->showMessage(message);
}

void UserMainWindow::showFavoriteChanged(const QJsonObject &result)
{
    const QString name = result.value(QStringLiteral("name")).toString();
    if (name.isEmpty()) return;
    const bool favorited = result.value(QStringLiteral("favorited")).toBool();
    if (result.value(QStringLiteral("stationId")).toInteger() == m_stationDetailPage->stationId())
        m_stationDetailPage->favoriteUpdateSucceeded(favorited);
    m_homePage->updateFavoriteState(result.value(QStringLiteral("stationId")).toInteger(), favorited);
    m_profilePage->setFavoriteStation(name, favorited);
    setConnectionStatus(favorited ? QStringLiteral("已加入常用充电站")
                                  : QStringLiteral("已取消收藏"), true);
}

void UserMainWindow::showFavoriteUpdateFailed(qint64 stationId, const QString &message)
{
    if (stationId == m_stationDetailPage->stationId())
        m_stationDetailPage->favoriteUpdateFailed(message);
    showFeatureMessage(QStringLiteral("收藏操作失败：%1").arg(message));
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
    const bool wasActive = m_chargingPage->hasActiveOrder();
    m_chargingPage->setSnapshot(snapshot);
    // 首次恢复或启动订单时进入充电页；后续实时推送只刷新数据，
    // 避免用户查看首页、我的页面或导航时被持续抢回充电页。
    if (!wasActive && m_chargingPage->hasActiveOrder())
        m_pages->setCurrentWidget(m_chargingPage);
}

void UserMainWindow::showChargingStopped(const QJsonObject &result)
{
    Q_UNUSED(result);
    m_chargingPage->resetToDefault();
    setConnectionStatus(QStringLiteral("充电已停止，费用结算完成"), true);
}

void UserMainWindow::showDemoWorkspace()
{
    m_demoMode = true;
    m_demoStations = demoStations();
    m_profilePage->setDemoMode(true);
    m_chargingPage->setDemoMode(true);
    m_stationDetailPage->setDemoMode(true);
    m_profilePage->setProfile({
        {QStringLiteral("nickname"), QStringLiteral("绿色出行者")},
        {QStringLiteral("phone"), QStringLiteral("138****2026")},
        {QStringLiteral("balanceCents"), 28650},
        {QStringLiteral("created"), false}});
    m_profilePage->setFavoriteStation(QStringLiteral("市民中心超级充电站"), true);
    m_homePage->setStations(m_demoStations);
    m_navWidget->show();
    m_pages->setCurrentWidget(m_homePage);
    setConnectionStatus(QStringLiteral("演示模式 · 使用本地样例数据，不依赖服务端"), true);
}

#include "ui/adminmainwindow.h"

#include "app/appinfo.h"
#include "pages/alarmspage.h"
#include "pages/assetspage.h"
#include "pages/monitorpage.h"
#include "pages/overviewpage.h"
#include "pages/recordspage.h"
#include "protocol/errorcodes.h"
#include "protocol/messagetypes.h"

#include <QDate>
#include <QFormLayout>
#include <QFrame>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSize>
#include <QSpinBox>
#include <QStackedWidget>
#include <QStyle>
#include <QStringList>
#include <QTabBar>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWidget>

namespace {

QJsonArray demoRevenuePoints(int days) {
  QJsonArray points;
  const QDate today = QDate::currentDate();
  for (int index = days - 1; index >= 0; --index) {
    const int elapsedDays = days - 1 - index;
    points.append(QJsonObject{
        {QStringLiteral("date"), today.addDays(-index).toString(Qt::ISODate)},
        {QStringLiteral("revenueCents"),
         16800 + (elapsedDays % 7) * 2350 + (elapsedDays / 7) * 900}});
  }
  return points;
}

} // namespace

AdminMainWindow::AdminMainWindow(QWidget *parent) : QMainWindow(parent) {
  setWindowTitle(QStringLiteral("充电运营管理"));
  resize(420, 820);
  setMinimumSize(360, 640);
  setStyleSheet(QStringLiteral(R"(
        QMainWindow, QWidget { background:#f4f7f6; color:#18211f; font-size:14px; }
        QLabel { background:transparent; }
        QLabel#appTitle { font-size:22px; font-weight:700; color:#102a27; padding:4px 4px 0 4px; }
        QLabel#pageTitle { font-size:20px; font-weight:700; color:#102a27; }
        QLabel#workspaceTitle { font-size:19px; font-weight:700; color:#102a27; }
        QLabel#sectionTitle { font-size:16px; font-weight:700; color:#253432; }
        QLabel#adminNameLabel { color:#4b5f5b; font-size:12px; font-weight:600; }
        QLabel#permissionLabel { padding:2px 6px; border-radius:4px; background:#e7f5f2; color:#0f766e; font-size:11px; }
        QFrame#workspaceHeader { background:white; border:0; border-bottom:1px solid #dce5e3; }
        QLabel#connectionStatus { margin:0 4px; padding:7px 10px; border:1px solid #dce5e3; border-radius:6px; background:white; color:#4b5f5b; }
        QLabel#connectionStatus[connected="true"] { border-color:#a7f3d0; background:#ecfdf5; color:#047857; }
        QLabel#connectionStatus[connected="false"] { border-color:#fed7aa; background:#fff7ed; color:#b45309; }
        QLabel#compactConnectionStatus { padding:5px 7px; border-radius:5px; font-size:12px; font-weight:600; }
        QLabel#compactConnectionStatus[connected="true"] { background:#ecfdf5; color:#047857; }
        QLabel#compactConnectionStatus[connected="false"] { background:#fff7ed; color:#b45309; }
        QLabel#noticeLabel { padding:8px 10px; border-left:3px solid #0891b2; border-radius:5px; background:#ecfeff; color:#155e75; }
        QLabel#noticeLabel[error="true"] { border-left-color:#dc2626; background:#fef2f2; color:#991b1b; }
        QLabel#overviewStateLabel, QLabel#monitorStateLabel, QLabel#alarmStateLabel,
        QLabel#stationStateLabel, QLabel#pileStateLabel, QLabel#userStateLabel,
        QLabel#orderStateLabel { color:#5e706c; font-size:12px; padding:3px 1px; }
        QFrame#metricCard { background:white; border:1px solid #dce5e3; border-radius:8px; }
        QLabel#metricCaption { color:#6b7c78; font-size:12px; }
        QLabel[class="metricValue"] { font-size:18px; font-weight:700; }
        QLabel[metricKind="revenue"] { color:#0f766e; }
        QLabel[metricKind="orders"] { color:#1d4ed8; }
        QLineEdit, QSpinBox, QDoubleSpinBox, QDateEdit, QComboBox {
            min-height:42px; padding:1px 10px; background:white; border:1px solid #c9d5d2; border-radius:6px; selection-background-color:#99f6e4;
        }
        QLineEdit:focus, QSpinBox:focus, QDoubleSpinBox:focus, QDateEdit:focus, QComboBox:focus { border:2px solid #14b8a6; padding:0 9px; }
        QComboBox::drop-down { width:30px; border:0; }
        QPushButton { min-height:42px; padding:0 12px; border:1px solid #c9d5d2; border-radius:6px; background:white; color:#253432; font-weight:500; }
        QPushButton:hover { background:#edf3f1; border-color:#aebfbb; }
        QPushButton:pressed { background:#dfe9e6; }
        QPushButton:disabled { color:#9aa8a5; background:#f1f4f3; border-color:#dce5e3; }
        QPushButton#primaryButton { background:#0f766e; border-color:#0f766e; color:white; font-weight:700; }
        QPushButton#primaryButton:pressed { background:#115e59; }
        QPushButton#secondaryButton { color:#1d4ed8; border-color:#bfdbfe; background:#f8fbff; }
        QPushButton#dangerButton { color:#b91c1c; border-color:#fecaca; background:#fffafa; }
        QPushButton#dangerButton:pressed { background:#fee2e2; }
        QPushButton#sevenDaysButton, QPushButton#thirtyDaysButton { min-height:34px; max-height:34px; padding:0 10px; background:#edf2f1; border-color:#d5dfdd; color:#526560; }
        QPushButton#sevenDaysButton { border-top-right-radius:0; border-bottom-right-radius:0; }
        QPushButton#thirtyDaysButton { border-top-left-radius:0; border-bottom-left-radius:0; }
        QPushButton#sevenDaysButton:checked, QPushButton#thirtyDaysButton:checked { background:#164e46; border-color:#164e46; color:white; }
        QPushButton#previousPageButton, QPushButton#nextPageButton { min-width:44px; max-width:44px; min-height:44px; max-height:44px; padding:0; }
        QLabel#pageNumberLabel { color:#526560; font-size:12px; font-weight:600; }
        QToolButton { min-width:42px; max-width:42px; min-height:42px; max-height:42px; border:1px solid #d5dfdd; border-radius:6px; background:white; }
        QToolButton:hover { background:#edf3f1; }
        QToolButton:pressed { background:#dfe9e6; }
        QTableWidget { background:white; alternate-background-color:#f7faf9; border:1px solid #dce5e3; border-radius:6px; gridline-color:transparent; selection-background-color:#d9f3ee; selection-color:#103f39; outline:0; }
        QTableWidget::item { padding:7px; border-bottom:1px solid #edf1f0; }
        QHeaderView::section { min-height:38px; background:#eaf0ee; color:#314542; padding:6px; border:0; border-bottom:1px solid #d5dfdd; font-weight:700; }
        QGroupBox { background:white; border:1px solid #dce5e3; border-radius:6px; margin-top:9px; padding-top:9px; }
        QGroupBox::title { subcontrol-origin:margin; left:9px; padding:0 4px; color:#526560; font-weight:600; }
        QTabWidget::pane { border:0; background:transparent; }
        QTabBar::tab { min-height:42px; padding:0 13px; border:0; border-bottom:2px solid transparent; color:#61736f; background:transparent; }
        QTabBar::tab:selected { border-bottom-color:#0f766e; color:#0f766e; background:white; font-weight:700; }
        QTabBar#bottomNavigation { background:white; }
        QTabBar#bottomNavigation::tab { min-height:54px; min-width:52px; padding:0 4px; border:0; border-top:3px solid transparent; color:#526560; background:white; }
        QTabBar#bottomNavigation::tab:selected { border-top-color:#0f766e; color:#0f766e; font-weight:700; }
        QScrollArea { background:transparent; }
    )"));

  auto *central = new QWidget(this);
  auto *root = new QVBoxLayout(central);
  root->setContentsMargins(8, 8, 8, 6);
  root->setSpacing(6);
  m_appTitleLabel = new QLabel(QStringLiteral("充电运营管理"), central);
  m_appTitleLabel->setObjectName(QStringLiteral("appTitle"));
  root->addWidget(m_appTitleLabel);
  m_statusLabel = new QLabel(QStringLiteral("尚未连接后台服务器"), central);
  m_statusLabel->setObjectName(QStringLiteral("connectionStatus"));
  m_statusLabel->setProperty("connected", false);
  m_statusLabel->setWordWrap(true);
  root->addWidget(m_statusLabel);

  m_pages = new QStackedWidget(central);
  m_pages->setObjectName(QStringLiteral("authenticationPages"));
  m_pages->addWidget(createLoginPage());
  m_pages->addWidget(createWorkspacePage());
  root->addWidget(m_pages, 1);
  setCentralWidget(central);

  m_refreshTimer = new QTimer(this);
  m_refreshTimer->setInterval(30'000);
  connect(m_refreshTimer, &QTimer::timeout, this,
          &AdminMainWindow::refreshCurrentPage);
}

QWidget *AdminMainWindow::createLoginPage() {
  auto *page = new QWidget(this);
  auto *layout = new QVBoxLayout(page);
  layout->setContentsMargins(8, 18, 8, 8);
  layout->setSpacing(12);
  auto *heading = new QLabel(QStringLiteral("管理员登录"), page);
  heading->setObjectName(QStringLiteral("pageTitle"));
  layout->addWidget(heading);

  auto *credentials = new QFormLayout;
  m_usernameEdit = new QLineEdit(QStringLiteral("admin"), page);
  m_usernameEdit->setMaxLength(32);
  m_passwordEdit = new QLineEdit(page);
  m_passwordEdit->setEchoMode(QLineEdit::Password);
  m_passwordEdit->setMaxLength(128);
  m_passwordEdit->setPlaceholderText(QStringLiteral("管理员密码"));
  credentials->addRow(QStringLiteral("账号"), m_usernameEdit);
  credentials->addRow(QStringLiteral("密码"), m_passwordEdit);
  layout->addLayout(credentials);

  auto *serverBox = new QGroupBox(QStringLiteral("服务器"), page);
  auto *serverForm = new QFormLayout(serverBox);
  m_hostEdit = new QLineEdit(QStringLiteral("127.0.0.1"), serverBox);
  m_portSpin = new QSpinBox(serverBox);
  m_portSpin->setRange(1, 65535);
  m_portSpin->setValue(Charging::AppInfo::DefaultServerPort);
  serverForm->addRow(QStringLiteral("地址"), m_hostEdit);
  serverForm->addRow(QStringLiteral("端口"), m_portSpin);
  layout->addWidget(serverBox);

  m_loginButton = new QPushButton(QStringLiteral("登录"), page);
  m_loginButton->setObjectName(QStringLiteral("primaryButton"));
  m_loginButton->setMinimumHeight(42);
  m_loginErrorLabel = new QLabel(page);
  m_loginErrorLabel->setObjectName(QStringLiteral("loginErrorLabel"));
  m_loginErrorLabel->setStyleSheet(QStringLiteral("color:#b91c1c;"));
  m_loginErrorLabel->setWordWrap(true);
  layout->addWidget(m_loginButton);
  layout->addWidget(m_loginErrorLabel);
  layout->addStretch();

  connect(m_loginButton, &QPushButton::clicked, this, [this] {
    const QString username = m_usernameEdit->text().trimmed();
    const QString host = m_hostEdit->text().trimmed();
    if (username.isEmpty() || m_passwordEdit->text().isEmpty()) {
      showLoginError(QStringLiteral("管理员账号和密码不能为空"));
      return;
    }
    if (host.isEmpty()) {
      showLoginError(QStringLiteral("服务器地址不能为空"));
      return;
    }
    showLoginError({});
    emit loginRequested(username, m_passwordEdit->text(), host,
                        static_cast<quint16>(m_portSpin->value()));
  });
  connect(m_passwordEdit, &QLineEdit::returnPressed, m_loginButton,
          &QPushButton::click);
  return page;
}

QWidget *AdminMainWindow::createWorkspacePage() {
  auto *page = new QWidget(this);
  auto *root = new QVBoxLayout(page);
  root->setContentsMargins(0, 0, 0, 0);
  root->setSpacing(6);
  auto *header = new QFrame(page);
  header->setObjectName(QStringLiteral("workspaceHeader"));
  auto *toolbar = new QHBoxLayout(header);
  toolbar->setContentsMargins(4, 2, 4, 5);
  toolbar->setSpacing(6);
  auto *identity = new QVBoxLayout;
  identity->setSpacing(1);
  m_workspaceTitleLabel = new QLabel(QStringLiteral("运营概览"), header);
  m_workspaceTitleLabel->setObjectName(QStringLiteral("workspaceTitle"));
  identity->addWidget(m_workspaceTitleLabel);
  auto *account = new QHBoxLayout;
  account->setContentsMargins(0, 0, 0, 0);
  account->setSpacing(6);
  m_adminNameLabel = new QLabel(header);
  m_adminNameLabel->setObjectName(QStringLiteral("adminNameLabel"));
  m_permissionLabel = new QLabel(header);
  m_permissionLabel->setObjectName(QStringLiteral("permissionLabel"));
  account->addWidget(m_adminNameLabel);
  account->addWidget(m_permissionLabel);
  account->addStretch();
  identity->addLayout(account);
  m_workspaceStatusLabel = new QLabel(QStringLiteral("● 离线"), header);
  m_workspaceStatusLabel->setObjectName(
      QStringLiteral("compactConnectionStatus"));
  m_workspaceStatusLabel->setProperty("connected", false);
  auto *refreshButton = new QToolButton(header);
  refreshButton->setObjectName(QStringLiteral("refreshButton"));
  refreshButton->setIcon(style()->standardIcon(QStyle::SP_BrowserReload));
  refreshButton->setToolTip(QStringLiteral("刷新当前页面"));
  auto *logoutButton = new QToolButton(header);
  logoutButton->setObjectName(QStringLiteral("logoutButton"));
  logoutButton->setIcon(style()->standardIcon(QStyle::SP_DialogCloseButton));
  logoutButton->setToolTip(QStringLiteral("退出登录"));
  toolbar->addLayout(identity);
  toolbar->addStretch();
  toolbar->addWidget(m_workspaceStatusLabel);
  toolbar->addWidget(refreshButton);
  toolbar->addWidget(logoutButton);
  root->addWidget(header);
  connect(refreshButton, &QToolButton::clicked, this,
          &AdminMainWindow::refreshCurrentPage);
  connect(logoutButton, &QToolButton::clicked, this,
          &AdminMainWindow::logoutRequested);

  m_noticeLabel = new QLabel(page);
  m_noticeLabel->setObjectName(QStringLiteral("noticeLabel"));
  m_noticeLabel->setWordWrap(true);
  m_noticeLabel->hide();
  root->addWidget(m_noticeLabel);

  m_contentPages = new QStackedWidget(page);
  m_contentPages->setObjectName(QStringLiteral("contentPages"));
  m_overviewPage = new OverviewPage(m_contentPages);
  m_monitorPage = new MonitorPage(m_contentPages);
  m_alarmsPage = new AlarmsPage(m_contentPages);
  m_assetsPage = new AssetsPage(m_contentPages);
  m_recordsPage = new RecordsPage(m_contentPages);
  m_contentPages->addWidget(m_overviewPage);
  m_contentPages->addWidget(m_monitorPage);
  m_contentPages->addWidget(m_alarmsPage);
  m_contentPages->addWidget(m_assetsPage);
  m_contentPages->addWidget(m_recordsPage);
  for (QLabel *title : m_contentPages->findChildren<QLabel *>(
           QStringLiteral("pageTitle"), Qt::FindChildrenRecursively)) {
    title->hide();
  }
  root->addWidget(m_contentPages, 1);

  m_navigation = new QTabBar(page);
  m_navigation->setObjectName(QStringLiteral("bottomNavigation"));
  m_navigation->setExpanding(true);
  m_navigation->setUsesScrollButtons(false);
  m_navigation->setMinimumHeight(54);
  m_navigation->setIconSize(QSize(18, 18));
  m_navigation->addTab(style()->standardIcon(QStyle::SP_DesktopIcon),
                       QStringLiteral("概览"));
  m_navigation->addTab(style()->standardIcon(QStyle::SP_MediaPlay),
                       QStringLiteral("充电"));
  m_navigation->addTab(style()->standardIcon(QStyle::SP_MessageBoxWarning),
                       QStringLiteral("告警"));
  m_navigation->addTab(style()->standardIcon(QStyle::SP_DriveHDIcon),
                       QStringLiteral("资产"));
  m_navigation->addTab(
      style()->standardIcon(QStyle::SP_FileDialogDetailedView),
      QStringLiteral("管理"));
  root->addWidget(m_navigation);
  connect(m_navigation, &QTabBar::currentChanged, this, [this](int index) {
    m_contentPages->setCurrentIndex(index);
    const QStringList titles = {
        QStringLiteral("运营概览"), QStringLiteral("实时充电"),
        QStringLiteral("异常告警"), QStringLiteral("站点资产"),
        QStringLiteral("用户订单")};
    if (index >= 0 && index < titles.size())
      m_workspaceTitleLabel->setText(titles.at(index));
    refreshCurrentPage();
  });

  connect(m_overviewPage, &OverviewPage::commandRequested, this,
          &AdminMainWindow::adminCommandRequested);
  connect(m_monitorPage, &MonitorPage::commandRequested, this,
          &AdminMainWindow::adminCommandRequested);
  connect(m_alarmsPage, &AlarmsPage::commandRequested, this,
          &AdminMainWindow::adminCommandRequested);
  connect(m_assetsPage, &AssetsPage::commandRequested, this,
          &AdminMainWindow::adminCommandRequested);
  connect(m_recordsPage, &RecordsPage::commandRequested, this,
          &AdminMainWindow::adminCommandRequested);
  return page;
}

void AdminMainWindow::setConnectionStatus(const QString &text, bool connected) {
  m_statusLabel->setText(text);
  m_statusLabel->setProperty("connected", connected);
  m_statusLabel->style()->unpolish(m_statusLabel);
  m_statusLabel->style()->polish(m_statusLabel);
  if (m_workspaceStatusLabel) {
    m_workspaceStatusLabel->setText(
        text.contains(QStringLiteral("演示"))
            ? QStringLiteral("● 演示")
            : connected ? QStringLiteral("● 在线") : QStringLiteral("● 离线"));
    m_workspaceStatusLabel->setProperty("connected", connected);
    m_workspaceStatusLabel->style()->unpolish(m_workspaceStatusLabel);
    m_workspaceStatusLabel->style()->polish(m_workspaceStatusLabel);
  }
}

void AdminMainWindow::setLoginBusy(bool busy) {
  m_loginButton->setDisabled(busy);
  m_loginButton->setText(busy ? QStringLiteral("正在验证…")
                              : QStringLiteral("登录"));
}

void AdminMainWindow::showLoginError(const QString &message) {
  m_loginErrorLabel->setText(message);
}

void AdminMainWindow::showAdminHome(const QJsonObject &admin) {
  m_appTitleLabel->hide();
  m_statusLabel->hide();
  m_passwordEdit->clear();
  m_adminNameLabel->setText(
      admin.value(QStringLiteral("username")).toString());
  m_permissionLabel->setText(
      admin.value(QStringLiteral("permissions")).toString());
  m_navigation->setCurrentIndex(0);
  m_contentPages->setCurrentIndex(0);
  m_pages->setCurrentIndex(1);
  m_refreshTimer->start();
  QTimer::singleShot(0, this, &AdminMainWindow::refreshCurrentPage);
}

void AdminMainWindow::showDemoWorkspace() {
  showAdminHome({{QStringLiteral("username"), QStringLiteral("demo-admin")},
                 {QStringLiteral("permissions"), QStringLiteral("演示模式")}});
  m_refreshTimer->stop();
  setConnectionStatus(QStringLiteral("演示数据 · 未连接服务器"), false);
  connect(this, &AdminMainWindow::adminCommandRequested, this,
          [this](const QString &action, const QJsonObject &parameters) {
            if (action == QStringLiteral("dashboard.revenue")) {
              const int requestedDays =
                  parameters.value(QStringLiteral("days")).toInt(7);
              const int days = requestedDays == 30 ? 30 : 7;
              m_overviewPage->setRevenue(
                  {{QStringLiteral("data"),
                    QJsonObject{{QStringLiteral("points"),
                                 demoRevenuePoints(days)}}}});
              return;
            }
            const QStringList mutations = {
                QStringLiteral("charging.stop"),
                QStringLiteral("stations.create"),
                QStringLiteral("stations.update"),
                QStringLiteral("piles.control"),
                QStringLiteral("users.freeze")};
            if (mutations.contains(action)) {
              setNotice(QStringLiteral("演示模式为只读，操作不会提交到服务器"));
            }
          });

  m_overviewPage->setSummary(
      {{QStringLiteral("data"),
        QJsonObject{
            {QStringLiteral("revenueMetrics"),
             QJsonObject{{QStringLiteral("todayRevenueCents"), 30950},
                         {QStringLiteral("monthRevenueCents"), 386420},
                         {QStringLiteral("totalRevenueCents"), 2859360}}},
            {QStringLiteral("orderMetrics"),
             QJsonObject{{QStringLiteral("todayOrderCount"), 28},
                         {QStringLiteral("monthOrderCount"), 416},
                         {QStringLiteral("totalOrderCount"), 3289}}},
            {QStringLiteral("pileStatus"),
             QJsonObject{{QStringLiteral("total"), 48},
                         {QStringLiteral("idle"), 21},
                         {QStringLiteral("reserved"), 6},
                         {QStringLiteral("charging"), 14},
                         {QStringLiteral("fault"), 3},
                         {QStringLiteral("offline"), 2},
                         {QStringLiteral("disabled"), 2}}},
            {QStringLiteral("updatedAt"), QStringLiteral("刚刚")}}}});
  m_overviewPage->setRevenue(
      {{QStringLiteral("data"),
        QJsonObject{{QStringLiteral("points"), demoRevenuePoints(7)}}}});

  const QJsonObject pagination = {{QStringLiteral("page"), 1},
                                  {QStringLiteral("pageSize"), 15},
                                  {QStringLiteral("total"), 2},
                                  {QStringLiteral("totalPages"), 1}};
  m_monitorPage->setChargingData(
      {{QStringLiteral("data"),
        QJsonObject{
            {QStringLiteral("items"),
             QJsonArray{
                 QJsonObject{
                     {QStringLiteral("orderId"), 101},
                     {QStringLiteral("orderNo"),
                      QStringLiteral("CHG20260905001")},
                     {QStringLiteral("phone"), QStringLiteral("138****8000")},
                     {QStringLiteral("pileCode"), QStringLiteral("DL-SP-001")},
                     {QStringLiteral("energyWh"), 12600},
                     {QStringLiteral("powerKw"), 58.4},
                     {QStringLiteral("durationSeconds"), 1620},
                     {QStringLiteral("feeCents"), 1512},
                     {QStringLiteral("updatedAt"), QStringLiteral("10:28:16")}},
                 QJsonObject{
                     {QStringLiteral("orderId"), 102},
                     {QStringLiteral("orderNo"),
                      QStringLiteral("CHG20260905002")},
                     {QStringLiteral("phone"), QStringLiteral("139****9000")},
                     {QStringLiteral("pileCode"), QStringLiteral("DL-XH-001")},
                     {QStringLiteral("energyWh"), 8400},
                     {QStringLiteral("powerKw"), 112.7},
                     {QStringLiteral("durationSeconds"), 930},
                     {QStringLiteral("feeCents"), 1159},
                     {QStringLiteral("updatedAt"),
                      QStringLiteral("10:28:11")}}}},
            {QStringLiteral("meta"), pagination}}}});

  m_alarmsPage->setAlarms(
      {{QStringLiteral("data"),
        QJsonObject{
            {QStringLiteral("items"),
             QJsonArray{
                 QJsonObject{
                     {QStringLiteral("alarmId"), 12},
                     {QStringLiteral("severity"), QStringLiteral("critical")},
                     {QStringLiteral("alarmType"),
                      QStringLiteral("temperature")},
                     {QStringLiteral("pileCode"), QStringLiteral("DL-SP-006")},
                     {QStringLiteral("message"),
                      QStringLiteral("电桩温度超过安全阈值")},
                     {QStringLiteral("status"), QStringLiteral("open")},
                     {QStringLiteral("occurredAt"),
                      QStringLiteral("2026-09-05 10:22:08")}},
                 QJsonObject{
                     {QStringLiteral("alarmId"), 11},
                     {QStringLiteral("severity"), QStringLiteral("warning")},
                     {QStringLiteral("alarmType"), QStringLiteral("offline")},
                     {QStringLiteral("pileCode"), QStringLiteral("DL-XH-004")},
                     {QStringLiteral("message"),
                      QStringLiteral("设备心跳中断")},
                     {QStringLiteral("status"), QStringLiteral("acknowledged")},
                     {QStringLiteral("occurredAt"),
                      QStringLiteral("2026-09-05 09:58:31")}}}},
            {QStringLiteral("meta"), pagination}}}});

  const QJsonArray stations = {
      QJsonObject{
          {QStringLiteral("stationId"), 1},
          {QStringLiteral("name"), QStringLiteral("软件园充电站")},
          {QStringLiteral("address"), QStringLiteral("大连市高新园区软件园路")},
          {QStringLiteral("longitude"), 121.5312},
          {QStringLiteral("latitude"), 38.8584},
          {QStringLiteral("priceCentsPerKwh"), 120},
          {QStringLiteral("status"), QStringLiteral("online")},
          {QStringLiteral("pileCount"), 24},
          {QStringLiteral("updatedAt"), QStringLiteral("10:28")}},
      QJsonObject{
          {QStringLiteral("stationId"), 2},
          {QStringLiteral("name"), QStringLiteral("星海充电站")},
          {QStringLiteral("address"), QStringLiteral("大连市沙河口区星海广场")},
          {QStringLiteral("longitude"), 121.5868},
          {QStringLiteral("latitude"), 38.8817},
          {QStringLiteral("priceCentsPerKwh"), 138},
          {QStringLiteral("status"), QStringLiteral("online")},
          {QStringLiteral("pileCount"), 24},
          {QStringLiteral("updatedAt"), QStringLiteral("10:27")}}};
  m_assetsPage->setStations(
      {{QStringLiteral("data"),
        QJsonObject{{QStringLiteral("items"), stations},
                    {QStringLiteral("meta"), pagination}}}});
  m_assetsPage->setPiles(
      {{QStringLiteral("data"),
        QJsonObject{
            {QStringLiteral("items"),
             QJsonArray{
                 QJsonObject{
                     {QStringLiteral("pileId"), 1},
                     {QStringLiteral("pileCode"), QStringLiteral("DL-SP-001")},
                     {QStringLiteral("stationName"),
                      QStringLiteral("软件园充电站")},
                     {QStringLiteral("chargeType"), QStringLiteral("fast")},
                     {QStringLiteral("powerKw"), 60.0},
                     {QStringLiteral("status"), QStringLiteral("charging")},
                     {QStringLiteral("totalChargeCount"), 286},
                     {QStringLiteral("totalChargeSeconds"), 828000},
                     {QStringLiteral("lastHeartbeatAt"),
                      QStringLiteral("10:28:18")}},
                 QJsonObject{
                     {QStringLiteral("pileId"), 2},
                     {QStringLiteral("pileCode"), QStringLiteral("DL-SP-002")},
                     {QStringLiteral("stationName"),
                      QStringLiteral("软件园充电站")},
                     {QStringLiteral("chargeType"), QStringLiteral("slow")},
                     {QStringLiteral("powerKw"), 7.0},
                     {QStringLiteral("status"), QStringLiteral("idle")},
                     {QStringLiteral("totalChargeCount"), 143},
                     {QStringLiteral("totalChargeSeconds"), 1234000},
                     {QStringLiteral("lastHeartbeatAt"),
                      QStringLiteral("10:28:17")}}}},
            {QStringLiteral("meta"), pagination}}}});

  m_recordsPage->setUsers(
      {{QStringLiteral("data"),
        QJsonObject{
            {QStringLiteral("items"),
             QJsonArray{
                 QJsonObject{
                     {QStringLiteral("userId"), 1},
                     {QStringLiteral("phone"), QStringLiteral("138****8000")},
                     {QStringLiteral("nickname"), QStringLiteral("用户8000")},
                     {QStringLiteral("balanceCents"), 8650},
                     {QStringLiteral("createdAt"),
                      QStringLiteral("2026-08-18")},
                     {QStringLiteral("status"), QStringLiteral("normal")}},
                 QJsonObject{
                     {QStringLiteral("userId"), 2},
                     {QStringLiteral("phone"), QStringLiteral("139****9000")},
                     {QStringLiteral("nickname"), QStringLiteral("用户9000")},
                     {QStringLiteral("balanceCents"), 2200},
                     {QStringLiteral("createdAt"),
                      QStringLiteral("2026-08-26")},
                     {QStringLiteral("status"), QStringLiteral("frozen")}}}},
            {QStringLiteral("meta"), pagination}}}});
  m_recordsPage->setOrders(
      {{QStringLiteral("data"),
        QJsonObject{
            {QStringLiteral("items"),
             QJsonArray{
                 QJsonObject{
                     {QStringLiteral("orderNo"),
                      QStringLiteral("CHG20260904018")},
                     {QStringLiteral("phone"), QStringLiteral("138****8000")},
                     {QStringLiteral("stationName"),
                      QStringLiteral("软件园充电站")},
                     {QStringLiteral("pileCode"), QStringLiteral("DL-SP-001")},
                     {QStringLiteral("status"), QStringLiteral("completed")},
                     {QStringLiteral("energyWh"), 28600},
                     {QStringLiteral("durationSeconds"), 2750},
                     {QStringLiteral("feeCents"), 3432},
                     {QStringLiteral("startedAt"),
                      QStringLiteral("2026-09-04 18:12:03")},
                     {QStringLiteral("stoppedAt"),
                      QStringLiteral("2026-09-04 18:57:53")}},
                 QJsonObject{
                     {QStringLiteral("orderNo"),
                      QStringLiteral("CHG20260904017")},
                     {QStringLiteral("phone"), QStringLiteral("139****9000")},
                     {QStringLiteral("stationName"),
                      QStringLiteral("星海充电站")},
                     {QStringLiteral("pileCode"), QStringLiteral("DL-XH-001")},
                     {QStringLiteral("status"),
                      QStringLiteral("fault_stopped")},
                     {QStringLiteral("energyWh"), 7900},
                     {QStringLiteral("durationSeconds"), 840},
                     {QStringLiteral("feeCents"), 1090},
                     {QStringLiteral("startedAt"),
                      QStringLiteral("2026-09-04 17:26:12")},
                     {QStringLiteral("stoppedAt"),
                      QStringLiteral("2026-09-04 17:40:12")}}}},
            {QStringLiteral("meta"), pagination}}}});
}

void AdminMainWindow::showLoginPage() {
  m_appTitleLabel->show();
  m_statusLabel->show();
  m_refreshTimer->stop();
  m_passwordEdit->clear();
  setLoginBusy(false);
  m_pages->setCurrentIndex(0);
}

void AdminMainWindow::setCommandBusy(const QString &action, bool busy) {
  m_overviewPage->setLoading(action, busy);
  m_monitorPage->setLoading(action, busy);
  m_alarmsPage->setLoading(action, busy);
  m_assetsPage->setLoading(action, busy);
  m_recordsPage->setLoading(action, busy);
}

void AdminMainWindow::handleCommandSucceeded(const QString &action,
                                             const QJsonObject &payload) {
  if (action == QStringLiteral("dashboard.summary"))
    m_overviewPage->setSummary(payload);
  else if (action == QStringLiteral("dashboard.revenue"))
    m_overviewPage->setRevenue(payload);
  else if (action == QStringLiteral("charging.active.list"))
    m_monitorPage->setChargingData(payload);
  else if (action == QStringLiteral("alarms.list"))
    m_alarmsPage->setAlarms(payload);
  else if (action == QStringLiteral("alarms.detail"))
    m_alarmsPage->setAlarmDetail(payload);
  else if (action == QStringLiteral("stations.list"))
    m_assetsPage->setStations(payload);
  else if (action == QStringLiteral("stations.detail"))
    m_assetsPage->setStationDetail(payload);
  else if (action == QStringLiteral("piles.list"))
    m_assetsPage->setPiles(payload);
  else if (action == QStringLiteral("piles.detail"))
    m_assetsPage->setPileDetail(payload);
  else if (action == QStringLiteral("users.list"))
    m_recordsPage->setUsers(payload);
  else if (action == QStringLiteral("orders.list"))
    m_recordsPage->setOrders(payload);
  else if (action == QStringLiteral("charging.stop")) {
    setNotice(payload.value(QStringLiteral("message"))
                  .toString(QStringLiteral("远程停止请求已执行")));
    m_monitorPage->requestRefresh();
  } else if (action == QStringLiteral("stations.create") ||
             action == QStringLiteral("stations.update") ||
             action == QStringLiteral("piles.control")) {
    m_assetsPage->operationSucceeded(action, payload);
    setNotice(payload.value(QStringLiteral("message"))
                  .toString(QStringLiteral("资产操作成功")));
  } else if (action == QStringLiteral("users.freeze")) {
    m_recordsPage->operationSucceeded(action, payload);
    setNotice(payload.value(QStringLiteral("message"))
                  .toString(QStringLiteral("用户状态更新成功")));
  }
}

void AdminMainWindow::handleCommandFailed(const QString &action,
                                          const QString &message,
                                          int errorCode) {
  if (action.startsWith(QStringLiteral("dashboard.")))
    m_overviewPage->setError(message);
  else if (action.startsWith(QStringLiteral("charging.")))
    m_monitorPage->setError(message);
  else if (action.startsWith(QStringLiteral("alarms.")))
    m_alarmsPage->setError(message);
  else if (action.startsWith(QStringLiteral("stations.")) ||
           action.startsWith(QStringLiteral("piles."))) {
    m_assetsPage->setError(action, message);
  } else if (action.startsWith(QStringLiteral("users.")) ||
             action.startsWith(QStringLiteral("orders."))) {
    m_recordsPage->setError(action, message);
  }
  setNotice(message, true);
  if (errorCode == static_cast<int>(Charging::ErrorCode::Unauthorized) ||
      errorCode == static_cast<int>(Charging::ErrorCode::SessionExpired)) {
    showLoginPage();
    showLoginError(message);
  }
}

void AdminMainWindow::handlePush(quint16 messageType,
                                 const QJsonObject &payload) {
  const auto type = static_cast<Charging::MessageType>(messageType);
  if (type == Charging::MessageType::ChargingProgressPush) {
    m_monitorPage->applyProgressPush(payload);
  } else if (type == Charging::MessageType::ChargingStoppedPush) {
    m_monitorPage->applyStoppedPush(payload);
  } else if (type == Charging::MessageType::AlarmPush) {
    m_alarmsPage->applyAlarmPush(payload);
    setNotice(QStringLiteral("收到新的设备告警"), true);
  } else if (type == Charging::MessageType::DeviceStatusPush) {
    m_assetsPage->applyDevicePush(payload);
  }
}

void AdminMainWindow::refreshCurrentPage() {
  if (m_pages->currentIndex() != 1)
    return;
  switch (m_contentPages->currentIndex()) {
  case 0:
    m_overviewPage->requestRefresh();
    break;
  case 1:
    m_monitorPage->requestRefresh();
    break;
  case 2:
    m_alarmsPage->requestRefresh();
    break;
  case 3:
    m_assetsPage->requestRefresh();
    break;
  case 4:
    m_recordsPage->requestRefresh();
    break;
  default:
    break;
  }
}

void AdminMainWindow::setNotice(const QString &message, bool error) {
  if (message.trimmed().isEmpty()) {
    m_noticeLabel->hide();
    return;
  }
  m_noticeLabel->setText(message);
  m_noticeLabel->setProperty("error", error);
  m_noticeLabel->style()->unpolish(m_noticeLabel);
  m_noticeLabel->style()->polish(m_noticeLabel);
  m_noticeLabel->show();
  const QString displayedMessage = message;
  QTimer::singleShot(5'000, m_noticeLabel,
                     [label = m_noticeLabel, displayedMessage] {
                       if (label->text() == displayedMessage)
                         label->hide();
                     });
}

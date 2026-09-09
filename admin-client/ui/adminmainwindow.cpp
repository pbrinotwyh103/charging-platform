#include "ui/adminmainwindow.h"

#include "app/appinfo.h"
#include "pages/alarmspage.h"
#include "pages/assetspage.h"
#include "pages/monitorpage.h"
#include "pages/overviewpage.h"
#include "pages/recordspage.h"
#include "protocol/errorcodes.h"
#include "protocol/messagetypes.h"

#include <QAbstractItemView>
#include <QDate>
#include <QFormLayout>
#include <QFrame>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPushButton>
#include <QSize>
#include <QSpinBox>
#include <QStackedWidget>
#include <QStyle>
#include <QStringList>
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
  setWindowTitle(QStringLiteral("充电运营管理平台"));
  resize(1440, 900);
  setMinimumSize(1080, 680);
  setStyleSheet(QStringLiteral(R"(
        QMainWindow, QWidget { background:#f4f6f8; color:#1f2933; font-size:14px; font-family:"Microsoft YaHei UI","Microsoft YaHei","PingFang SC","Arial"; }
        QLabel { background:transparent; }
        QWidget#loginPage { background:#edf1f2; }
        QFrame#loginBrandPanel { background:#18373d; border:0; }
        QLabel#brandKicker { color:#4fd1c5; font-size:13px; font-weight:700; }
        QLabel#appTitle { color:#ffffff; font-size:34px; font-weight:700; }
        QLabel#brandSubtitle { color:#c6d6d9; font-size:16px; }
        QLabel#brandVersion { color:#8fa8ad; font-size:12px; }
        QWidget#loginSurface { background:#edf1f2; }
        QFrame#loginPanel { background:#ffffff; border:1px solid #d9e0e3; border-radius:8px; }
        QLabel#loginTitle { color:#172327; font-size:25px; font-weight:700; }
        QLabel#loginSubtitle { color:#66757a; font-size:13px; }
        QLabel#fieldLabel { color:#445258; font-size:12px; font-weight:600; }
        QLabel#loginErrorLabel { color:#b42336; background:#fff2f3; border:1px solid #ffd5da; border-radius:4px; padding:8px 10px; }
        QLabel#loginErrorLabel:empty { border:0; background:transparent; padding:0; }
        QLabel#pageTitle { font-size:22px; font-weight:700; color:#1f2933; }
        QLabel#workspaceTitle { font-size:22px; font-weight:700; color:#172327; }
        QLabel#workspaceSubtitle { color:#718087; font-size:12px; }
        QLabel#sectionTitle { font-size:15px; font-weight:700; color:#27363b; }
        QWidget#dashboardShell { background:#f4f6f8; }
        QFrame#sideBar { background:#182a2f; border:0; }
        QLabel#sideBrandMark { color:#13272c; background:#46c5b7; border-radius:6px; font-size:17px; font-weight:800; }
        QLabel#sideBrandTitle { color:#f7faf9; font-size:16px; font-weight:700; }
        QLabel#sideBrandCaption { color:#8fa6aa; font-size:11px; }
        QListWidget#sideNavigation { background:transparent; border:0; outline:0; color:#b9c8cb; }
        QListWidget#sideNavigation::item { min-height:46px; border-radius:5px; padding:0 12px; margin:2px 0; }
        QListWidget#sideNavigation::item:hover { background:#233c42; color:#ffffff; }
        QListWidget#sideNavigation::item:selected { background:#2c4d52; color:#ffffff; border-left:3px solid #46c5b7; padding-left:9px; }
        QFrame#sidebarAccount { background:#21373c; border:1px solid #2d474d; border-radius:6px; }
        QLabel#adminNameLabel { color:#f6f9f8; font-size:13px; font-weight:700; }
        QLabel#permissionLabel { color:#9fb3b7; font-size:11px; }
        QWidget#workspaceContent { background:#f4f6f8; }
        QFrame#workspaceHeader { background:transparent; border:0; border-bottom:1px solid #dce2e4; }
        QLabel#connectionStatus { padding:9px 11px; border:1px solid #d7dfe2; border-radius:4px; background:#f8fafb; color:#59686e; }
        QLabel#connectionStatus[connected="true"] { border-color:#a9ddd5; background:#edf9f7; color:#08786d; }
        QLabel#connectionStatus[connected="false"] { border-color:#f1c69c; background:#fff8ed; color:#9a5314; }
        QLabel#compactConnectionStatus { padding:7px 10px; border:1px solid #d7dfe2; border-radius:4px; font-size:12px; font-weight:700; }
        QLabel#compactConnectionStatus[connected="true"] { border-color:#a9ddd5; background:#edf9f7; color:#08786d; }
        QLabel#compactConnectionStatus[connected="false"] { border-color:#f1c69c; background:#fff8ed; color:#9a5314; }
        QLabel#noticeLabel { padding:9px 12px; border-left:3px solid #168b80; border-radius:4px; background:#edf8f6; color:#175f58; }
        QLabel#noticeLabel[error="true"] { border-left-color:#cf3f50; background:#fff1f2; color:#a82234; }
        QLabel#overviewStateLabel, QLabel#monitorStateLabel, QLabel#alarmStateLabel,
        QLabel#stationStateLabel, QLabel#pileStateLabel, QLabel#userStateLabel,
        QLabel#orderStateLabel { color:#68777c; font-size:12px; padding:4px 1px; }
        QFrame#metricCard { background:#ffffff; border:1px solid #dce2e4; border-radius:6px; }
        QLabel#metricCaption { color:#708087; font-size:12px; font-weight:600; }
        QLabel[class="metricValue"] { font-size:23px; font-weight:700; }
        QLabel[metricKind="revenue"] { color:#08786d; }
        QLabel[metricKind="orders"] { color:#324b55; }
        QWidget#revenueChartWidget, QWidget#pileStatusChartWidget { background:#ffffff; border:1px solid #dce2e4; border-radius:6px; }
        QFrame#filterBar, QFrame#tablePanel { background:#ffffff; border:1px solid #dce2e4; border-radius:6px; }
        QLineEdit, QSpinBox, QDoubleSpinBox, QDateEdit, QComboBox {
            min-height:36px; padding:0 10px; background:#ffffff; border:1px solid #cfd8dc; border-radius:4px; selection-background-color:#9fddd6;
        }
        QLineEdit:focus, QSpinBox:focus, QDoubleSpinBox:focus, QDateEdit:focus, QComboBox:focus { border:1px solid #168b80; }
        QComboBox::drop-down { width:26px; border:0; }
        QCheckBox { color:#4b5a60; spacing:7px; }
        QPushButton { min-height:36px; padding:0 14px; border:1px solid #cfd8dc; border-radius:4px; background:#ffffff; color:#334248; font-weight:600; }
        QPushButton:hover { background:#f2f7f6; border-color:#8ebcb6; }
        QPushButton:pressed { background:#e7f1ef; }
        QPushButton:disabled { color:#9ba7ab; background:#f2f4f5; border-color:#e1e6e8; }
        QPushButton#primaryButton { background:#168b80; border-color:#168b80; color:white; }
        QPushButton#primaryButton:hover { background:#11796f; }
        QPushButton#primaryButton:pressed { background:#0d6a61; }
        QPushButton#secondaryButton { color:#176e66; border-color:#9fcfc9; background:#f5fbfa; }
        QPushButton#dangerButton { color:#b72e40; border-color:#e7aeb6; background:#fffafa; }
        QPushButton#dangerButton:hover { background:#fff0f2; border-color:#d9808c; }
        QPushButton#sevenDaysButton, QPushButton#thirtyDaysButton { min-height:30px; max-height:30px; padding:0 11px; background:#f3f6f7; border-color:#d9e0e2; color:#536268; }
        QPushButton#sevenDaysButton { border-top-right-radius:0; border-bottom-right-radius:0; }
        QPushButton#thirtyDaysButton { border-top-left-radius:0; border-bottom-left-radius:0; }
        QPushButton#sevenDaysButton:checked, QPushButton#thirtyDaysButton:checked { background:#168b80; border-color:#168b80; color:white; }
        QPushButton#previousPageButton, QPushButton#nextPageButton { min-width:36px; max-width:36px; min-height:36px; max-height:36px; padding:0; }
        QLabel#pageNumberLabel { color:#68777c; font-size:12px; font-weight:600; }
        QToolButton { min-width:36px; max-width:36px; min-height:36px; max-height:36px; border:1px solid #cfd8dc; border-radius:4px; background:white; }
        QToolButton:hover { background:#eef5f4; border-color:#8ebcb6; }
        QToolButton:pressed { background:#e1eeec; }
        QTableWidget { background:white; alternate-background-color:#f8fafb; border:1px solid #dce2e4; border-radius:4px; gridline-color:transparent; selection-background-color:#dff2ef; selection-color:#173d39; outline:0; }
        QTableWidget::item { padding:6px 8px; border-bottom:1px solid #e9edef; }
        QHeaderView::section { min-height:38px; background:#eef2f3; color:#44545a; padding:6px 8px; border:0; border-bottom:1px solid #d8e0e2; font-weight:700; }
        QGroupBox { background:white; border:1px solid #dce2e4; border-radius:6px; margin-top:10px; padding:18px 14px 14px 14px; }
        QGroupBox::title { subcontrol-origin:margin; left:12px; padding:0 5px; color:#3e4d52; font-weight:700; }
        QTabWidget::pane { border:1px solid #dce2e4; border-radius:5px; background:#ffffff; top:-1px; }
        QTabBar::tab { min-height:36px; padding:0 18px; border:1px solid transparent; border-bottom:2px solid transparent; color:#68777c; background:transparent; }
        QTabBar::tab:hover { color:#27373c; background:#eef4f3; }
        QTabBar::tab:selected { border-bottom-color:#168b80; color:#126f66; background:white; font-weight:700; }
        QSplitter::handle { background:#e5eaec; width:1px; height:1px; }
        QScrollArea { background:transparent; border:0; }
        QScrollBar:vertical { background:#eef1f2; width:10px; margin:0; }
        QScrollBar::handle:vertical { background:#bac5c8; min-height:28px; border-radius:4px; }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height:0; }
        QToolTip { color:#ffffff; background:#26383d; border:0; padding:5px; }
    )"));

  auto *central = new QWidget(this);
  auto *root = new QVBoxLayout(central);
  root->setContentsMargins(0, 0, 0, 0);
  root->setSpacing(0);

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
  page->setObjectName(QStringLiteral("loginPage"));
  auto *pageLayout = new QHBoxLayout(page);
  pageLayout->setContentsMargins(0, 0, 0, 0);
  pageLayout->setSpacing(0);

  auto *brandPanel = new QFrame(page);
  brandPanel->setObjectName(QStringLiteral("loginBrandPanel"));
  brandPanel->setMinimumWidth(390);
  auto *brandLayout = new QVBoxLayout(brandPanel);
  brandLayout->setContentsMargins(64, 64, 64, 54);
  brandLayout->setSpacing(12);
  auto *brandKicker =
      new QLabel(QStringLiteral("EV OPERATIONS  /  PC ADMIN"), brandPanel);
  brandKicker->setObjectName(QStringLiteral("brandKicker"));
  m_appTitleLabel =
      new QLabel(QStringLiteral("充电运营\n管理平台"), brandPanel);
  m_appTitleLabel->setObjectName(QStringLiteral("appTitle"));
  auto *brandSubtitle =
      new QLabel(QStringLiteral("面向充电站运营人员的桌面管理端"), brandPanel);
  brandSubtitle->setObjectName(QStringLiteral("brandSubtitle"));
  auto *brandLine = new QFrame(brandPanel);
  brandLine->setFrameShape(QFrame::HLine);
  brandLine->setStyleSheet(QStringLiteral("background:#315158; max-height:1px;"));
  auto *brandVersion =
      new QLabel(QStringLiteral("Charging Platform  ·  %1")
                     .arg(Charging::AppInfo::Version),
                 brandPanel);
  brandVersion->setObjectName(QStringLiteral("brandVersion"));
  brandLayout->addStretch(2);
  brandLayout->addWidget(brandKicker);
  brandLayout->addWidget(m_appTitleLabel);
  brandLayout->addWidget(brandSubtitle);
  brandLayout->addSpacing(18);
  brandLayout->addWidget(brandLine);
  brandLayout->addStretch(3);
  brandLayout->addWidget(brandVersion);

  auto *loginSurface = new QWidget(page);
  loginSurface->setObjectName(QStringLiteral("loginSurface"));
  auto *surfaceLayout = new QVBoxLayout(loginSurface);
  surfaceLayout->setContentsMargins(56, 48, 56, 48);
  surfaceLayout->addStretch();

  auto *panel = new QFrame(loginSurface);
  panel->setObjectName(QStringLiteral("loginPanel"));
  panel->setMaximumWidth(460);
  panel->setMinimumWidth(400);
  auto *layout = new QVBoxLayout(panel);
  layout->setContentsMargins(36, 32, 36, 32);
  layout->setSpacing(10);
  auto *heading = new QLabel(QStringLiteral("管理员登录"), panel);
  heading->setObjectName(QStringLiteral("loginTitle"));
  auto *subtitle = new QLabel(QStringLiteral("请输入管理账号和服务端连接信息"), panel);
  subtitle->setObjectName(QStringLiteral("loginSubtitle"));
  layout->addWidget(heading);
  layout->addWidget(subtitle);
  layout->addSpacing(14);

  auto addFieldLabel = [layout, panel](const QString &text) {
    auto *label = new QLabel(text, panel);
    label->setObjectName(QStringLiteral("fieldLabel"));
    layout->addWidget(label);
  };

  addFieldLabel(QStringLiteral("管理员账号"));
  m_usernameEdit = new QLineEdit(QStringLiteral("admin"), panel);
  m_usernameEdit->setMaxLength(32);
  layout->addWidget(m_usernameEdit);
  addFieldLabel(QStringLiteral("登录密码"));
  m_passwordEdit = new QLineEdit(panel);
  m_passwordEdit->setEchoMode(QLineEdit::Password);
  m_passwordEdit->setMaxLength(128);
  m_passwordEdit->setPlaceholderText(QStringLiteral("管理员密码"));
  layout->addWidget(m_passwordEdit);
  layout->addSpacing(10);
  auto *endpointTitle = new QLabel(QStringLiteral("服务器连接"), panel);
  endpointTitle->setObjectName(QStringLiteral("sectionTitle"));
  layout->addWidget(endpointTitle);
  auto *serverForm = new QHBoxLayout;
  serverForm->setSpacing(10);
  m_hostEdit = new QLineEdit(QStringLiteral("127.0.0.1"), panel);
  m_hostEdit->setPlaceholderText(QStringLiteral("服务器地址"));
  m_portSpin = new QSpinBox(panel);
  m_portSpin->setRange(1, 65535);
  m_portSpin->setValue(Charging::AppInfo::DefaultServerPort);
  m_portSpin->setFixedWidth(112);
  serverForm->addWidget(m_hostEdit, 1);
  serverForm->addWidget(m_portSpin);
  layout->addLayout(serverForm);

  m_loginButton = new QPushButton(QStringLiteral("登录管理平台"), panel);
  m_loginButton->setObjectName(QStringLiteral("primaryButton"));
  m_loginButton->setMinimumHeight(40);
  layout->addSpacing(8);
  layout->addWidget(m_loginButton);
  m_loginErrorLabel = new QLabel(panel);
  m_loginErrorLabel->setObjectName(QStringLiteral("loginErrorLabel"));
  m_loginErrorLabel->setWordWrap(true);
  m_loginErrorLabel->hide();
  layout->addWidget(m_loginErrorLabel);

  m_statusLabel = new QLabel(QStringLiteral("尚未连接后台服务器"), panel);
  m_statusLabel->setObjectName(QStringLiteral("connectionStatus"));
  m_statusLabel->setProperty("connected", false);
  m_statusLabel->setWordWrap(true);
  layout->addSpacing(8);
  layout->addWidget(m_statusLabel);

  surfaceLayout->addWidget(panel, 0, Qt::AlignHCenter);
  surfaceLayout->addStretch();
  pageLayout->addWidget(brandPanel, 4);
  pageLayout->addWidget(loginSurface, 6);

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
  page->setObjectName(QStringLiteral("dashboardShell"));
  auto *shell = new QHBoxLayout(page);
  shell->setContentsMargins(0, 0, 0, 0);
  shell->setSpacing(0);

  auto *sidebar = new QFrame(page);
  sidebar->setObjectName(QStringLiteral("sideBar"));
  sidebar->setFixedWidth(224);
  auto *sidebarLayout = new QVBoxLayout(sidebar);
  sidebarLayout->setContentsMargins(18, 22, 18, 18);
  sidebarLayout->setSpacing(16);
  auto *brandRow = new QHBoxLayout;
  brandRow->setSpacing(10);
  auto *brandMark = new QLabel(QStringLiteral("EV"), sidebar);
  brandMark->setObjectName(QStringLiteral("sideBrandMark"));
  brandMark->setAlignment(Qt::AlignCenter);
  brandMark->setFixedSize(42, 42);
  auto *brandText = new QVBoxLayout;
  brandText->setSpacing(0);
  auto *brandTitle = new QLabel(QStringLiteral("充电运营中心"), sidebar);
  brandTitle->setObjectName(QStringLiteral("sideBrandTitle"));
  auto *brandCaption = new QLabel(QStringLiteral("PC 管理端"), sidebar);
  brandCaption->setObjectName(QStringLiteral("sideBrandCaption"));
  brandText->addWidget(brandTitle);
  brandText->addWidget(brandCaption);
  brandRow->addWidget(brandMark);
  brandRow->addLayout(brandText, 1);
  sidebarLayout->addLayout(brandRow);

  m_navigation = new QListWidget(sidebar);
  m_navigation->setObjectName(QStringLiteral("sideNavigation"));
  m_navigation->setFrameShape(QFrame::NoFrame);
  m_navigation->setSelectionMode(QAbstractItemView::SingleSelection);
  m_navigation->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  m_navigation->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  m_navigation->setIconSize(QSize(19, 19));
  const QList<QPair<QStyle::StandardPixmap, QString>> navigationItems = {
      {QStyle::SP_DesktopIcon, QStringLiteral("运营概览")},
      {QStyle::SP_MediaPlay, QStringLiteral("实时充电")},
      {QStyle::SP_MessageBoxWarning, QStringLiteral("异常告警")},
      {QStyle::SP_DriveHDIcon, QStringLiteral("站点资产")},
      {QStyle::SP_FileDialogDetailedView, QStringLiteral("用户订单")}};
  for (const auto &entry : navigationItems) {
    auto *item = new QListWidgetItem(style()->standardIcon(entry.first),
                                     entry.second, m_navigation);
    item->setSizeHint(QSize(184, 48));
    item->setTextAlignment(Qt::AlignVCenter);
  }
  sidebarLayout->addWidget(m_navigation, 1);

  auto *accountPanel = new QFrame(sidebar);
  accountPanel->setObjectName(QStringLiteral("sidebarAccount"));
  auto *accountLayout = new QVBoxLayout(accountPanel);
  accountLayout->setContentsMargins(12, 10, 12, 10);
  accountLayout->setSpacing(2);
  m_adminNameLabel = new QLabel(accountPanel);
  m_adminNameLabel->setObjectName(QStringLiteral("adminNameLabel"));
  m_permissionLabel = new QLabel(accountPanel);
  m_permissionLabel->setObjectName(QStringLiteral("permissionLabel"));
  m_permissionLabel->setWordWrap(true);
  accountLayout->addWidget(m_adminNameLabel);
  accountLayout->addWidget(m_permissionLabel);
  sidebarLayout->addWidget(accountPanel);

  auto *content = new QWidget(page);
  content->setObjectName(QStringLiteral("workspaceContent"));
  auto *root = new QVBoxLayout(content);
  root->setContentsMargins(24, 14, 24, 18);
  root->setSpacing(12);
  auto *header = new QFrame(content);
  header->setObjectName(QStringLiteral("workspaceHeader"));
  auto *toolbar = new QHBoxLayout(header);
  toolbar->setContentsMargins(0, 2, 0, 14);
  toolbar->setSpacing(10);
  auto *identity = new QVBoxLayout;
  identity->setSpacing(3);
  m_workspaceTitleLabel = new QLabel(QStringLiteral("运营概览"), header);
  m_workspaceTitleLabel->setObjectName(QStringLiteral("workspaceTitle"));
  identity->addWidget(m_workspaceTitleLabel);
  m_workspaceSubtitleLabel =
      new QLabel(QStringLiteral("查看核心运营指标与设备运行概况"), header);
  m_workspaceSubtitleLabel->setObjectName(QStringLiteral("workspaceSubtitle"));
  identity->addWidget(m_workspaceSubtitleLabel);
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

  shell->addWidget(sidebar);
  shell->addWidget(content, 1);
  connect(m_navigation, &QListWidget::currentRowChanged, this,
          [this](int index) {
    m_contentPages->setCurrentIndex(index);
    const QStringList titles = {
        QStringLiteral("运营概览"), QStringLiteral("实时充电"),
        QStringLiteral("异常告警"), QStringLiteral("站点资产"),
        QStringLiteral("用户订单")};
    const QStringList subtitles = {
        QStringLiteral("查看核心运营指标与设备运行概况"),
        QStringLiteral("跟踪活动订单并处理远程停止请求"),
        QStringLiteral("筛选设备告警并查看异常上下文"),
        QStringLiteral("维护充电站信息并管理电桩状态"),
        QStringLiteral("查询用户与订单，导出运营记录")};
    if (index >= 0 && index < titles.size())
      m_workspaceTitleLabel->setText(titles.at(index));
    if (index >= 0 && index < subtitles.size())
      m_workspaceSubtitleLabel->setText(subtitles.at(index));
    refreshCurrentPage();
  });
  m_navigation->setCurrentRow(0);

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
                              : QStringLiteral("登录管理平台"));
}

void AdminMainWindow::showLoginError(const QString &message) {
  m_loginErrorLabel->setText(message);
  m_loginErrorLabel->setVisible(!message.isEmpty());
}

void AdminMainWindow::showAdminHome(const QJsonObject &admin) {
  m_appTitleLabel->hide();
  m_statusLabel->hide();
  m_passwordEdit->clear();
  m_adminNameLabel->setText(
      admin.value(QStringLiteral("username")).toString());
  m_permissionLabel->setText(
      admin.value(QStringLiteral("permissions")).toString());
  m_navigation->setCurrentRow(0);
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
            if (action == QStringLiteral("report.revenue")) {
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
                QStringLiteral("admin.pileControl"),
                QStringLiteral("admin.saveStation"),
                QStringLiteral("admin.setUserStatus")};
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
                     {QStringLiteral("pileId"), 1},
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
                     {QStringLiteral("pileId"), 2},
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
  if (action == QStringLiteral("report.summary"))
    m_overviewPage->setSummary(payload);
  else if (action == QStringLiteral("report.revenue"))
    m_overviewPage->setRevenue(payload);
  else if (action == QStringLiteral("report.pileStates"))
    m_overviewPage->setPileStatus(payload);
  else if (action == QStringLiteral("admin.monitor"))
    m_monitorPage->setChargingData(payload);
  else if (action == QStringLiteral("admin.alarms"))
    m_alarmsPage->setAlarms(payload);
  else if (action == QStringLiteral("admin.stations"))
    m_assetsPage->setStations(payload);
  else if (action == QStringLiteral("admin.stationDetail"))
    m_assetsPage->setStationDetail(payload);
  else if (action == QStringLiteral("admin.piles"))
    m_assetsPage->setPiles(payload);
  else if (action == QStringLiteral("admin.users"))
    m_recordsPage->setUsers(payload);
  else if (action == QStringLiteral("orders.list"))
    m_recordsPage->setOrders(payload);
  else if (action == QStringLiteral("admin.pileControl")) {
    setNotice(payload.value(QStringLiteral("message"))
                  .toString(QStringLiteral("设备控制请求已受理")));
    m_monitorPage->requestRefresh();
    m_assetsPage->operationSucceeded(action, payload);
  } else if (action == QStringLiteral("admin.saveStation")) {
    m_assetsPage->operationSucceeded(action, payload);
    setNotice(payload.value(QStringLiteral("message"))
                  .toString(QStringLiteral("资产操作成功")));
  } else if (action == QStringLiteral("admin.setUserStatus")) {
    m_recordsPage->operationSucceeded(action, payload);
    setNotice(payload.value(QStringLiteral("message"))
                  .toString(QStringLiteral("用户状态更新成功")));
  }
}

void AdminMainWindow::handleCommandFailed(const QString &action,
                                          const QString &message,
                                          int errorCode) {
  if (action.startsWith(QStringLiteral("report.")))
    m_overviewPage->setError(message);
  else if (action == QStringLiteral("admin.monitor"))
    m_monitorPage->setError(message);
  else if (action == QStringLiteral("admin.alarms"))
    m_alarmsPage->setError(message);
  else if (action.startsWith(QStringLiteral("admin.station")) ||
           action == QStringLiteral("admin.stations") ||
           action == QStringLiteral("admin.piles") ||
           action == QStringLiteral("admin.saveStation")) {
    m_assetsPage->setError(action, message);
  } else if (action == QStringLiteral("admin.pileControl")) {
    m_monitorPage->setError(message);
    m_assetsPage->setError(action, message);
  } else if (action == QStringLiteral("admin.users") ||
             action == QStringLiteral("admin.setUserStatus") ||
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

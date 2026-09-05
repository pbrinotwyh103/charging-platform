#include "pages/recordspage.h"

#include "widgets/adminuihelpers.h"
#include "widgets/paginationbar.h"

#include <QAbstractItemView>
#include <QCheckBox>
#include <QComboBox>
#include <QDate>
#include <QDateEdit>
#include <QFileDialog>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QResizeEvent>
#include <QSaveFile>
#include <QStringConverter>
#include <QStringList>
#include <QTabWidget>
#include <QTableWidget>
#include <QTextStream>
#include <QTimer>
#include <QVBoxLayout>

RecordsPage::RecordsPage(QWidget *parent) : QWidget(parent) {
  setObjectName(QStringLiteral("recordsPage"));
  auto *root = new QVBoxLayout(this);
  root->setContentsMargins(8, 8, 8, 8);
  auto *title = new QLabel(QStringLiteral("用户与订单"), this);
  title->setObjectName(QStringLiteral("pageTitle"));
  root->addWidget(title);
  m_tabs = new QTabWidget(this);
  m_tabs->setObjectName(QStringLiteral("recordTabs"));
  m_tabs->addTab(createUsersTab(), QStringLiteral("用户管理"));
  m_tabs->addTab(createOrdersTab(), QStringLiteral("订单查询"));
  connect(m_tabs, &QTabWidget::currentChanged, this,
          [this] { requestRefresh(); });
  root->addWidget(m_tabs, 1);
  updateResponsiveLayout();
}

QWidget *RecordsPage::createUsersTab() {
  auto *page = new QWidget(this);
  auto *root = new QVBoxLayout(page);
  root->setContentsMargins(4, 8, 4, 4);
  root->setSpacing(8);
  auto *tools = new QGridLayout;
  m_phoneSearch = new QLineEdit(page);
  m_phoneSearch->setObjectName(QStringLiteral("userPhoneSearch"));
  m_phoneSearch->setPlaceholderText(QStringLiteral("手机号片段"));
  m_phoneSearch->setMaxLength(11);
  m_phoneSearch->setValidator(new QRegularExpressionValidator(
      QRegularExpression(QStringLiteral("\\d{0,11}")), m_phoneSearch));
  auto *searchButton = new QPushButton(QStringLiteral("查询"), page);
  m_freezeButton = new QPushButton(QStringLiteral("冻结用户"), page);
  m_freezeButton->setObjectName(QStringLiteral("dangerButton"));
  m_freezeButton->setEnabled(false);
  tools->setHorizontalSpacing(8);
  tools->setVerticalSpacing(8);
  tools->addWidget(m_phoneSearch, 0, 0);
  tools->addWidget(searchButton, 0, 1);
  tools->addWidget(m_freezeButton, 1, 0, 1, 2);
  tools->setColumnStretch(0, 1);
  tools->setColumnStretch(1, 1);
  root->addLayout(tools);

  m_searchTimer = new QTimer(this);
  m_searchTimer->setSingleShot(true);
  m_searchTimer->setInterval(350);
  connect(m_phoneSearch, &QLineEdit::textChanged, m_searchTimer,
          qOverload<>(&QTimer::start));
  connect(m_searchTimer, &QTimer::timeout, this, [this] { requestUsers(1); });
  connect(searchButton, &QPushButton::clicked, this,
          [this] { requestUsers(1); });
  connect(m_phoneSearch, &QLineEdit::returnPressed, searchButton,
          &QPushButton::click);
  connect(m_freezeButton, &QPushButton::clicked, this, [this] {
    const qint64 userId =
        AdminUi::integerId(m_selectedUser, QStringLiteral("userId"));
    if (userId <= 0)
      return;
    const bool freeze =
        m_selectedUser.value(QStringLiteral("status")).toString() !=
        QStringLiteral("frozen");
    const QString actionText =
        freeze ? QStringLiteral("冻结") : QStringLiteral("解冻");
    if (QMessageBox::question(
            this, actionText + QStringLiteral("用户"),
            QStringLiteral("确认%1用户 %2？")
                .arg(actionText, m_selectedUser.value(QStringLiteral("phone"))
                                     .toString())) == QMessageBox::Yes) {
      emit commandRequested(QStringLiteral("users.freeze"),
                            {{QStringLiteral("userId"), userId},
                             {QStringLiteral("frozen"), freeze}});
    }
  });

  m_userTable = new QTableWidget(page);
  m_userTable->setObjectName(QStringLiteral("userTable"));
  m_userTable->setColumnCount(6);
  m_userTable->setHorizontalHeaderLabels(
      {QStringLiteral("用户ID"), QStringLiteral("手机号"),
       QStringLiteral("昵称"), QStringLiteral("余额"),
       QStringLiteral("注册时间"), QStringLiteral("状态")});
  m_userTable->setSelectionBehavior(QAbstractItemView::SelectRows);
  m_userTable->setSelectionMode(QAbstractItemView::SingleSelection);
  m_userTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
  m_userTable->setAlternatingRowColors(true);
  m_userTable->verticalHeader()->setVisible(false);
  m_userTable->horizontalHeader()->setSectionResizeMode(
      QHeaderView::ResizeToContents);
  m_userTable->horizontalHeader()->setStretchLastSection(true);
  AdminUi::configureTouchTable(m_userTable);
  connect(m_userTable, &QTableWidget::itemSelectionChanged, this,
          &RecordsPage::updateUserSelection);
  root->addWidget(m_userTable, 1);
  m_userState = new QLabel(QStringLiteral("等待用户数据"), page);
  m_userState->setObjectName(QStringLiteral("userStateLabel"));
  m_userState->setWordWrap(true);
  root->addWidget(m_userState);
  m_userPagination = new PaginationBar(page);
  connect(m_userPagination, &PaginationBar::pageRequested, this,
          &RecordsPage::requestUsers);
  root->addWidget(m_userPagination);
  return page;
}

QWidget *RecordsPage::createOrdersTab() {
  auto *page = new QWidget(this);
  auto *root = new QVBoxLayout(page);
  root->setContentsMargins(4, 8, 4, 4);
  root->setSpacing(8);
  auto *filters = new QGridLayout;
  m_orderStatus = new QComboBox(page);
  m_orderStatus->setObjectName(QStringLiteral("orderStatusFilter"));
  m_orderStatus->addItem(QStringLiteral("全部状态"), QString());
  m_orderStatus->addItem(QStringLiteral("充电中"), QStringLiteral("charging"));
  m_orderStatus->addItem(QStringLiteral("已完成"), QStringLiteral("completed"));
  m_orderStatus->addItem(QStringLiteral("异常结束"),
                         QStringLiteral("fault_stopped"));
  m_orderStatus->addItem(QStringLiteral("已取消"), QStringLiteral("cancelled"));
  m_orderNumber = new QLineEdit(page);
  m_orderNumber->setPlaceholderText(QStringLiteral("订单号"));
  m_orderNumber->setObjectName(QStringLiteral("orderNumberSearch"));
  m_orderPhone = new QLineEdit(page);
  m_orderPhone->setPlaceholderText(QStringLiteral("手机号"));
  m_orderPhone->setObjectName(QStringLiteral("orderPhoneSearch"));
  m_orderPhone->setMaxLength(11);
  m_orderPhone->setValidator(new QRegularExpressionValidator(
      QRegularExpression(QStringLiteral("\\d{0,11}")), m_orderPhone));
  m_limitDates = new QCheckBox(QStringLiteral("日期"), page);
  m_fromDate = new QDateEdit(QDate::currentDate().addDays(-30), page);
  m_fromDate->setObjectName(QStringLiteral("orderFromDate"));
  m_fromDate->setCalendarPopup(true);
  m_fromDate->setDisplayFormat(QStringLiteral("yyyy-MM-dd"));
  m_toDate = new QDateEdit(QDate::currentDate(), page);
  m_toDate->setObjectName(QStringLiteral("orderToDate"));
  m_toDate->setCalendarPopup(true);
  m_toDate->setDisplayFormat(QStringLiteral("yyyy-MM-dd"));
  m_fromDate->setEnabled(false);
  m_toDate->setEnabled(false);
  connect(m_limitDates, &QCheckBox::toggled, m_fromDate, &QWidget::setEnabled);
  connect(m_limitDates, &QCheckBox::toggled, m_toDate, &QWidget::setEnabled);
  connect(m_limitDates, &QCheckBox::toggled, this,
          [this] { updateResponsiveLayout(); });
  auto *searchButton = new QPushButton(QStringLiteral("查询"), page);
  m_exportButton = new QPushButton(QStringLiteral("导出CSV"), page);
  m_exportButton->setObjectName(QStringLiteral("secondaryButton"));
  m_exportButton->setEnabled(false);
  filters->setHorizontalSpacing(8);
  filters->setVerticalSpacing(8);
  filters->addWidget(m_orderStatus, 0, 0);
  filters->addWidget(m_orderNumber, 0, 1);
  filters->addWidget(m_orderPhone, 1, 0);
  filters->addWidget(searchButton, 1, 1);
  filters->addWidget(m_limitDates, 2, 0);
  filters->addWidget(m_exportButton, 2, 1);
  filters->addWidget(m_fromDate, 3, 0);
  filters->addWidget(m_toDate, 3, 1);
  filters->setColumnStretch(0, 1);
  filters->setColumnStretch(1, 1);
  root->addLayout(filters);
  connect(searchButton, &QPushButton::clicked, this,
          [this] { requestOrders(1); });
  connect(m_exportButton, &QPushButton::clicked, this,
          &RecordsPage::exportOrdersInteractively);

  m_orderTable = new QTableWidget(page);
  m_orderTable->setObjectName(QStringLiteral("orderTable"));
  m_orderTable->setColumnCount(9);
  m_orderTable->setHorizontalHeaderLabels(
      {QStringLiteral("订单号"), QStringLiteral("用户"), QStringLiteral("站点"),
       QStringLiteral("电桩"), QStringLiteral("状态"), QStringLiteral("电量"),
       QStringLiteral("时长"), QStringLiteral("费用"),
       QStringLiteral("开始时间")});
  m_orderTable->setSelectionBehavior(QAbstractItemView::SelectRows);
  m_orderTable->setSelectionMode(QAbstractItemView::SingleSelection);
  m_orderTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
  m_orderTable->setAlternatingRowColors(true);
  m_orderTable->verticalHeader()->setVisible(false);
  m_orderTable->horizontalHeader()->setSectionResizeMode(
      QHeaderView::ResizeToContents);
  m_orderTable->horizontalHeader()->setStretchLastSection(true);
  AdminUi::configureTouchTable(m_orderTable);
  connect(m_orderTable, &QTableWidget::itemSelectionChanged, this,
          &RecordsPage::updateOrderSelection);
  root->addWidget(m_orderTable, 1);
  m_orderState = new QLabel(QStringLiteral("等待订单数据"), page);
  m_orderState->setObjectName(QStringLiteral("orderStateLabel"));
  m_orderState->setWordWrap(true);
  root->addWidget(m_orderState);
  m_orderPagination = new PaginationBar(page);
  connect(m_orderPagination, &PaginationBar::pageRequested, this,
          &RecordsPage::requestOrders);
  root->addWidget(m_orderPagination);
  return page;
}

void RecordsPage::requestRefresh() {
  if (m_tabs->currentIndex() == 0)
    requestUsers(m_userPagination->currentPage());
  else
    requestOrders(m_orderPagination->currentPage());
}

void RecordsPage::setUsers(const QJsonObject &payload) {
  const QJsonArray items = AdminUi::itemArray(payload);
  const QJsonObject meta = AdminUi::metaObject(payload);
  m_userTable->setRowCount(items.size());
  for (int row = 0; row < items.size(); ++row) {
    const QJsonObject user = items.at(row).toObject();
    auto *idItem = new QTableWidgetItem(
        QString::number(AdminUi::integerId(user, QStringLiteral("userId"))));
    idItem->setData(Qt::UserRole, user.toVariantMap());
    m_userTable->setItem(row, 0, idItem);
    m_userTable->setItem(
        row, 1,
        new QTableWidgetItem(user.value(QStringLiteral("phone")).toString()));
    m_userTable->setItem(
        row, 2,
        new QTableWidgetItem(
            user.value(QStringLiteral("nickname")).toString()));
    m_userTable->setItem(
        row, 3,
        new QTableWidgetItem(AdminUi::money(static_cast<qint64>(
            user.value(QStringLiteral("balanceCents")).toDouble()))));
    m_userTable->setItem(
        row, 4,
        new QTableWidgetItem(
            user.value(QStringLiteral("createdAt")).toString()));
    const QString status = user.value(QStringLiteral("status")).toString();
    auto *statusItem = new QTableWidgetItem(AdminUi::userStatus(status));
    AdminUi::styleStateItem(statusItem,
                            status == QStringLiteral("frozen")
                                ? AdminUi::Tone::Warning
                                : AdminUi::Tone::Success);
    m_userTable->setItem(row, 5, statusItem);
  }
  m_userPagination->setPage(
      meta.value(QStringLiteral("page")).toInt(1),
      meta.value(QStringLiteral("totalPages")).toInt(1),
      meta.value(QStringLiteral("total")).toInt(items.size()));
  m_userState->setText(
      items.isEmpty() ? QStringLiteral("没有匹配的用户")
                      : QStringLiteral("本页 %1 位用户").arg(items.size()));
  m_selectedUser = {};
  updateUserSelection();
}

void RecordsPage::setOrders(const QJsonObject &payload) {
  m_orders = AdminUi::itemArray(payload);
  const QJsonObject meta = AdminUi::metaObject(payload);
  m_orderTable->setRowCount(m_orders.size());
  for (int row = 0; row < m_orders.size(); ++row) {
    const QJsonObject order = m_orders.at(row).toObject();
    m_orderTable->setItem(
        row, 0,
        new QTableWidgetItem(
            order.value(QStringLiteral("orderNo")).toString()));
    m_orderTable->setItem(
        row, 1,
        new QTableWidgetItem(order.value(QStringLiteral("phone")).toString()));
    m_orderTable->setItem(
        row, 2,
        new QTableWidgetItem(
            order.value(QStringLiteral("stationName")).toString()));
    m_orderTable->setItem(
        row, 3,
        new QTableWidgetItem(
            order.value(QStringLiteral("pileCode")).toString()));
    const QString status = order.value(QStringLiteral("status")).toString();
    auto *statusItem = new QTableWidgetItem(AdminUi::orderStatus(status));
    AdminUi::Tone tone = AdminUi::Tone::Neutral;
    if (status == QStringLiteral("charging"))
      tone = AdminUi::Tone::Info;
    else if (status == QStringLiteral("completed"))
      tone = AdminUi::Tone::Success;
    else if (status == QStringLiteral("fault_stopped"))
      tone = AdminUi::Tone::Danger;
    AdminUi::styleStateItem(statusItem, tone);
    m_orderTable->setItem(row, 4, statusItem);
    m_orderTable->setItem(
        row, 5,
        new QTableWidgetItem(QStringLiteral("%1 kWh").arg(
            order.value(QStringLiteral("energyWh")).toDouble() / 1000.0, 0, 'f',
            2)));
    m_orderTable->setItem(
        row, 6,
        new QTableWidgetItem(AdminUi::duration(
            order.value(QStringLiteral("durationSeconds")).toInt())));
    m_orderTable->setItem(
        row, 7,
        new QTableWidgetItem(AdminUi::money(static_cast<qint64>(
            order.value(QStringLiteral("feeCents")).toDouble()))));
    m_orderTable->setItem(
        row, 8,
        new QTableWidgetItem(
            order.value(QStringLiteral("startedAt")).toString()));
  }
  m_orderPagination->setPage(
      meta.value(QStringLiteral("page")).toInt(1),
      meta.value(QStringLiteral("totalPages")).toInt(1),
      meta.value(QStringLiteral("total")).toInt(m_orders.size()));
  m_orderState->setText(
      m_orders.isEmpty()
          ? QStringLiteral("没有匹配的订单")
          : QStringLiteral("本页 %1 笔订单").arg(m_orders.size()));
  m_exportButton->setEnabled(!m_orders.isEmpty());
}

void RecordsPage::operationSucceeded(const QString &action,
                                     const QJsonObject &payload) {
  Q_UNUSED(payload)
  if (action == QStringLiteral("users.freeze")) {
    m_userState->setText(QStringLiteral("用户状态更新成功"));
    requestUsers(m_userPagination->currentPage());
  }
}

void RecordsPage::setLoading(const QString &action, bool loading) {
  if (action == QStringLiteral("users.freeze")) {
    m_freezeButton->setEnabled(!loading && !m_selectedUser.isEmpty());
  }
  if (!loading)
    return;
  if (action.startsWith(QStringLiteral("users."))) {
    m_userState->setText(QStringLiteral("正在处理用户数据…"));
  } else if (action.startsWith(QStringLiteral("orders."))) {
    m_orderState->setText(QStringLiteral("正在加载订单…"));
  }
}

void RecordsPage::setError(const QString &action, const QString &message) {
  if (action.startsWith(QStringLiteral("users.")))
    m_userState->setText(message);
  else
    m_orderState->setText(message);
}

void RecordsPage::requestUsers(int page) {
  emit commandRequested(
      QStringLiteral("users.list"),
      {{QStringLiteral("page"), page},
       {QStringLiteral("pageSize"), 15},
       {QStringLiteral("phone"), m_phoneSearch->text().trimmed()}});
}

void RecordsPage::requestOrders(int page) {
  if (m_limitDates->isChecked() && m_fromDate->date() > m_toDate->date()) {
    m_orderState->setText(QStringLiteral("开始日期不能晚于结束日期"));
    return;
  }
  emit commandRequested(
      QStringLiteral("orders.list"),
      {{QStringLiteral("page"), page},
       {QStringLiteral("pageSize"), 15},
       {QStringLiteral("status"), m_orderStatus->currentData().toString()},
       {QStringLiteral("orderNo"), m_orderNumber->text().trimmed()},
       {QStringLiteral("phone"), m_orderPhone->text().trimmed()},
       {QStringLiteral("from"), m_limitDates->isChecked()
                                    ? m_fromDate->date().toString(Qt::ISODate)
                                    : QString()},
       {QStringLiteral("to"), m_limitDates->isChecked()
                                  ? m_toDate->date().toString(Qt::ISODate)
                                  : QString()}});
}

void RecordsPage::updateUserSelection() {
  const int row = m_userTable->currentRow();
  if (row < 0 || !m_userTable->item(row, 0)) {
    m_selectedUser = {};
    m_freezeButton->setEnabled(false);
    m_freezeButton->setText(QStringLiteral("冻结用户"));
    updateResponsiveLayout();
    return;
  }
  m_selectedUser = QJsonObject::fromVariantMap(
      m_userTable->item(row, 0)->data(Qt::UserRole).toMap());
  const bool frozen =
      m_selectedUser.value(QStringLiteral("status")).toString() ==
      QStringLiteral("frozen");
  m_freezeButton->setText(frozen ? QStringLiteral("解冻用户")
                                 : QStringLiteral("冻结用户"));
  m_freezeButton->setEnabled(true);
  updateResponsiveLayout();
  m_userState->setText(
      QStringLiteral("%1 · %2\n余额 %3 · %4\n注册于 %5")
          .arg(m_selectedUser.value(QStringLiteral("phone")).toString(),
               m_selectedUser.value(QStringLiteral("nickname")).toString(),
               AdminUi::money(static_cast<qint64>(
                   m_selectedUser.value(QStringLiteral("balanceCents"))
                       .toDouble())),
               AdminUi::userStatus(
                   m_selectedUser.value(QStringLiteral("status")).toString()),
               m_selectedUser.value(QStringLiteral("createdAt")).toString()));
}

void RecordsPage::updateOrderSelection() {
  const int row = m_orderTable->currentRow();
  if (row < 0 || row >= m_orders.size())
    return;
  const QJsonObject order = m_orders.at(row).toObject();
  m_orderState->setText(
      QStringLiteral("%1 · %2\n%3 · %4\n%5 kWh · %6 · %7\n%8")
          .arg(order.value(QStringLiteral("phone")).toString(),
               AdminUi::orderStatus(
                   order.value(QStringLiteral("status")).toString()),
               order.value(QStringLiteral("stationName")).toString(),
               order.value(QStringLiteral("pileCode")).toString(),
               QString::number(
                   order.value(QStringLiteral("energyWh")).toDouble() / 1000.0,
                   'f', 2),
               AdminUi::duration(
                   order.value(QStringLiteral("durationSeconds")).toInt()),
               AdminUi::money(static_cast<qint64>(
                   order.value(QStringLiteral("feeCents")).toDouble())),
               order.value(QStringLiteral("startedAt")).toString()));
}

void RecordsPage::resizeEvent(QResizeEvent *event) {
  QWidget::resizeEvent(event);
  updateResponsiveLayout();
}

void RecordsPage::updateResponsiveLayout() {
  const bool compact = width() < 720;
  AdminUi::setResponsiveColumns(m_userTable, {1, 2, 5}, compact);
  AdminUi::setResponsiveColumns(m_orderTable, {0, 4, 7}, compact);
  const bool showDates = !compact || m_limitDates->isChecked();
  m_fromDate->setVisible(showDates);
  m_toDate->setVisible(showDates);
  m_freezeButton->setVisible(!compact || !m_selectedUser.isEmpty());
}

bool RecordsPage::exportOrdersCsv(const QString &filePath,
                                  QString *error) const {
  if (filePath.trimmed().isEmpty()) {
    if (error)
      *error = QStringLiteral("导出路径不能为空");
    return false;
  }
  QSaveFile file(filePath);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
    if (error)
      *error = file.errorString();
    return false;
  }
  QTextStream stream(&file);
  stream.setEncoding(QStringConverter::Utf8);
  stream << QChar(0xfeff)
         << QStringLiteral("订单号,手机号,充电站,电桩,状态,电量(kWh),时长(秒),"
                           "费用(元),开始时间,结束时间\n");
  for (const QJsonValue &value : m_orders) {
    const QJsonObject order = value.toObject();
    const QStringList columns = {
        order.value(QStringLiteral("orderNo")).toString(),
        order.value(QStringLiteral("phone")).toString(),
        order.value(QStringLiteral("stationName")).toString(),
        order.value(QStringLiteral("pileCode")).toString(),
        AdminUi::orderStatus(order.value(QStringLiteral("status")).toString()),
        QString::number(order.value(QStringLiteral("energyWh")).toDouble() /
                            1000.0,
                        'f', 2),
        QString::number(order.value(QStringLiteral("durationSeconds")).toInt()),
        QString::number(
            order.value(QStringLiteral("feeCents")).toDouble() / 100.0, 'f', 2),
        order.value(QStringLiteral("startedAt")).toString(),
        order.value(QStringLiteral("stoppedAt")).toString()};
    QStringList escaped;
    for (const QString &column : columns)
      escaped.append(AdminUi::csvCell(column));
    stream << escaped.join(QLatin1Char(',')) << QLatin1Char('\n');
  }
  if (!file.commit()) {
    if (error)
      *error = file.errorString();
    return false;
  }
  return true;
}

void RecordsPage::exportOrdersInteractively() {
  const QString path =
      QFileDialog::getSaveFileName(this, QStringLiteral("导出运营报表"),
                                   QStringLiteral("charging-orders.csv"),
                                   QStringLiteral("CSV 文件 (*.csv)"));
  if (path.isEmpty())
    return;
  QString error;
  if (!exportOrdersCsv(path, &error)) {
    QMessageBox::critical(this, QStringLiteral("导出失败"), error);
    return;
  }
  m_orderState->setText(QStringLiteral("报表已导出到 %1").arg(path));
}

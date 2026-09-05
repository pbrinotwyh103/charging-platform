#include "pages/alarmspage.h"

#include "widgets/adminuihelpers.h"
#include "widgets/paginationbar.h"

#include <QAbstractItemView>
#include <QComboBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QLabel>
#include <QPushButton>
#include <QResizeEvent>
#include <QTableWidget>
#include <QVBoxLayout>

AlarmsPage::AlarmsPage(QWidget *parent) : QWidget(parent) {
  setObjectName(QStringLiteral("alarmsPage"));
  auto *root = new QVBoxLayout(this);
  root->setContentsMargins(12, 12, 12, 12);
  root->setSpacing(9);
  auto *title = new QLabel(QStringLiteral("异常告警中心"), this);
  title->setObjectName(QStringLiteral("pageTitle"));
  root->addWidget(title);

  auto *filters = new QGridLayout;
  m_severity = new QComboBox(this);
  m_severity->setObjectName(QStringLiteral("alarmSeverityFilter"));
  m_severity->addItem(QStringLiteral("全部级别"), QString());
  m_severity->addItem(QStringLiteral("严重"), QStringLiteral("critical"));
  m_severity->addItem(QStringLiteral("警告"), QStringLiteral("warning"));
  m_severity->addItem(QStringLiteral("提示"), QStringLiteral("info"));
  m_status = new QComboBox(this);
  m_status->setObjectName(QStringLiteral("alarmStatusFilter"));
  m_status->addItem(QStringLiteral("全部状态"), QString());
  m_status->addItem(QStringLiteral("待处理"), QStringLiteral("open"));
  m_status->addItem(QStringLiteral("已确认"), QStringLiteral("acknowledged"));
  m_status->addItem(QStringLiteral("已恢复"), QStringLiteral("resolved"));
  filters->setHorizontalSpacing(8);
  filters->setVerticalSpacing(8);
  filters->addWidget(m_severity, 0, 0);
  filters->addWidget(m_status, 0, 1);
  filters->setColumnStretch(0, 1);
  filters->setColumnStretch(1, 1);
  root->addLayout(filters);

  connect(m_severity, &QComboBox::currentIndexChanged, this,
          [this] { requestPage(1); });
  connect(m_status, &QComboBox::currentIndexChanged, this,
          [this] { requestPage(1); });
  m_table = new QTableWidget(this);
  m_table->setObjectName(QStringLiteral("alarmTable"));
  m_table->setColumnCount(7);
  m_table->setHorizontalHeaderLabels(
      {QStringLiteral("编号"), QStringLiteral("级别"), QStringLiteral("类型"),
       QStringLiteral("电桩"), QStringLiteral("内容"), QStringLiteral("状态"),
       QStringLiteral("发生时间")});
  m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
  m_table->setSelectionMode(QAbstractItemView::SingleSelection);
  m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
  m_table->setAlternatingRowColors(true);
  m_table->verticalHeader()->setVisible(false);
  m_table->horizontalHeader()->setSectionResizeMode(
      QHeaderView::ResizeToContents);
  m_table->horizontalHeader()->setStretchLastSection(true);
  AdminUi::configureTouchTable(m_table);
  connect(m_table, &QTableWidget::itemSelectionChanged, this, [this] {
    const int row = m_table->currentRow();
    if (row < 0 || !m_table->item(row, 0))
      return;
    const QJsonObject alarm = QJsonObject::fromVariantMap(
        m_table->item(row, 0)->data(Qt::UserRole).toMap());
    showDetail(alarm);
    updateResponsiveLayout();
    const qint64 alarmId = AdminUi::integerId(alarm, QStringLiteral("alarmId"));
    if (alarmId > 0) {
      emit commandRequested(QStringLiteral("alarms.detail"),
                            {{QStringLiteral("alarmId"), alarmId}});
    }
  });
  root->addWidget(m_table, 1);

  m_stateLabel = new QLabel(QStringLiteral("等待告警数据"), this);
  m_stateLabel->setObjectName(QStringLiteral("alarmStateLabel"));
  m_stateLabel->setWordWrap(true);
  root->addWidget(m_stateLabel);

  m_detailBox = new QGroupBox(QStringLiteral("告警详情"), this);
  m_detailBox->setObjectName(QStringLiteral("alarmDetailBox"));
  auto *detailLayout = new QVBoxLayout(m_detailBox);
  m_detailLabel = new QLabel(QStringLiteral("请选择一条告警"), m_detailBox);
  m_detailLabel->setObjectName(QStringLiteral("alarmDetailLabel"));
  m_detailLabel->setWordWrap(true);
  m_detailLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
  detailLayout->addWidget(m_detailLabel);
  root->addWidget(m_detailBox);

  m_pagination = new PaginationBar(this);
  connect(m_pagination, &PaginationBar::pageRequested, this,
          &AlarmsPage::requestPage);
  root->addWidget(m_pagination);
  updateResponsiveLayout();
}

void AlarmsPage::requestRefresh() { requestPage(m_pagination->currentPage()); }

void AlarmsPage::setAlarms(const QJsonObject &payload) {
  const QJsonArray items = AdminUi::itemArray(payload);
  const QJsonObject meta = AdminUi::metaObject(payload);
  m_table->setRowCount(items.size());
  for (int row = 0; row < items.size(); ++row) {
    const QJsonObject alarm = items.at(row).toObject();
    auto *idItem = new QTableWidgetItem(
        QString::number(AdminUi::integerId(alarm, QStringLiteral("alarmId"))));
    idItem->setData(Qt::UserRole, alarm.toVariantMap());
    m_table->setItem(row, 0, idItem);
    const QString severity =
        alarm.value(QStringLiteral("severity")).toString();
    auto *severityItem =
        new QTableWidgetItem(AdminUi::alarmSeverity(severity));
    AdminUi::styleStateItem(
        severityItem, severity == QStringLiteral("critical")
                          ? AdminUi::Tone::Danger
                          : severity == QStringLiteral("warning")
                                ? AdminUi::Tone::Warning
                                : AdminUi::Tone::Info);
    m_table->setItem(row, 1, severityItem);
    m_table->setItem(row, 2,
                     new QTableWidgetItem(
                         alarm.value(QStringLiteral("alarmType")).toString()));
    m_table->setItem(row, 3,
                     new QTableWidgetItem(
                         alarm.value(QStringLiteral("pileCode")).toString()));
    m_table->setItem(row, 4,
                     new QTableWidgetItem(
                         alarm.value(QStringLiteral("message")).toString()));
    const QString status = alarm.value(QStringLiteral("status")).toString();
    auto *statusItem = new QTableWidgetItem(AdminUi::alarmStatus(status));
    AdminUi::styleStateItem(
        statusItem, status == QStringLiteral("resolved")
                        ? AdminUi::Tone::Success
                        : status == QStringLiteral("acknowledged")
                              ? AdminUi::Tone::Warning
                              : AdminUi::Tone::Danger);
    m_table->setItem(row, 5, statusItem);
    m_table->setItem(row, 6,
                     new QTableWidgetItem(
                         alarm.value(QStringLiteral("occurredAt")).toString()));
  }
  m_pagination->setPage(
      meta.value(QStringLiteral("page")).toInt(1),
      meta.value(QStringLiteral("totalPages")).toInt(1),
      meta.value(QStringLiteral("total")).toInt(items.size()));
  m_stateLabel->setText(
      items.isEmpty() ? QStringLiteral("当前筛选条件下没有告警")
                      : QStringLiteral("本页 %1 条告警").arg(items.size()));
  if (items.isEmpty())
    m_detailLabel->setText(QStringLiteral("请选择一条告警"));
}

void AlarmsPage::setAlarmDetail(const QJsonObject &payload) {
  showDetail(AdminUi::dataObject(payload));
}

void AlarmsPage::applyAlarmPush(const QJsonObject &payload) {
  const QString message = payload.value(QStringLiteral("message")).toString();
  m_stateLabel->setText(message.isEmpty()
                            ? QStringLiteral("收到新告警，正在刷新…")
                            : QStringLiteral("新告警：%1").arg(message));
  requestRefresh();
}

void AlarmsPage::setLoading(const QString &action, bool loading) {
  if (loading && action.startsWith(QStringLiteral("alarms."))) {
    m_stateLabel->setText(QStringLiteral("正在加载告警数据…"));
  }
}

void AlarmsPage::setError(const QString &message) {
  m_stateLabel->setText(message);
}

void AlarmsPage::requestPage(int page) {
  emit commandRequested(
      QStringLiteral("alarms.list"),
      {{QStringLiteral("page"), page},
       {QStringLiteral("pageSize"), 15},
       {QStringLiteral("severity"), m_severity->currentData().toString()},
       {QStringLiteral("status"), m_status->currentData().toString()}});
}

void AlarmsPage::showDetail(const QJsonObject &alarm) {
  if (alarm.isEmpty()) {
    m_detailLabel->setText(QStringLiteral("告警详情不可用"));
    return;
  }
  m_detailLabel->setText(
      QStringLiteral("%1 · %2\n电桩：%3\n订单：%4\n%5\n发生：%6\n恢复：%7")
          .arg(AdminUi::alarmSeverity(
                   alarm.value(QStringLiteral("severity")).toString()),
               AdminUi::alarmStatus(
                   alarm.value(QStringLiteral("status")).toString()),
               alarm.value(QStringLiteral("pileCode"))
                   .toString(QStringLiteral("--")),
               alarm.value(QStringLiteral("orderNo"))
                   .toString(QStringLiteral("--")),
               alarm.value(QStringLiteral("message")).toString(),
               alarm.value(QStringLiteral("occurredAt"))
                   .toString(QStringLiteral("--")),
               alarm.value(QStringLiteral("recoveredAt"))
                   .toString(QStringLiteral("--"))));
}

void AlarmsPage::resizeEvent(QResizeEvent *event) {
  QWidget::resizeEvent(event);
  updateResponsiveLayout();
}

void AlarmsPage::updateResponsiveLayout() {
  const bool compact = width() < 720;
  AdminUi::setResponsiveColumns(m_table, {1, 3, 5}, compact);
  if (m_detailBox)
    m_detailBox->setVisible(!compact || m_table->currentRow() >= 0);
}

#include "pages/monitorpage.h"

#include "widgets/adminuihelpers.h"
#include "widgets/paginationbar.h"

#include <QAbstractItemView>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QResizeEvent>
#include <QTableWidget>
#include <QVBoxLayout>

MonitorPage::MonitorPage(QWidget *parent) : QWidget(parent) {
  setObjectName(QStringLiteral("monitorPage"));
  auto *root = new QVBoxLayout(this);
  root->setContentsMargins(12, 12, 12, 12);
  root->setSpacing(9);

  auto *title = new QLabel(QStringLiteral("实时充电监控"), this);
  title->setObjectName(QStringLiteral("pageTitle"));
  root->addWidget(title);

  m_table = new QTableWidget(this);
  m_table->setObjectName(QStringLiteral("chargingTable"));
  m_table->setColumnCount(8);
  m_table->setHorizontalHeaderLabels(
      {QStringLiteral("订单号"), QStringLiteral("用户"), QStringLiteral("电桩"),
       QStringLiteral("电量(kWh)"), QStringLiteral("功率(kW)"),
       QStringLiteral("时长"), QStringLiteral("费用"),
       QStringLiteral("更新时间")});
  m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
  m_table->setSelectionMode(QAbstractItemView::SingleSelection);
  m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
  m_table->setAlternatingRowColors(true);
  m_table->verticalHeader()->setVisible(false);
  m_table->horizontalHeader()->setSectionResizeMode(
      QHeaderView::ResizeToContents);
  m_table->horizontalHeader()->setStretchLastSection(true);
  AdminUi::configureTouchTable(m_table);
  connect(m_table, &QTableWidget::itemSelectionChanged, this,
          &MonitorPage::updateSelection);
  root->addWidget(m_table, 1);

  m_stateLabel = new QLabel(QStringLiteral("等待实时充电数据"), this);
  m_stateLabel->setObjectName(QStringLiteral("monitorStateLabel"));
  m_stateLabel->setWordWrap(true);
  root->addWidget(m_stateLabel);

  auto *footer = new QHBoxLayout;
  m_stopButton = new QPushButton(QStringLiteral("远程停止"), this);
  m_stopButton->setObjectName(QStringLiteral("dangerButton"));
  m_stopButton->setEnabled(false);
  connect(m_stopButton, &QPushButton::clicked, this, [this] {
    const int row = m_table->currentRow();
    if (row < 0 || !m_table->item(row, 0))
      return;
    const qint64 orderId =
        m_table->item(row, 0)->data(Qt::UserRole).toLongLong();
    if (orderId <= 0)
      return;
    const auto answer =
        QMessageBox::question(this, QStringLiteral("确认远程停止"),
                              QStringLiteral("确认停止订单 %1 的充电？")
                                  .arg(m_table->item(row, 0)->text()));
    if (answer == QMessageBox::Yes) {
      emit commandRequested(QStringLiteral("charging.stop"),
                            {{QStringLiteral("orderId"), orderId}});
    }
  });
  m_pagination = new PaginationBar(this);
  connect(m_pagination, &PaginationBar::pageRequested, this,
          &MonitorPage::requestPage);
  footer->addWidget(m_stopButton);
  footer->addWidget(m_pagination, 1);
  root->addLayout(footer);
  updateResponsiveLayout();
}

void MonitorPage::requestRefresh() { requestPage(m_pagination->currentPage()); }

void MonitorPage::setChargingData(const QJsonObject &payload) {
  const QJsonArray items = AdminUi::itemArray(payload);
  const QJsonObject meta = AdminUi::metaObject(payload);
  m_table->setRowCount(items.size());
  for (int row = 0; row < items.size(); ++row) {
    const QJsonObject item = items.at(row).toObject();
    auto *orderItem =
        new QTableWidgetItem(item.value(QStringLiteral("orderNo")).toString());
    orderItem->setData(Qt::UserRole,
                       AdminUi::integerId(item, QStringLiteral("orderId")));
    m_table->setItem(row, 0, orderItem);
    m_table->setItem(
        row, 1,
        new QTableWidgetItem(item.value(QStringLiteral("phone")).toString()));
    m_table->setItem(row, 2,
                     new QTableWidgetItem(
                         item.value(QStringLiteral("pileCode")).toString()));
    m_table->setItem(
        row, 3,
        new QTableWidgetItem(QString::number(
            item.value(QStringLiteral("energyWh")).toDouble() / 1000.0, 'f',
            2)));
    m_table->setItem(
        row, 4,
        new QTableWidgetItem(QString::number(
            item.value(QStringLiteral("powerKw")).toDouble(), 'f', 1)));
    m_table->setItem(
        row, 5,
        new QTableWidgetItem(AdminUi::duration(
            item.value(QStringLiteral("durationSeconds")).toInt())));
    m_table->setItem(row, 6,
                     new QTableWidgetItem(AdminUi::money(static_cast<qint64>(
                         item.value(QStringLiteral("feeCents")).toDouble()))));
    m_table->setItem(row, 7,
                     new QTableWidgetItem(
                         item.value(QStringLiteral("updatedAt")).toString()));
  }
  m_pagination->setPage(
      meta.value(QStringLiteral("page")).toInt(1),
      meta.value(QStringLiteral("totalPages")).toInt(1),
      meta.value(QStringLiteral("total")).toInt(items.size()));
  m_stateLabel->setText(
      items.isEmpty()
          ? QStringLiteral("当前没有正在充电的订单")
          : QStringLiteral("正在监控 %1 条充电记录").arg(items.size()));
  updateSelection();
}

void MonitorPage::applyProgressPush(const QJsonObject &payload) {
  const int row =
      rowForOrder(AdminUi::integerId(payload, QStringLiteral("orderId")));
  if (row < 0) {
    requestRefresh();
    return;
  }
  m_table->item(row, 3)->setText(QString::number(
      payload.value(QStringLiteral("energyWh")).toDouble() / 1000.0, 'f', 2));
  m_table->item(row, 4)->setText(QString::number(
      payload.value(QStringLiteral("powerKw")).toDouble(), 'f', 1));
  m_table->item(row, 5)->setText(AdminUi::duration(
      payload.value(QStringLiteral("durationSeconds")).toInt()));
  m_table->item(row, 6)->setText(AdminUi::money(static_cast<qint64>(
      payload.value(QStringLiteral("feeCents")).toDouble())));
  m_table->item(row, 7)->setText(
      payload.value(QStringLiteral("updatedAt")).toString());
  if (m_table->currentRow() == row)
    updateSelection();
}

void MonitorPage::applyStoppedPush(const QJsonObject &payload) {
  const int row =
      rowForOrder(AdminUi::integerId(payload, QStringLiteral("orderId")));
  if (row >= 0)
    m_table->removeRow(row);
  m_stateLabel->setText(QStringLiteral("订单已停止：%1")
                            .arg(payload.value(QStringLiteral("stopReason"))
                                     .toString(QStringLiteral("远程停止"))));
  updateSelection();
}

void MonitorPage::setLoading(const QString &action, bool loading) {
  if (action == QStringLiteral("charging.active.list") && loading) {
    m_stateLabel->setText(QStringLiteral("正在加载实时充电记录…"));
  }
  if (action == QStringLiteral("charging.stop")) {
    m_stopButton->setEnabled(!loading && m_table->currentRow() >= 0);
  }
}

void MonitorPage::setError(const QString &message) {
  m_stateLabel->setText(message);
}

void MonitorPage::requestPage(int page) {
  emit commandRequested(
      QStringLiteral("charging.active.list"),
      {{QStringLiteral("page"), page}, {QStringLiteral("pageSize"), 15}});
}

void MonitorPage::updateSelection() {
  const int row = m_table->currentRow();
  m_stopButton->setEnabled(row >= 0);
  if (row < 0 || !m_table->item(row, 0)) {
    updateResponsiveLayout();
    return;
  }
  m_stateLabel->setText(
      QStringLiteral("%1 · %2\n%3 kWh · %4 kW · %5\n费用 %6 · 更新 %7")
          .arg(m_table->item(row, 1)->text(), m_table->item(row, 2)->text(),
               m_table->item(row, 3)->text(), m_table->item(row, 4)->text(),
               m_table->item(row, 5)->text(), m_table->item(row, 6)->text(),
               m_table->item(row, 7)->text()));
  updateResponsiveLayout();
}

void MonitorPage::resizeEvent(QResizeEvent *event) {
  QWidget::resizeEvent(event);
  updateResponsiveLayout();
}

void MonitorPage::updateResponsiveLayout() {
  const bool compact = width() < 720;
  AdminUi::setResponsiveColumns(m_table, {0, 2, 6}, compact);
  m_stopButton->setVisible(!compact || m_table->currentRow() >= 0);
}

int MonitorPage::rowForOrder(qint64 orderId) const {
  for (int row = 0; row < m_table->rowCount(); ++row) {
    if (m_table->item(row, 0) &&
        m_table->item(row, 0)->data(Qt::UserRole).toLongLong() == orderId) {
      return row;
    }
  }
  return -1;
}

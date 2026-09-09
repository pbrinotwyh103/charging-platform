#include "charts/pilestatuschartwidget.h"

#include <QtCharts/QChart>
#include <QtCharts/QChartView>
#include <QtCharts/QLegend>
#include <QtCharts/QPieSeries>
#include <QtCharts/QPieSlice>

#include <QColor>
#include <QAbstractItemView>
#include <QFrame>
#include <QHeaderView>
#include <QLabel>
#include <QList>
#include <QPainter>
#include <QTableWidget>
#include <QVBoxLayout>

namespace {

int statusCount(const QJsonObject &status, const QString &key) {
  return qMax(0, status.value(key).toInt());
}

} // namespace

PileStatusChartWidget::PileStatusChartWidget(QWidget *parent)
    : QWidget(parent) {
  setObjectName(QStringLiteral("pileStatusChartWidget"));
  auto *root = new QVBoxLayout(this);
  root->setContentsMargins(0, 0, 0, 0);
  root->setSpacing(6);
  auto *title = new QLabel(QStringLiteral("电桩状态占比"), this);
  title->setObjectName(QStringLiteral("sectionTitle"));
  root->addWidget(title);

  m_series = new QPieSeries(this);
  m_series->setHoleSize(0.48);
  auto *chart = new QChart;
  chart->setBackgroundVisible(false);
  chart->setMargins(QMargins(0, 0, 0, 0));
  chart->legend()->setAlignment(Qt::AlignBottom);
  chart->legend()->setLabelColor(QColor(QStringLiteral("#526560")));
  chart->addSeries(m_series);
  auto *view = new QChartView(chart, this);
  view->setObjectName(QStringLiteral("pileStatusChartView"));
  view->setRenderHint(QPainter::Antialiasing);
  view->setFrameShape(QFrame::NoFrame);
  view->setBackgroundBrush(Qt::transparent);
  view->setMinimumHeight(235);
  root->addWidget(view);

  m_statusTable = new QTableWidget(3, 3, this);
  m_statusTable->setObjectName(QStringLiteral("pileStatusTable"));
  m_statusTable->setHorizontalHeaderLabels(
      {QStringLiteral("状态"), QStringLiteral("数量"), QStringLiteral("占比")});
  m_statusTable->setVerticalHeaderLabels(
      {QStringLiteral("在用"), QStringLiteral("闲置"), QStringLiteral("故障")});
  m_statusTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
  m_statusTable->setSelectionMode(QAbstractItemView::NoSelection);
  m_statusTable->horizontalHeader()->setStretchLastSection(true);
  m_statusTable->verticalHeader()->setVisible(false);
  m_statusTable->setMaximumHeight(132);
  root->addWidget(m_statusTable);

  m_summaryLabel = new QLabel(this);
  m_summaryLabel->setObjectName(QStringLiteral("pileStatusSummaryLabel"));
  m_summaryLabel->setAlignment(Qt::AlignCenter);
  root->addWidget(m_summaryLabel);
  setStatusData({});
}

void PileStatusChartWidget::setStatusData(const QJsonObject &status) {
  m_series->clear();
  const int idle = statusCount(status, QStringLiteral("idle"));
  const int reserved = statusCount(status, QStringLiteral("reserved"));
  int charging = statusCount(status, QStringLiteral("charging"));
  const int fault = statusCount(status, QStringLiteral("fault"));
  const int offline = statusCount(status, QStringLiteral("offline"));
  const int disabled = statusCount(status, QStringLiteral("disabled"));
  if (charging == 0 && reserved == 0 && status.contains(QStringLiteral("inUse")))
    charging = statusCount(status, QStringLiteral("inUse"));
  const int sum = idle + reserved + charging + fault + offline + disabled;
  const int total = qMax(sum, status.value(QStringLiteral("total")).toInt(sum));

  const QList<QPair<QString, int>> requiredRows = {
      {QStringLiteral("在用"), charging + reserved},
      {QStringLiteral("闲置"), idle},
      {QStringLiteral("故障"), fault + offline + disabled}};
  for (int row = 0; row < requiredRows.size(); ++row) {
    const int count = requiredRows.at(row).second;
    const double percentage = total > 0 ? count * 100.0 / total : 0.0;
    m_statusTable->setItem(row, 0,
                           new QTableWidgetItem(requiredRows.at(row).first));
    m_statusTable->setItem(row, 1,
                           new QTableWidgetItem(QString::number(count)));
    m_statusTable->setItem(
        row, 2,
        new QTableWidgetItem(
            QStringLiteral("%1%").arg(percentage, 0, 'f', 1)));
  }

  struct SliceData {
    QString name;
    int count;
    QColor color;
  };
  const QList<SliceData> slices = {
      {QStringLiteral("空闲"), idle, QColor(QStringLiteral("#16a34a"))},
      {QStringLiteral("充电"), charging, QColor(QStringLiteral("#2563eb"))},
      {QStringLiteral("预约"), reserved, QColor(QStringLiteral("#d97706"))},
      {QStringLiteral("故障"), fault, QColor(QStringLiteral("#dc2626"))},
      {QStringLiteral("离线/停用"), offline + disabled,
       QColor(QStringLiteral("#64748b"))}};
  for (const SliceData &item : slices) {
    if (item.count == 0)
      continue;
    const double percentage = total > 0
                                  ? item.count * 100.0 / total
                                  : 0.0;
    auto *slice = m_series->append(
        QStringLiteral("%1 %2 (%3%)")
            .arg(item.name)
            .arg(item.count)
            .arg(percentage, 0, 'f', 1),
        item.count);
    slice->setBrush(item.color);
    slice->setBorderColor(QColor(QStringLiteral("#ffffff")));
    slice->setBorderWidth(2);
    slice->setLabelColor(QColor(QStringLiteral("#314542")));
    slice->setLabelVisible(total > 0 &&
                           static_cast<double>(item.count) / total >= 0.08);
  }
  if (m_series->count() == 0) {
    auto *slice = m_series->append(QStringLiteral("暂无数据"), 1);
    slice->setBrush(QColor(QStringLiteral("#cbd5e1")));
  }
  m_summaryLabel->setText(QStringLiteral("共 %1 个 · 使用中 %2 · 异常 %3")
                              .arg(total)
                              .arg(charging + reserved)
                              .arg(fault + offline));
}

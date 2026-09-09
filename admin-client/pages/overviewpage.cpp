#include "pages/overviewpage.h"

#include "charts/pilestatuschartwidget.h"
#include "charts/revenuechartwidget.h"
#include "widgets/adminuihelpers.h"

#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QLabel>
#include <QScrollArea>
#include <QStyle>
#include <QVBoxLayout>

namespace {

QString metricMoney(const QJsonObject &metrics, const QString &formattedKey,
                    const QString &centsKey, const QString &fenKey) {
  const QString formatted = metrics.value(formattedKey).toString();
  if (!formatted.isEmpty())
    return formatted;
  const QString amountKey = metrics.contains(centsKey) ? centsKey : fenKey;
  return AdminUi::money(
      static_cast<qint64>(metrics.value(amountKey).toDouble()));
}

QString metricCount(const QJsonObject &metrics, const QString &key) {
  return QStringLiteral("%1 单").arg(qMax(0, metrics.value(key).toInt()));
}

} // namespace

OverviewPage::OverviewPage(QWidget *parent) : QWidget(parent) {
  setObjectName(QStringLiteral("overviewPage"));
  auto *root = new QVBoxLayout(this);
  root->setContentsMargins(0, 0, 0, 0);
  auto *scroll = new QScrollArea(this);
  scroll->setObjectName(QStringLiteral("overviewScroll"));
  scroll->setWidgetResizable(true);
  scroll->setFrameShape(QFrame::NoFrame);
  AdminUi::enableTouchScrolling(scroll);
  auto *content = new QWidget(scroll);
  auto *layout = new QVBoxLayout(content);
  layout->setContentsMargins(0, 0, 4, 8);
  layout->setSpacing(14);

  auto *heading = new QLabel(QStringLiteral("运营概览"), content);
  heading->setObjectName(QStringLiteral("pageTitle"));
  layout->addWidget(heading);

  auto *metricsTitle = new QLabel(QStringLiteral("关键指标"), content);
  metricsTitle->setObjectName(QStringLiteral("sectionTitle"));
  layout->addWidget(metricsTitle);
  auto *metrics = new QGridLayout;
  metrics->setHorizontalSpacing(12);
  metrics->setVerticalSpacing(12);
  m_todayRevenue = createMetricCard(
      QStringLiteral("今日营收"), QStringLiteral("todayRevenueValue"), content);
  m_monthRevenue = createMetricCard(
      QStringLiteral("本月营收"), QStringLiteral("monthRevenueValue"), content);
  m_totalRevenue = createMetricCard(
      QStringLiteral("累计营收"), QStringLiteral("totalRevenueValue"), content);
  m_todayOrders = createMetricCard(QStringLiteral("今日订单"),
                                   QStringLiteral("todayOrderValue"), content);
  m_monthOrders = createMetricCard(QStringLiteral("本月订单"),
                                   QStringLiteral("monthOrderValue"), content);
  m_totalOrders = createMetricCard(QStringLiteral("累计订单"),
                                   QStringLiteral("totalOrderValue"), content);
  metrics->addWidget(m_todayRevenue->parentWidget(), 0, 0);
  metrics->addWidget(m_monthRevenue->parentWidget(), 0, 1);
  metrics->addWidget(m_totalRevenue->parentWidget(), 0, 2);
  metrics->addWidget(m_todayOrders->parentWidget(), 0, 3);
  metrics->addWidget(m_monthOrders->parentWidget(), 0, 4);
  metrics->addWidget(m_totalOrders->parentWidget(), 0, 5);
  for (int column = 0; column < 6; ++column)
    metrics->setColumnStretch(column, 1);
  layout->addLayout(metrics);

  auto *chartRow = new QHBoxLayout;
  chartRow->setSpacing(12);
  m_revenueChart = new RevenueChartWidget(content);
  m_revenueChart->setMinimumWidth(480);
  connect(m_revenueChart, &RevenueChartWidget::rangeChanged, this,
          [this](int days) {
            emit commandRequested(QStringLiteral("report.revenue"),
                                  {{QStringLiteral("days"), days}});
          });
  m_pileStatusChart = new PileStatusChartWidget(content);
  m_pileStatusChart->setMinimumWidth(280);
  chartRow->addWidget(m_revenueChart, 2);
  chartRow->addWidget(m_pileStatusChart, 1);
  layout->addLayout(chartRow, 1);

  m_stateLabel = new QLabel(QStringLiteral("等待运营数据"), content);
  m_stateLabel->setObjectName(QStringLiteral("overviewStateLabel"));
  m_stateLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
  m_stateLabel->setWordWrap(true);
  layout->addWidget(m_stateLabel);
  scroll->setWidget(content);
  root->addWidget(scroll);
}

int OverviewPage::revenueDays() const { return m_revenueChart->days(); }

void OverviewPage::requestRefresh() {
  emit commandRequested(QStringLiteral("report.summary"), {});
  emit commandRequested(QStringLiteral("report.revenue"),
                        {{QStringLiteral("days"), revenueDays()}});
  emit commandRequested(QStringLiteral("report.pileStates"), {});
}

void OverviewPage::setSummary(const QJsonObject &payload) {
  const QJsonObject data = AdminUi::dataObject(payload);
  QJsonObject revenue = data.value(QStringLiteral("revenueMetrics")).toObject();
  QJsonObject orders = data.value(QStringLiteral("orderMetrics")).toObject();
  if (revenue.isEmpty())
    revenue = data.value(QStringLiteral("metrics")).toObject();
  if (revenue.isEmpty())
    revenue = data;
  if (orders.isEmpty())
    orders = data.value(QStringLiteral("metrics")).toObject();
  if (orders.isEmpty())
    orders = data;

  const QJsonArray points = data.value(QStringLiteral("points")).toArray();
  if (!points.isEmpty())
    m_revenueChart->setRevenueData(points);

  m_todayRevenue->setText(metricMoney(revenue, QStringLiteral("todayRevenue"),
                                      QStringLiteral("todayRevenueCents"),
                                      QStringLiteral("todayRevenueFen")));
  m_monthRevenue->setText(metricMoney(revenue, QStringLiteral("monthRevenue"),
                                      QStringLiteral("monthRevenueCents"),
                                      QStringLiteral("monthRevenueFen")));
  m_totalRevenue->setText(metricMoney(revenue, QStringLiteral("totalRevenue"),
                                      QStringLiteral("totalRevenueCents"),
                                      QStringLiteral("totalRevenueFen")));
  m_todayOrders->setText(
      metricCount(orders, QStringLiteral("todayOrderCount")));
  m_monthOrders->setText(
      metricCount(orders, QStringLiteral("monthOrderCount")));
  m_totalOrders->setText(
      metricCount(orders, QStringLiteral("totalOrderCount")));
  const QJsonObject pileStatus = data.value(QStringLiteral("pileStatus")).toObject();
  if (!pileStatus.isEmpty())
    m_pileStatusChart->setStatusData(pileStatus);
  setState(QStringLiteral("更新于 %1")
               .arg(data.value(QStringLiteral("updatedAt"))
                        .toString(QStringLiteral("刚刚"))),
           QStringLiteral("ready"));
}

void OverviewPage::setRevenue(const QJsonObject &payload) {
  const QJsonObject data = AdminUi::dataObject(payload);
  QJsonArray points = data.value(QStringLiteral("points")).toArray();
  if (points.isEmpty())
    points = payload.value(QStringLiteral("points")).toArray();
  m_revenueChart->setRevenueData(points);
}

void OverviewPage::setPileStatus(const QJsonObject &payload) {
  m_pileStatusChart->setStatusData(AdminUi::dataObject(payload));
}

void OverviewPage::setLoading(const QString &action, bool loading) {
  if (loading && action.startsWith(QStringLiteral("report."))) {
    setState(QStringLiteral("正在刷新运营数据…"), QStringLiteral("loading"));
  }
}

void OverviewPage::setError(const QString &message) {
  setState(message, QStringLiteral("error"));
}

QLabel *OverviewPage::createMetricCard(const QString &title,
                                       const QString &objectName,
                                       QWidget *parent) {
  auto *card = new QFrame(parent);
  card->setObjectName(QStringLiteral("metricCard"));
  card->setMinimumHeight(96);
  auto *layout = new QVBoxLayout(card);
  layout->setContentsMargins(15, 13, 15, 13);
  layout->setSpacing(7);
  auto *caption = new QLabel(title, card);
  caption->setObjectName(QStringLiteral("metricCaption"));
  auto *value = new QLabel(QStringLiteral("--"), card);
  value->setObjectName(objectName);
  value->setProperty("class", QStringLiteral("metricValue"));
  value->setProperty("metricKind",
                     title.contains(QStringLiteral("营收"))
                         ? QStringLiteral("revenue")
                         : QStringLiteral("orders"));
  value->setWordWrap(true);
  layout->addWidget(caption);
  layout->addWidget(value);
  return value;
}

void OverviewPage::setState(const QString &text, const QString &state) {
  m_stateLabel->setText(text);
  m_stateLabel->setProperty("state", state);
  m_stateLabel->style()->unpolish(m_stateLabel);
  m_stateLabel->style()->polish(m_stateLabel);
}

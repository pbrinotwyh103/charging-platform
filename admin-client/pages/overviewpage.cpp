#include "pages/overviewpage.h"

#include "charts/pilestatuschartwidget.h"
#include "charts/revenuechartwidget.h"
#include "widgets/adminuihelpers.h"

#include <QFrame>
#include <QGridLayout>
#include <QJsonArray>
#include <QLabel>
#include <QScrollArea>
#include <QStyle>
#include <QVBoxLayout>

namespace {

QString metricMoney(const QJsonObject &metrics, const QString &formattedKey,
                    const QString &centsKey) {
  const QString formatted = metrics.value(formattedKey).toString();
  if (!formatted.isEmpty())
    return formatted;
  return AdminUi::money(
      static_cast<qint64>(metrics.value(centsKey).toDouble()));
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
  layout->setContentsMargins(12, 12, 12, 18);
  layout->setSpacing(12);

  auto *heading = new QLabel(QStringLiteral("运营概览"), content);
  heading->setObjectName(QStringLiteral("pageTitle"));
  layout->addWidget(heading);

  auto *metrics = new QGridLayout;
  metrics->setHorizontalSpacing(8);
  metrics->setVerticalSpacing(8);
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
  metrics->addWidget(m_totalRevenue->parentWidget(), 1, 0);
  metrics->addWidget(m_todayOrders->parentWidget(), 1, 1);
  metrics->addWidget(m_monthOrders->parentWidget(), 2, 0);
  metrics->addWidget(m_totalOrders->parentWidget(), 2, 1);
  layout->addLayout(metrics);

  m_revenueChart = new RevenueChartWidget(content);
  connect(m_revenueChart, &RevenueChartWidget::rangeChanged, this,
          [this](int days) {
            emit commandRequested(QStringLiteral("dashboard.revenue"),
                                  {{QStringLiteral("days"), days}});
          });
  layout->addWidget(m_revenueChart);
  m_pileStatusChart = new PileStatusChartWidget(content);
  layout->addWidget(m_pileStatusChart);

  m_stateLabel = new QLabel(QStringLiteral("等待运营数据"), content);
  m_stateLabel->setObjectName(QStringLiteral("overviewStateLabel"));
  m_stateLabel->setAlignment(Qt::AlignCenter);
  m_stateLabel->setWordWrap(true);
  layout->addWidget(m_stateLabel);
  layout->addStretch();
  scroll->setWidget(content);
  root->addWidget(scroll);
}

int OverviewPage::revenueDays() const { return m_revenueChart->days(); }

void OverviewPage::requestRefresh() {
  emit commandRequested(QStringLiteral("dashboard.summary"), {});
  emit commandRequested(QStringLiteral("dashboard.revenue"),
                        {{QStringLiteral("days"), revenueDays()}});
}

void OverviewPage::setSummary(const QJsonObject &payload) {
  const QJsonObject data = AdminUi::dataObject(payload);
  QJsonObject revenue = data.value(QStringLiteral("revenueMetrics")).toObject();
  QJsonObject orders = data.value(QStringLiteral("orderMetrics")).toObject();
  if (revenue.isEmpty())
    revenue = data.value(QStringLiteral("metrics")).toObject();
  if (orders.isEmpty())
    orders = revenue;

  m_todayRevenue->setText(metricMoney(revenue, QStringLiteral("todayRevenue"),
                                      QStringLiteral("todayRevenueCents")));
  m_monthRevenue->setText(metricMoney(revenue, QStringLiteral("monthRevenue"),
                                      QStringLiteral("monthRevenueCents")));
  m_totalRevenue->setText(metricMoney(revenue, QStringLiteral("totalRevenue"),
                                      QStringLiteral("totalRevenueCents")));
  m_todayOrders->setText(
      metricCount(orders, QStringLiteral("todayOrderCount")));
  m_monthOrders->setText(
      metricCount(orders, QStringLiteral("monthOrderCount")));
  m_totalOrders->setText(
      metricCount(orders, QStringLiteral("totalOrderCount")));
  m_pileStatusChart->setStatusData(
      data.value(QStringLiteral("pileStatus")).toObject());
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

void OverviewPage::setLoading(const QString &action, bool loading) {
  if (loading && action.startsWith(QStringLiteral("dashboard."))) {
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
  card->setMinimumHeight(82);
  auto *layout = new QVBoxLayout(card);
  layout->setContentsMargins(11, 9, 11, 9);
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

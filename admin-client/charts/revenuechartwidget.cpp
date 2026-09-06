#include "charts/revenuechartwidget.h"

#include <QtCharts/QChart>
#include <QtCharts/QChartView>
#include <QtCharts/QDateTimeAxis>
#include <QtCharts/QLegend>
#include <QtCharts/QLineSeries>
#include <QtCharts/QValueAxis>

#include <QDate>
#include <QDateTime>
#include <QFrame>
#include <QHBoxLayout>
#include <QJsonObject>
#include <QLabel>
#include <QPainter>
#include <QPen>
#include <QPushButton>
#include <QTime>
#include <QVBoxLayout>

namespace {

QDateTime pointTime(const QJsonObject &point) {
  const QJsonValue timestamp = point.value(QStringLiteral("timestamp"));
  if (timestamp.isDouble()) {
    return QDateTime::fromMSecsSinceEpoch(
        static_cast<qint64>(timestamp.toDouble()));
  }
  QString text = timestamp.toString();
  if (text.isEmpty())
    text = point.value(QStringLiteral("date")).toString();
  QDateTime value = QDateTime::fromString(text, Qt::ISODateWithMs);
  if (!value.isValid())
    value = QDateTime::fromString(text, Qt::ISODate);
  if (!value.isValid()) {
    const QDate date = QDate::fromString(text.left(10), Qt::ISODate);
    if (date.isValid())
      value = QDateTime(date, QTime(0, 0));
  }
  return value;
}

double pointRevenue(const QJsonObject &point) {
  if (point.contains(QStringLiteral("revenueCents"))) {
    return point.value(QStringLiteral("revenueCents")).toDouble() / 100.0;
  }
  return point.value(QStringLiteral("revenue")).toDouble();
}

} // namespace

RevenueChartWidget::RevenueChartWidget(QWidget *parent) : QWidget(parent) {
  setObjectName(QStringLiteral("revenueChartWidget"));
  auto *root = new QVBoxLayout(this);
  root->setContentsMargins(0, 0, 0, 0);
  root->setSpacing(6);

  auto *toolbar = new QHBoxLayout;
  toolbar->setSpacing(0);
  auto *title = new QLabel(QStringLiteral("营收趋势"), this);
  title->setObjectName(QStringLiteral("sectionTitle"));
  toolbar->addWidget(title);
  toolbar->addStretch();
  m_sevenDaysButton = new QPushButton(QStringLiteral("近7日"), this);
  m_sevenDaysButton->setObjectName(QStringLiteral("sevenDaysButton"));
  m_sevenDaysButton->setCheckable(true);
  m_thirtyDaysButton = new QPushButton(QStringLiteral("近30日"), this);
  m_thirtyDaysButton->setObjectName(QStringLiteral("thirtyDaysButton"));
  m_thirtyDaysButton->setCheckable(true);
  toolbar->addWidget(m_sevenDaysButton);
  toolbar->addWidget(m_thirtyDaysButton);
  root->addLayout(toolbar);

  m_series = new QLineSeries(this);
  m_series->setName(QStringLiteral("营收"));
  QPen seriesPen(QColor(QStringLiteral("#0f766e")));
  seriesPen.setWidthF(2.6);
  seriesPen.setCapStyle(Qt::RoundCap);
  seriesPen.setJoinStyle(Qt::RoundJoin);
  m_series->setPen(seriesPen);
  m_series->setPointsVisible(true);

  auto *chart = new QChart;
  chart->setBackgroundVisible(false);
  chart->legend()->hide();
  chart->setMargins(QMargins(0, 4, 0, 0));
  chart->addSeries(m_series);

  m_axisX = new QDateTimeAxis(chart);
  m_axisX->setFormat(QStringLiteral("MM-dd"));
  m_axisX->setTickCount(7);
  m_axisX->setLabelsAngle(-35);
  m_axisX->setLabelsColor(QColor(QStringLiteral("#6b7c78")));
  m_axisX->setGridLineColor(QColor(QStringLiteral("#e5ebe9")));
  m_axisX->setLinePenColor(QColor(QStringLiteral("#c9d5d2")));
  chart->addAxis(m_axisX, Qt::AlignBottom);
  m_series->attachAxis(m_axisX);

  m_axisY = new QValueAxis(chart);
  m_axisY->setLabelFormat(QStringLiteral("%.0f"));
  m_axisY->setTitleText(QStringLiteral("元"));
  m_axisY->setRange(0.0, 10.0);
  m_axisY->setLabelsColor(QColor(QStringLiteral("#6b7c78")));
  m_axisY->setGridLineColor(QColor(QStringLiteral("#e5ebe9")));
  m_axisY->setLinePenColor(QColor(QStringLiteral("#c9d5d2")));
  chart->addAxis(m_axisY, Qt::AlignLeft);
  m_series->attachAxis(m_axisY);

  auto *view = new QChartView(chart, this);
  view->setObjectName(QStringLiteral("revenueChartView"));
  view->setRenderHint(QPainter::Antialiasing);
  view->setFrameShape(QFrame::NoFrame);
  view->setBackgroundBrush(Qt::transparent);
  view->setMinimumHeight(210);
  root->addWidget(view);

  connect(m_sevenDaysButton, &QPushButton::clicked, this,
          [this] { selectRange(7, true); });
  connect(m_thirtyDaysButton, &QPushButton::clicked, this,
          [this] { selectRange(30, true); });
  selectRange(7, false);
  setRevenueData({});
}

int RevenueChartWidget::days() const { return m_days; }

int RevenueChartWidget::pointCount() const { return m_series->count(); }

void RevenueChartWidget::setRevenueData(const QJsonArray &points) {
  m_series->clear();
  qint64 minimumTime = 0;
  qint64 maximumTime = 0;
  double maximumRevenue = 0.0;
  for (const QJsonValue &value : points) {
    const QJsonObject point = value.toObject();
    const QDateTime dateTime = pointTime(point);
    if (!dateTime.isValid())
      continue;
    const qint64 timestamp = dateTime.toMSecsSinceEpoch();
    const double revenue = qMax(0.0, pointRevenue(point));
    m_series->append(static_cast<qreal>(timestamp), revenue);
    minimumTime = minimumTime == 0 ? timestamp : qMin(minimumTime, timestamp);
    maximumTime = qMax(maximumTime, timestamp);
    maximumRevenue = qMax(maximumRevenue, revenue);
  }
  m_series->setPointsVisible(m_series->count() <= 10);

  if (m_series->count() == 0) {
    const QDateTime end = QDateTime::currentDateTime();
    m_axisX->setRange(end.addDays(-(m_days - 1)), end);
    m_axisY->setRange(0.0, 10.0);
    return;
  }
  if (minimumTime == maximumTime) {
    minimumTime -= 12 * 60 * 60 * 1000;
    maximumTime += 12 * 60 * 60 * 1000;
  }
  m_axisX->setRange(QDateTime::fromMSecsSinceEpoch(minimumTime),
                    QDateTime::fromMSecsSinceEpoch(maximumTime));
  m_axisX->setTickCount(m_days == 30 ? 8 : qMax(2, qMin(7, m_series->count())));
  m_axisY->setRange(0.0, maximumRevenue > 0.0 ? maximumRevenue * 1.15 : 10.0);
}

void RevenueChartWidget::selectRange(int days, bool notify) {
  if (days != 7 && days != 30)
    return;
  const bool changed = m_days != days;
  m_days = days;
  m_sevenDaysButton->setChecked(days == 7);
  m_thirtyDaysButton->setChecked(days == 30);
  if (notify && changed)
    emit rangeChanged(days);
}

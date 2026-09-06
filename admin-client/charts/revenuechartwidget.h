#pragma once

#include <QJsonArray>
#include <QWidget>

class QDateTimeAxis;
class QLineSeries;
class QPushButton;
class QValueAxis;

class RevenueChartWidget final : public QWidget {
  Q_OBJECT

public:
  explicit RevenueChartWidget(QWidget *parent = nullptr);

  int days() const;
  int pointCount() const;

public slots:
  void setRevenueData(const QJsonArray &points);

signals:
  void rangeChanged(int days);

private:
  void selectRange(int days, bool notify);

  int m_days = 7;
  QLineSeries *m_series = nullptr;
  QDateTimeAxis *m_axisX = nullptr;
  QValueAxis *m_axisY = nullptr;
  QPushButton *m_sevenDaysButton = nullptr;
  QPushButton *m_thirtyDaysButton = nullptr;
};

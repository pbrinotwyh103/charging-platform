#pragma once

#include <QJsonObject>
#include <QWidget>

class QLabel;
class QPieSeries;

class PileStatusChartWidget final : public QWidget {
  Q_OBJECT

public:
  explicit PileStatusChartWidget(QWidget *parent = nullptr);

public slots:
  void setStatusData(const QJsonObject &status);

private:
  QPieSeries *m_series = nullptr;
  QLabel *m_summaryLabel = nullptr;
};

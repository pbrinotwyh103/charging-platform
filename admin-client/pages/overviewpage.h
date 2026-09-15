#pragma once

#include <QHash>
#include <QJsonObject>
#include <QWidget>

class QLabel;
class QComboBox;
class PileStatusChartWidget;
class RevenueChartWidget;

class OverviewPage final : public QWidget {
  Q_OBJECT

public:
  explicit OverviewPage(QWidget *parent = nullptr);

  int revenueDays() const;

public slots:
  void requestRefresh();
  void setSummary(const QJsonObject &payload);
  void setRevenue(const QJsonObject &payload);
  void setPileStatus(const QJsonObject &payload);
  void setLoadWarnings(const QJsonObject &payload);
  void setAnalyticsStatus(const QJsonObject &payload);
  void setLoading(const QString &action, bool loading);
  void setError(const QString &message);

signals:
  void commandRequested(const QString &action, const QJsonObject &parameters);

private:
  QLabel *createMetricCard(const QString &title, const QString &objectName,
                           QWidget *parent);
  void setState(const QString &text, const QString &state);
  void renderWarnings();

  QLabel *m_todayRevenue = nullptr;
  QLabel *m_monthRevenue = nullptr;
  QLabel *m_totalRevenue = nullptr;
  QLabel *m_todayOrders = nullptr;
  QLabel *m_monthOrders = nullptr;
  QLabel *m_totalOrders = nullptr;
  QLabel *m_stateLabel = nullptr;
  QLabel *m_loadWarningSummary = nullptr;
  QLabel *m_analyticsStatus = nullptr;
  QComboBox *m_warningHorizon = nullptr;
  QComboBox *m_warningLevel = nullptr;
  QComboBox *m_warningRegion = nullptr;
  QHash<QString, QJsonObject> m_warningItems;
  RevenueChartWidget *m_revenueChart = nullptr;
  PileStatusChartWidget *m_pileStatusChart = nullptr;
};

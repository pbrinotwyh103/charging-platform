#pragma once

#include <QJsonObject>
#include <QWidget>

class QLabel;
class PaginationBar;
class QPushButton;
class QResizeEvent;
class QTableWidget;

class MonitorPage final : public QWidget {
  Q_OBJECT

public:
  explicit MonitorPage(QWidget *parent = nullptr);

public slots:
  void requestRefresh();
  void setChargingData(const QJsonObject &payload);
  void applyProgressPush(const QJsonObject &payload);
  void applyStoppedPush(const QJsonObject &payload);
  void setLoading(const QString &action, bool loading);
  void setError(const QString &message);

signals:
  void commandRequested(const QString &action, const QJsonObject &parameters);

protected:
  void resizeEvent(QResizeEvent *event) override;

private:
  void requestPage(int page);
  void updateSelection();
  void updateResponsiveLayout();
  int rowForOrder(qint64 orderId) const;

  QTableWidget *m_table = nullptr;
  QLabel *m_stateLabel = nullptr;
  PaginationBar *m_pagination = nullptr;
  QPushButton *m_stopButton = nullptr;
};

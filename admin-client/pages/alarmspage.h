#pragma once

#include <QJsonObject>
#include <QWidget>

class QComboBox;
class QGroupBox;
class QLabel;
class PaginationBar;
class QResizeEvent;
class QTableWidget;

class AlarmsPage final : public QWidget {
  Q_OBJECT

public:
  explicit AlarmsPage(QWidget *parent = nullptr);

public slots:
  void requestRefresh();
  void setAlarms(const QJsonObject &payload);
  void setAlarmDetail(const QJsonObject &payload);
  void applyAlarmPush(const QJsonObject &payload);
  void setLoading(const QString &action, bool loading);
  void setError(const QString &message);

signals:
  void commandRequested(const QString &action, const QJsonObject &parameters);

protected:
  void resizeEvent(QResizeEvent *event) override;

private:
  void requestPage(int page);
  void showDetail(const QJsonObject &alarm);
  void updateResponsiveLayout();

  QComboBox *m_severity = nullptr;
  QComboBox *m_status = nullptr;
  QTableWidget *m_table = nullptr;
  QLabel *m_stateLabel = nullptr;
  QGroupBox *m_detailBox = nullptr;
  QLabel *m_detailLabel = nullptr;
  PaginationBar *m_pagination = nullptr;
};

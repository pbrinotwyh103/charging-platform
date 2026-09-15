#pragma once

#include <QJsonObject>
#include <QWidget>

class QLabel;
class QTabWidget;

class AdvicePage final : public QWidget {
  Q_OBJECT
public:
  explicit AdvicePage(QWidget *parent = nullptr);
public slots:
  void requestRefresh();
  void setReport(const QString &action, const QJsonObject &payload);
  void setLoading(const QString &action, bool loading);
  void setError(const QString &message);
signals:
  void commandRequested(const QString &action, const QJsonObject &parameters);
private:
  QLabel *labelFor(const QString &action) const;
  QTabWidget *m_tabs = nullptr;
  QLabel *m_model = nullptr;
  QLabel *m_drift = nullptr;
  QLabel *m_scheduling = nullptr;
  QLabel *m_maintenance = nullptr;
  QLabel *m_expansion = nullptr;
  QLabel *m_regulator = nullptr;
  QLabel *m_state = nullptr;
};

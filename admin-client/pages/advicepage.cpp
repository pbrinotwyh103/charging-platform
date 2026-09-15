#include "pages/advicepage.h"

#include <QJsonDocument>
#include <QJsonArray>
#include <QLabel>
#include <QTabWidget>
#include <QVBoxLayout>

AdvicePage::AdvicePage(QWidget *parent) : QWidget(parent) {
  setObjectName(QStringLiteral("advicePage"));
  auto *layout = new QVBoxLayout(this);
  auto *title = new QLabel(QStringLiteral("智能决策支持"), this);
  title->setObjectName(QStringLiteral("pageTitle"));
  layout->addWidget(title);
  auto *notice = new QLabel(QStringLiteral("以下建议仅供人工决策，不会自动控制电桩、停止充电或执行扩容。"), this);
  notice->setObjectName(QStringLiteral("adviceReadOnlyNotice")); notice->setWordWrap(true);
  layout->addWidget(notice);
  m_tabs = new QTabWidget(this); m_tabs->setObjectName(QStringLiteral("adviceTabs"));
  auto add = [this](const QString &title, QLabel **label, const QString &name) {
    auto *page = new QWidget(m_tabs); auto *box = new QVBoxLayout(page);
    *label = new QLabel(QStringLiteral("暂无数据"), page); (*label)->setObjectName(name); (*label)->setWordWrap(true);
    (*label)->setTextInteractionFlags(Qt::TextSelectableByMouse); box->addWidget(*label); box->addStretch();
    m_tabs->addTab(page, title);
  };
  add(QStringLiteral("模型对比"), &m_model, QStringLiteral("modelComparisonReport"));
  add(QStringLiteral("漂移"), &m_drift, QStringLiteral("driftReport"));
  add(QStringLiteral("调度"), &m_scheduling, QStringLiteral("schedulingAdvice"));
  add(QStringLiteral("维护"), &m_maintenance, QStringLiteral("maintenanceAdvice"));
  add(QStringLiteral("扩容"), &m_expansion, QStringLiteral("expansionAdvice"));
  add(QStringLiteral("监管"), &m_regulator, QStringLiteral("regulatorReport"));
  layout->addWidget(m_tabs, 1);
  m_state = new QLabel(QStringLiteral("等待分析数据"), this); m_state->setObjectName(QStringLiteral("adviceState"));
  layout->addWidget(m_state);
}

void AdvicePage::requestRefresh() {
  for (const QString &action : {"analytics.modelComparison", "analytics.drift", "analytics.scheduling",
                                "analytics.maintenance", "analytics.expansion", "analytics.regulator"})
    emit commandRequested(action, {});
}

QLabel *AdvicePage::labelFor(const QString &action) const {
  if (action.endsWith("modelComparison")) return m_model;
  if (action.endsWith("drift")) return m_drift;
  if (action.endsWith("scheduling")) return m_scheduling;
  if (action.endsWith("maintenance")) return m_maintenance;
  if (action.endsWith("expansion")) return m_expansion;
  if (action.endsWith("regulator")) return m_regulator;
  return nullptr;
}

void AdvicePage::setReport(const QString &action, const QJsonObject &payload) {
  auto *label = labelFor(action); if (!label) return;
  const QJsonValue data = payload.value("data");
  label->setText(QString::fromUtf8(QJsonDocument(data.isArray() ? QJsonDocument(data.toArray())
                                                               : QJsonDocument(data.toObject())).toJson(QJsonDocument::Indented)));
  m_state->setText(payload.value("meta").toObject().value("stale").toBool()
      ? QStringLiteral("数据已过期，仅供参考") : QStringLiteral("分析数据已更新"));
}
void AdvicePage::setLoading(const QString &action, bool loading) { if (loading && action.startsWith("analytics.")) m_state->setText(QStringLiteral("正在加载分析建议…")); }
void AdvicePage::setError(const QString &message) { m_state->setText(message); }

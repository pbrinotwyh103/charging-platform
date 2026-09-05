#pragma once

#include <QAbstractItemView>
#include <QAbstractScrollArea>
#include <QColor>
#include <QFont>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QScroller>
#include <QString>
#include <QTableWidget>

namespace AdminUi {

enum class Tone { Neutral, Success, Info, Warning, Danger };

inline QJsonObject dataObject(const QJsonObject &payload) {
  const QJsonObject data = payload.value(QStringLiteral("data")).toObject();
  return data.isEmpty() ? payload : data;
}

inline QJsonArray itemArray(const QJsonObject &payload) {
  const QJsonObject data = dataObject(payload);
  QJsonArray items = data.value(QStringLiteral("items")).toArray();
  if (items.isEmpty() && payload.value(QStringLiteral("data")).isArray()) {
    items = payload.value(QStringLiteral("data")).toArray();
  }
  return items;
}

inline QJsonObject metaObject(const QJsonObject &payload) {
  const QJsonObject data = dataObject(payload);
  const QJsonObject meta = data.value(QStringLiteral("meta")).toObject();
  return meta.isEmpty() ? payload.value(QStringLiteral("meta")).toObject()
                        : meta;
}

inline qint64 integerId(const QJsonObject &object, const QString &key) {
  return static_cast<qint64>(object.value(key).toDouble());
}

inline QString money(qint64 cents) {
  return QStringLiteral("¥%1").arg(cents / 100.0, 0, 'f', 2);
}

inline QString duration(int seconds) {
  seconds = qMax(0, seconds);
  return QStringLiteral("%1:%2:%3")
      .arg(seconds / 3600, 2, 10, QLatin1Char('0'))
      .arg((seconds / 60) % 60, 2, 10, QLatin1Char('0'))
      .arg(seconds % 60, 2, 10, QLatin1Char('0'));
}

inline QString pileStatus(const QString &status) {
  if (status == QStringLiteral("idle"))
    return QStringLiteral("空闲");
  if (status == QStringLiteral("reserved"))
    return QStringLiteral("已预约");
  if (status == QStringLiteral("charging"))
    return QStringLiteral("充电中");
  if (status == QStringLiteral("fault"))
    return QStringLiteral("故障");
  if (status == QStringLiteral("offline"))
    return QStringLiteral("离线");
  if (status == QStringLiteral("disabled"))
    return QStringLiteral("已停用");
  return status.isEmpty() ? QStringLiteral("未知") : status;
}

inline QString userStatus(const QString &status) {
  return status == QStringLiteral("frozen") ? QStringLiteral("已冻结")
                                            : QStringLiteral("正常");
}

inline QString orderStatus(const QString &status) {
  if (status == QStringLiteral("charging"))
    return QStringLiteral("充电中");
  if (status == QStringLiteral("completed"))
    return QStringLiteral("已完成");
  if (status == QStringLiteral("fault_stopped"))
    return QStringLiteral("异常结束");
  if (status == QStringLiteral("cancelled"))
    return QStringLiteral("已取消");
  return status.isEmpty() ? QStringLiteral("未知") : status;
}

inline QString alarmSeverity(const QString &severity) {
  if (severity == QStringLiteral("critical"))
    return QStringLiteral("严重");
  if (severity == QStringLiteral("warning"))
    return QStringLiteral("警告");
  return QStringLiteral("提示");
}

inline QString alarmStatus(const QString &status) {
  if (status == QStringLiteral("acknowledged"))
    return QStringLiteral("已确认");
  if (status == QStringLiteral("resolved"))
    return QStringLiteral("已恢复");
  return QStringLiteral("待处理");
}

inline QString csvCell(QString value) {
  value.replace(QLatin1Char('"'), QStringLiteral("\"\""));
  if (value.contains(QLatin1Char(',')) || value.contains(QLatin1Char('"')) ||
      value.contains(QLatin1Char('\n')) || value.contains(QLatin1Char('\r'))) {
    return QStringLiteral("\"%1\"").arg(value);
  }
  return value;
}

inline void styleStateItem(QTableWidgetItem *item, Tone tone) {
  if (!item)
    return;

  QColor foreground(QStringLiteral("#4b5563"));
  QColor background(QStringLiteral("#f3f4f6"));
  switch (tone) {
  case Tone::Success:
    foreground = QColor(QStringLiteral("#047857"));
    background = QColor(QStringLiteral("#ecfdf5"));
    break;
  case Tone::Info:
    foreground = QColor(QStringLiteral("#1d4ed8"));
    background = QColor(QStringLiteral("#eff6ff"));
    break;
  case Tone::Warning:
    foreground = QColor(QStringLiteral("#b45309"));
    background = QColor(QStringLiteral("#fffbeb"));
    break;
  case Tone::Danger:
    foreground = QColor(QStringLiteral("#b91c1c"));
    background = QColor(QStringLiteral("#fef2f2"));
    break;
  case Tone::Neutral:
    break;
  }
  item->setForeground(foreground);
  item->setBackground(background);
  item->setTextAlignment(Qt::AlignCenter);
  QFont font = item->font();
  font.setBold(true);
  item->setFont(font);
}

inline Tone pileTone(const QString &status) {
  if (status == QStringLiteral("idle"))
    return Tone::Success;
  if (status == QStringLiteral("charging"))
    return Tone::Info;
  if (status == QStringLiteral("reserved"))
    return Tone::Warning;
  if (status == QStringLiteral("fault"))
    return Tone::Danger;
  return Tone::Neutral;
}

inline void enableTouchScrolling(QAbstractScrollArea *area) {
  if (!area)
    return;
  area->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  area->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  QScroller::grabGesture(area->viewport(), QScroller::TouchGesture);
}

inline void configureTouchTable(QTableWidget *table) {
  if (!table)
    return;
  enableTouchScrolling(table);
  table->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
  table->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
  table->setCornerButtonEnabled(false);
  table->setFocusPolicy(Qt::NoFocus);
  table->setShowGrid(false);
  table->setTextElideMode(Qt::ElideRight);
  table->setWordWrap(false);
  table->horizontalHeader()->setHighlightSections(false);
  table->horizontalHeader()->setMinimumSectionSize(72);
  table->verticalHeader()->setDefaultSectionSize(54);
  table->verticalHeader()->setMinimumSectionSize(54);
}

inline void setResponsiveColumns(QTableWidget *table,
                                 const QList<int> &compactColumns,
                                 bool compact) {
  if (!table)
    return;
  for (int column = 0; column < table->columnCount(); ++column) {
    table->setColumnHidden(column,
                           compact && !compactColumns.contains(column));
  }
  table->setHorizontalScrollBarPolicy(compact ? Qt::ScrollBarAlwaysOff
                                               : Qt::ScrollBarAsNeeded);
  table->horizontalHeader()->setStretchLastSection(!compact);
  table->horizontalHeader()->setSectionResizeMode(
      compact ? QHeaderView::Stretch : QHeaderView::ResizeToContents);
}

} // namespace AdminUi

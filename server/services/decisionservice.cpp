#include "services/decisionservice.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QJsonObject>
#include <algorithm>

namespace {
QString stableId(const QString &kind, const QJsonObject &row)
{
    const QByteArray key = kind.toUtf8() + ':'
        + QByteArray::number(qint64(row.value("stationId").toDouble())) + ':'
        + row.value("modelVersion").toString().toUtf8();
    return kind + '-' + QCryptographicHash::hash(key, QCryptographicHash::Sha256).toHex().left(12);
}
}

QJsonArray DecisionService::ranked(const QString &kind, const QJsonArray &rows) const
{
    QList<QJsonObject> advice;
    for (const auto &value : rows) {
        if (!value.isObject()) continue;
        auto row = value.toObject();
        const qint64 stationId = qint64(row.value("stationId").toDouble());
        if (stationId <= 0) continue;
        double score = row.value("score").toDouble();
        QStringList reasons;
        if (kind == "scheduling") {
            const int idle = row.value("predictedAvailablePiles").toInt();
            const double sessions = row.value("predictedSessions").toDouble();
            score = sessions * 10.0 - idle * 5.0;
            reasons << QStringLiteral("预测会话 %1").arg(sessions)
                    << QStringLiteral("预计空闲桩 %1").arg(idle);
        } else if (kind == "maintenance") {
            score = row.value("openSevereAlarms").toInt() * 20.0
                + row.value("unavailablePiles").toInt() * 10.0;
            reasons << QStringLiteral("严重未结告警 %1").arg(row.value("openSevereAlarms").toInt())
                    << QStringLiteral("不可用桩 %1").arg(row.value("unavailablePiles").toInt());
        } else {
            score = row.value("sustainedUtilization").toDouble() * 100.0
                + row.value("queuePressure").toDouble() * 50.0;
            reasons << QStringLiteral("持续利用率 %1").arg(row.value("sustainedUtilization").toDouble())
                    << QStringLiteral("排队压力 %1").arg(row.value("queuePressure").toDouble());
        }
        if (score <= 0) continue;
        const QString severity = score >= 80 ? "severe" : score >= 40 ? "attention" : "normal";
        advice.append(QJsonObject{{"adviceId", stableId(kind, row)}, {"kind", kind},
            {"stationId", stationId}, {"severity", severity}, {"score", score},
            {"reasons", QJsonArray::fromStringList(reasons)},
            {"generatedAt", row.value("generatedAt")}, {"modelVersion", row.value("modelVersion")},
            {"stale", row.value("stale").toBool(false)}, {"readOnly", true}});
    }
    std::sort(advice.begin(), advice.end(), [](const QJsonObject &a, const QJsonObject &b) {
        if (a.value("score").toDouble() != b.value("score").toDouble())
            return a.value("score").toDouble() > b.value("score").toDouble();
        return a.value("adviceId").toString() < b.value("adviceId").toString();
    });
    QJsonArray result;
    for (const auto &item : advice) result.append(item);
    return result;
}

QJsonArray DecisionService::scheduling(const QJsonArray &rows) const { return ranked("scheduling", rows); }
QJsonArray DecisionService::maintenance(const QJsonArray &rows) const { return ranked("maintenance", rows); }
QJsonArray DecisionService::expansion(const QJsonArray &rows) const { return ranked("expansion", rows); }

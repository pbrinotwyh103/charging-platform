#include "services/analyticsservice.h"

#include "services/servicehelpers.h"

#include <QEventLoop>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QMutexLocker>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcessEnvironment>
#include <QSet>
#include <QTimer>
#include <QUrlQuery>
#include <algorithm>
#include <cmath>

using Charging::ErrorCode;
using namespace ServiceHelpers;

namespace
{
constexpr int AnalyticsTimeoutMilliseconds = 2000;

bool horizonValue(const QJsonObject &payload, int *horizon)
{
    if (!payload.contains("horizonHours"))
    {
        *horizon = 1;
        return true;
    }
    const auto value = payload.value("horizonHours");
    if (!integer(value, 1, 72))
        return false;
    *horizon = value.toInt();
    return *horizon == 1 || *horizon == 6 || *horizon == 24 || *horizon == 48 || *horizon == 72;
}

QString warningLevel(const QJsonObject &item)
{
    const int available = item.value("predictedAvailablePiles").toInt();
    const int total = item.value("totalPiles").toInt();
    if (available <= 0)
        return QStringLiteral("severe");
    if (total <= 0)
        return item.value("predictedSessions").toDouble() >= 8.0
            ? QStringLiteral("congested") : QStringLiteral("normal");
    const double occupancy = 1.0 - double(available) / total;
    if (occupancy >= 0.85)
        return QStringLiteral("congested");
    if (occupancy >= 0.65)
        return QStringLiteral("attention");
    return QStringLiteral("normal");
}

bool validNumber(const QJsonValue &value, double minimum)
{
    return value.isDouble() && std::isfinite(value.toDouble()) && value.toDouble() >= minimum;
}

bool normalizeItem(const QJsonObject &raw, int requestedHorizon, QJsonObject *item)
{
    if (!identifier(raw.value("stationId"))
        || !integer(raw.value("horizonHours"), 1, 72)
        || raw.value("horizonHours").toInt() != requestedHorizon
        || !validNumber(raw.value("predictedSessions"), 0)
        || !integer(raw.value("predictedAvailablePiles"), 0, 1000000)
        || !integer(raw.value("totalPiles"), 0, 1000000)
        || raw.value("predictedAvailablePiles").toInt() > raw.value("totalPiles").toInt())
        return false;
    for (const auto &key : QStringList{"stationName", "forecastTime", "generatedAt", "modelVersion"})
        if (raw.contains(key) && !raw.value(key).isString())
            return false;
    *item = raw;
    item->insert("warningLevel", warningLevel(raw));
    return true;
}

ServiceResult malformed()
{
    return failure(ErrorCode::InvalidPacket, QStringLiteral("分析服务返回了无效数据"),
                   QStringLiteral("invalid_analytics_payload"));
}
}

AnalyticsService::AnalyticsService(Fetcher fetcher) : m_fetcher(std::move(fetcher)) {}

QUrl AnalyticsService::endpoint(const QString &path) const
{
    QString base = QProcessEnvironment::systemEnvironment().value(
        QStringLiteral("CHARGING_ANALYTICS_URL"), QStringLiteral("http://127.0.0.1:5000"));
    while (base.endsWith('/'))
        base.chop(1);
    return QUrl(base + path);
}

AnalyticsFetchResult AnalyticsService::fetch(const QUrl &url) const
{
    if (m_fetcher)
        return m_fetcher(url);
    QNetworkAccessManager manager;
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("charging-platform-server/2"));
    const auto apiKey = QProcessEnvironment::systemEnvironment().value(QStringLiteral("CHARGING_API_KEY"));
    if (!apiKey.isEmpty()) request.setRawHeader("X-Analytics-Key", apiKey.toUtf8());
    QNetworkReply *reply = manager.get(request);
    QTimer timer;
    timer.setSingleShot(true);
    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    timer.start(AnalyticsTimeoutMilliseconds);
    loop.exec();
    if (!timer.isActive())
    {
        reply->abort();
        reply->deleteLater();
        return {false, 0, {}, QStringLiteral("分析服务请求超时")};
    }
    timer.stop();
    const int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const auto error = reply->error();
    const QByteArray body = reply->readAll();
    const QString errorText = reply->errorString();
    reply->deleteLater();
    return {error == QNetworkReply::NoError, statusCode, body, errorText};
}

ServiceResult AnalyticsService::predictionQuery(const QJsonObject &payload, const QString &mode)
{
    int horizon = 1;
    if (!horizonValue(payload, &horizon))
        return invalid();
    QSet<qint64> stationIds;
    if (payload.contains("stationId"))
    {
        if (!identifier(payload.value("stationId")))
            return invalid();
        stationIds.insert(qint64(payload.value("stationId").toDouble()));
    }
    if (payload.contains("stationIds"))
    {
        if (!payload.value("stationIds").isArray())
            return invalid();
        const auto values = payload.value("stationIds").toArray();
        if (values.isEmpty() || values.size() > 100)
            return invalid();
        for (const auto &value : values)
        {
            if (!identifier(value))
                return invalid();
            stationIds.insert(qint64(value.toDouble()));
        }
    }

    QUrl url = endpoint(QStringLiteral("/api/predictions"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("horizonHours"), QString::number(horizon));
    url.setQuery(query);
    QStringList stationKeys;
    for (const qint64 id : stationIds)
        stationKeys.append(QString::number(id));
    stationKeys.sort();
    const QString cacheKey = mode + ':' + QString::number(horizon) + ':' + stationKeys.join(',');
    const auto fetched = fetch(url);
    if (!fetched.transportOk || fetched.statusCode < 200 || fetched.statusCode >= 300)
    {
        QMutexLocker locker(&m_cacheMutex);
        if (m_cache.contains(cacheKey))
        {
            auto cached = m_cache.value(cacheKey);
            cached.insert("stale", true);
            cached.insert("message", QStringLiteral("分析服务不可用，已返回最近一次预测"));
            ServiceResult result;
            result.payload = cached;
            return result;
        }
        return failure(fetched.error.contains(QStringLiteral("超时")) ? ErrorCode::RequestTimeout
                                                                       : ErrorCode::NetworkUnavailable,
                       QStringLiteral("分析服务暂不可用"), QStringLiteral("analytics_unavailable"));
    }

    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(fetched.body, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isArray())
        return malformed();
    QJsonArray items;
    for (const auto &value : document.array())
    {
        if (!value.isObject())
            return malformed();
        QJsonObject item;
        if (!normalizeItem(value.toObject(), horizon, &item))
            return malformed();
        const qint64 id = qint64(item.value("stationId").toDouble());
        if (!stationIds.isEmpty() && !stationIds.contains(id))
            continue;
        items.append(item);
    }
    if (mode == QStringLiteral("recommendations"))
    {
        QList<QJsonObject> sorted;
        for (const auto &value : items)
            sorted.append(value.toObject());
        std::sort(sorted.begin(), sorted.end(), [](const QJsonObject &left, const QJsonObject &right) {
            const int leftAvailable = left.value("predictedAvailablePiles").toInt();
            const int rightAvailable = right.value("predictedAvailablePiles").toInt();
            if (leftAvailable != rightAvailable)
                return leftAvailable > rightAvailable;
            return left.value("predictedSessions").toDouble()
                < right.value("predictedSessions").toDouble();
        });
        items = {};
        for (const auto &item : sorted)
            items.append(item);
    }
    else if (mode == QStringLiteral("warnings"))
    {
        QJsonArray filtered;
        for (const auto &value : items)
            if (value.toObject().value("warningLevel").toString() != QStringLiteral("normal"))
                filtered.append(value);
        items = filtered;
    }

    QJsonObject response{{"horizonHours", horizon}, {"items", items}, {"stale", false}};
    if (!items.isEmpty())
        response.insert("generatedAt", items.first().toObject().value("generatedAt"));
    {
        QMutexLocker locker(&m_cacheMutex);
        m_cache.insert(cacheKey, response);
    }
    ServiceResult result;
    result.payload = response;
    return result;
}

ServiceResult AnalyticsService::predictions(const QJsonObject &payload)
{
    return predictionQuery(payload, QStringLiteral("predictions"));
}

ServiceResult AnalyticsService::recommendations(const QJsonObject &payload)
{
    return predictionQuery(payload, QStringLiteral("recommendations"));
}

ServiceResult AnalyticsService::warnings(const QJsonObject &payload)
{
    return predictionQuery(payload, QStringLiteral("warnings"));
}

ServiceResult AnalyticsService::status()
{
    const auto fetched = fetch(endpoint(QStringLiteral("/api/health")));
    if (!fetched.transportOk || fetched.statusCode < 200 || fetched.statusCode >= 300)
        return failure(ErrorCode::NetworkUnavailable, QStringLiteral("分析服务暂不可用"),
                       QStringLiteral("analytics_unavailable"));
    QJsonParseError error;
    const auto document = QJsonDocument::fromJson(fetched.body, &error);
    if (error.error != QJsonParseError::NoError || !document.isObject())
        return malformed();
    ServiceResult result;
    result.payload = document.object();
    return result;
}

ServiceResult AnalyticsService::report(const QString &name, const QJsonObject &payload)
{
    static const QSet<QString> allowed{"model-comparison", "drift", "scheduling", "maintenance",
                                       "expansion", "regulator"};
    if (!allowed.contains(name) || !payload.isEmpty())
        return invalid();
    const auto fetched = fetch(endpoint(QStringLiteral("/api/") + name));
    if (!fetched.transportOk || fetched.statusCode < 200 || fetched.statusCode >= 300)
        return failure(fetched.error.contains(QStringLiteral("超时")) ? ErrorCode::RequestTimeout
                                                                       : ErrorCode::NetworkUnavailable,
                       QStringLiteral("分析服务暂不可用"), QStringLiteral("analytics_unavailable"));
    QJsonParseError error;
    const auto document = QJsonDocument::fromJson(fetched.body, &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) return malformed();
    const auto envelope = document.object();
    if (!envelope.contains("data") || !envelope.value("meta").isObject()) return malformed();
    ServiceResult result;
    result.payload = QJsonObject{{"data", envelope.value("data")}, {"meta", envelope.value("meta")}};
    return result;
}

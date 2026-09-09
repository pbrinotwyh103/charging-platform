#include "map/mapnavigator.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <QUrlQuery>

QString MapNavigator::apiKey()
{
    const QByteArray env = qgetenv("TENCENT_MAP_KEY");
    if (!env.isEmpty()) return QString::fromUtf8(env).trimmed();
    const QStringList candidates{
        QDir::current().absoluteFilePath(QStringLiteral("config/app.ini")),
        QDir(QCoreApplication::applicationDirPath()).absoluteFilePath(QStringLiteral("../../config/app.ini")),
        QDir(QCoreApplication::applicationDirPath()).absoluteFilePath(QStringLiteral("../../../../config/app.ini"))
    };
    for (const auto &path : candidates) {
        if (!QFileInfo::exists(path)) continue;
        QSettings settings(path, QSettings::IniFormat);
        const QString key = settings.value(QStringLiteral("map/tencent_key")).toString().trimmed();
        if (!key.isEmpty()) return key;
    }
    return {};
}

bool MapNavigator::isConfigured()
{
    return !apiKey().isEmpty();
}

QUrl MapNavigator::navigationUrl(double fromLat, double fromLon, double toLat,
                                 double toLon, const QString &mode)
{
    if (!isConfigured()) return {};
    const QString safeMode = mode == QStringLiteral("walking") ? QStringLiteral("walk") : QStringLiteral("drive");
    QUrl url(QStringLiteral("https://apis.map.qq.com/uri/v1/routeplan"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("type"), safeMode);
    query.addQueryItem(QStringLiteral("from"), QStringLiteral("当前位置"));
    query.addQueryItem(QStringLiteral("fromcoord"), QStringLiteral("%1,%2").arg(fromLat, 0, 'f', 6).arg(fromLon, 0, 'f', 6));
    query.addQueryItem(QStringLiteral("to"), QStringLiteral("目标充电站"));
    query.addQueryItem(QStringLiteral("tocoord"), QStringLiteral("%1,%2").arg(toLat, 0, 'f', 6).arg(toLon, 0, 'f', 6));
    query.addQueryItem(QStringLiteral("coord_type"), QStringLiteral("1"));
    query.addQueryItem(QStringLiteral("policy"), QStringLiteral("0"));
    query.addQueryItem(QStringLiteral("key"), apiKey());
    url.setQuery(query);
    return url;
}

QUrl MapNavigator::geocodingUrl(const QString &address, const QString &region)
{
    if (!isConfigured() || address.trimmed().isEmpty()) return {};
    QUrl url(QStringLiteral("https://apis.map.qq.com/ws/geocoder/v1/"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("address"), address.trimmed());
    const QString normalizedRegion = region.trimmed();
    if (!normalizedRegion.isEmpty() && normalizedRegion != QStringLiteral("全部区域"))
        query.addQueryItem(QStringLiteral("region"), normalizedRegion);
    query.addQueryItem(QStringLiteral("key"), apiKey());
    url.setQuery(query);
    return url;
}

bool MapNavigator::parseGeocodingResponse(const QByteArray &body, double *latitude,
                                           double *longitude, QString *error)
{
    if (!latitude || !longitude) return false;
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(body, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        if (error) *error = QStringLiteral("地图服务返回了无法解析的数据");
        return false;
    }
    const QJsonObject root = document.object();
    if (root.value(QStringLiteral("status")).toInt(-1) != 0) {
        if (error) {
            *error = root.value(QStringLiteral("message")).toString(
                QStringLiteral("腾讯地图未找到该地址"));
        }
        return false;
    }
    const QJsonObject location = root.value(QStringLiteral("result")).toObject()
                                     .value(QStringLiteral("location")).toObject();
    const double lat = location.value(QStringLiteral("lat")).toDouble(999.0);
    const double lon = location.value(QStringLiteral("lng")).toDouble(999.0);
    if (lat < -90.0 || lat > 90.0 || lon < -180.0 || lon > 180.0) {
        if (error) *error = QStringLiteral("地图服务没有返回有效坐标");
        return false;
    }
    *latitude = lat;
    *longitude = lon;
    return true;
}

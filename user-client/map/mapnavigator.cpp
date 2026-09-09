#include "map/mapnavigator.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QSettings>
#include <QUrlQuery>
#include <QtMath>

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

bool MapNavigator::isAllowedNavigationUrl(const QUrl &url)
{
    if (url == QUrl(QStringLiteral("about:blank"))) return true;
    if (url.scheme().compare(QStringLiteral("https"), Qt::CaseInsensitive) != 0)
        return false;
    const QString host = url.host().toLower();
    return host == QStringLiteral("map.qq.com")
        || host.endsWith(QStringLiteral(".map.qq.com"));
}

QUrl MapNavigator::navigationUrl(double fromLat, double fromLon, double toLat,
                                 double toLon, const QString &mode)
{
    if (!isConfigured()) return {};
    if (!qIsFinite(fromLat) || !qIsFinite(fromLon) || !qIsFinite(toLat) || !qIsFinite(toLon)
        || fromLat < -90.0 || fromLat > 90.0 || toLat < -90.0 || toLat > 90.0
        || fromLon < -180.0 || fromLon > 180.0 || toLon < -180.0 || toLon > 180.0)
        return {};
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

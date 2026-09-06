#include "pages/stationutils.h"
#include <QJsonObject>
#include <QtMath>
#include <algorithm>
#include <cmath>
#include <limits>
namespace StationUtils {
double distanceKm(double a, double o, double b, double p) {
    if (!std::isfinite(a) || !std::isfinite(o)
        || !std::isfinite(b) || !std::isfinite(p)
        || a < -90.0 || a > 90.0 || b < -90.0 || b > 90.0
        || o < -180.0 || o > 180.0 || p < -180.0 || p > 180.0)
        return std::numeric_limits<double>::quiet_NaN();
    constexpr double r = 6371.0088;
    const double da = qDegreesToRadians(b - a);
    const double d = qDegreesToRadians(p - o);
    const double rawH = qSin(da / 2) * qSin(da / 2)
        + qCos(qDegreesToRadians(a)) * qCos(qDegreesToRadians(b))
            * qSin(d / 2) * qSin(d / 2);
    const double h = qBound(0.0, rawH, 1.0);
    return r * 2 * qAtan2(qSqrt(h), qSqrt(1 - h));
}
QJsonArray sortByDistance(const QJsonArray &stations, double lat, double lon) {
    QList<QJsonObject> list;
    for (const auto &v : stations) {
        auto object = v.toObject();
        const double stationLat = object.value(QStringLiteral("latitude")).toDouble(
            std::numeric_limits<double>::quiet_NaN());
        const double stationLon = object.value(QStringLiteral("longitude")).toDouble(
            std::numeric_limits<double>::quiet_NaN());
        object.insert(QStringLiteral("distanceKm"),
                      distanceKm(lat, lon, stationLat, stationLon));
        list.push_back(object);
    }
    std::stable_sort(list.begin(), list.end(), [](const auto &left, const auto &right) {
        const double leftDistance = left.value(QStringLiteral("distanceKm")).toDouble(
            std::numeric_limits<double>::quiet_NaN());
        const double rightDistance = right.value(QStringLiteral("distanceKm")).toDouble(
            std::numeric_limits<double>::quiet_NaN());
        const bool leftValid = std::isfinite(leftDistance);
        const bool rightValid = std::isfinite(rightDistance);
        if (leftValid != rightValid) return leftValid;
        return leftValid && leftDistance < rightDistance;
    });
    QJsonArray out;
    for (const auto &object : list) out.append(object);
    return out;
}
}

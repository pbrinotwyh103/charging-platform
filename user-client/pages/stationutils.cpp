#include "pages/stationutils.h"
#include <QJsonObject>
#include <QtMath>
#include <algorithm>
#include <limits>
namespace StationUtils {
bool isValidCoordinate(double latitude, double longitude)
{
    return qIsFinite(latitude) && qIsFinite(longitude)
        && latitude >= -90.0 && latitude <= 90.0
        && longitude >= -180.0 && longitude <= 180.0;
}

double distanceKm(double a, double o, double b, double p) {
    if (!isValidCoordinate(a, o) || !isValidCoordinate(b, p))
        return std::numeric_limits<double>::infinity();
    constexpr double r = 6371.0088;
    const double da = qDegreesToRadians(b-a), d = qDegreesToRadians(p-o);
    const double raw = qSin(da/2)*qSin(da/2)+qCos(qDegreesToRadians(a))*qCos(qDegreesToRadians(b))*qSin(d/2)*qSin(d/2);
    const double h = qBound(0.0, raw, 1.0);
    return r * 2 * qAtan2(qSqrt(h), qSqrt(1-h));
}
QJsonArray sortByDistance(const QJsonArray &stations, double lat, double lon) {
    struct LocatedStation { QJsonObject station; double distance; };
    QList<LocatedStation> list;
    for (const auto &value : stations) {
        if (!value.isObject()) continue;
        auto station = value.toObject();
        const QJsonValue latitudeValue = station.value(QStringLiteral("latitude"));
        const QJsonValue longitudeValue = station.value(QStringLiteral("longitude"));
        const double distance = latitudeValue.isDouble() && longitudeValue.isDouble()
            ? distanceKm(lat, lon, latitudeValue.toDouble(), longitudeValue.toDouble())
            : std::numeric_limits<double>::infinity();
        station.insert(QStringLiteral("distanceKm"),
                       qIsFinite(distance) ? QJsonValue(distance) : QJsonValue(QJsonValue::Null));
        list.push_back({station, distance});
    }
    std::stable_sort(list.begin(), list.end(), [](const auto &a, const auto &b) {
        return a.distance < b.distance;
    });
    QJsonArray out;
    for (const auto &item : list) out.append(item.station);
    return out;
}
}

#include "pages/stationutils.h"
#include <QJsonObject>
#include <QtMath>
#include <algorithm>
namespace StationUtils {
double distanceKm(double a, double o, double b, double p) {
    constexpr double r = 6371.0088; const double da = qDegreesToRadians(b-a), d= qDegreesToRadians(p-o);
    const double h = qSin(da/2)*qSin(da/2)+qCos(qDegreesToRadians(a))*qCos(qDegreesToRadians(b))*qSin(d/2)*qSin(d/2);
    return r * 2 * qAtan2(qSqrt(h), qSqrt(1-h));
}
QJsonArray sortByDistance(const QJsonArray &stations, double lat, double lon) {
    QList<QJsonObject> list; for (const auto &v : stations) { auto o=v.toObject(); o.insert(QStringLiteral("distanceKm"), distanceKm(lat,lon,o.value("latitude").toDouble(),o.value("longitude").toDouble())); list.push_back(o); }
    std::stable_sort(list.begin(), list.end(), [](const auto&a,const auto&b){ return a.value("distanceKm").toDouble() < b.value("distanceKm").toDouble(); });
    QJsonArray out; for (const auto&o:list) out.append(o); return out;
}
}

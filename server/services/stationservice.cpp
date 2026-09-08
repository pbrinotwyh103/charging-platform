#include "services/stationservice.h"
#include "services/servicehelpers.h"
#include "repositories/favoriterepository.h"
#include "repositories/stationrepository.h"
#include "repositories/userrepository.h"
#include <QDateTime>
#include <QJsonArray>
#include <QSet>
#include <algorithm>

using namespace ServiceHelpers;

namespace {
double distanceKm(double latitude, double longitude, const StationRecord &station)
{
    constexpr double radians = 3.14159265358979323846 / 180.0;
    const double dLat = (station.latitude - latitude) * radians;
    const double dLon = (station.longitude - longitude) * radians;
    const double a = std::pow(std::sin(dLat / 2), 2)
        + std::cos(latitude * radians) * std::cos(station.latitude * radians)
            * std::pow(std::sin(dLon / 2), 2);
    return 6371.0 * 2 * std::asin(std::sqrt(qBound(0.0, a, 1.0)));
}
bool coordinate(const QJsonValue &value, double minimum, double maximum)
{
    return value.isDouble() && std::isfinite(value.toDouble())
        && value.toDouble() >= minimum && value.toDouble() <= maximum;
}
}

ServiceResult StationService::nearby(qint64 userId, const QJsonObject &payload)
{
    ConnectionCleanup cleanup(database());
    Pagination page;
    const bool hasCoordinates = payload.contains("latitude") || payload.contains("longitude");
    if (userId <= 0 || !pagination(payload, &page)
        || (hasCoordinates && (!coordinate(payload.value("latitude"), -90, 90)
                               || !coordinate(payload.value("longitude"), -180, 180))))
        return invalid();
    for (const auto &key : {"region", "address", "sort"})
        if (payload.contains(key) && !payload.value(key).isString()) return invalid();
    if (payload.contains("radiusKm") && !coordinate(payload.value("radiusKm"), 1, 100)) return invalid();
    const QString sort = payload.value("sort").toString();
    if (!sort.isEmpty() && sort != "distance" && sort != "name" && sort != "price") return invalid();
    if (!ready(database())) return databaseError();
    QList<StationRecord> stations;
    QList<qint64> favoriteIds;
    QString error;
    if (!StationRepository(database()).list("online", &stations, &error)
        || !FavoriteRepository(database()).listStationIds(userId, &favoriteIds, &error)) return databaseError();
    const QSet<qint64> favorites(favoriteIds.cbegin(), favoriteIds.cend());
    struct Match { StationRecord station; double distance; };
    QList<Match> matches;
    QString region = payload.value("region").toString().trimmed();
    if (region == QStringLiteral("全部区域")) region.clear();
    const QString address = payload.value("address").toString().trimmed();
    const double radius = payload.value("radiusKm").toDouble(10);
    for (const auto &station : stations) {
        if ((!region.isEmpty() && !station.name.contains(region, Qt::CaseInsensitive)
             && !station.address.contains(region, Qt::CaseInsensitive))
            || (!address.isEmpty() && !station.name.contains(address, Qt::CaseInsensitive)
                && !station.address.contains(address, Qt::CaseInsensitive))) continue;
        const double distance = hasCoordinates
            ? distanceKm(payload.value("latitude").toDouble(), payload.value("longitude").toDouble(), station) : 0;
        if (hasCoordinates && distance > radius) continue;
        matches.append({station, distance});
    }
    std::stable_sort(matches.begin(), matches.end(), [&](const Match &a, const Match &b) {
        if (sort == "name") return a.station.name < b.station.name;
        if (sort == "price") return a.station.priceCentsPerKwh < b.station.priceCentsPerKwh;
        if (hasCoordinates) return a.distance < b.distance;
        return a.station.id < b.station.id;
    });
    QJsonArray items;
    const int end = int(qMin<qint64>(matches.size(), qint64(page.offset) + page.pageSize));
    for (int i = page.offset; i < end; ++i) {
        const auto &match = matches[i];
        const auto &s = match.station;
        items.append(QJsonObject{{"stationId", double(s.id)}, {"name", s.name}, {"address", s.address},
            {"latitude", s.latitude}, {"longitude", s.longitude}, {"priceCentsPerKwh", double(s.priceCentsPerKwh)},
            {"totalPiles", s.totalPileCount}, {"availablePiles", s.idlePileCount},
            {"favorited", favorites.contains(s.id)}, {"status", s.status},
            {"distanceKm", hasCoordinates ? QJsonValue(std::round(match.distance * 100) / 100) : QJsonValue()}});
    }
    ServiceResult result;
    result.payload = {{"items", items}, {"page", page.page}, {"pageSize", page.pageSize}, {"total", matches.size()}};
    return result;
}

ServiceResult StationService::toggleFavorite(qint64 userId, const QJsonObject &payload)
{
    ConnectionCleanup cleanup(database());
    if (userId <= 0 || !identifier(payload.value("stationId")) || !payload.value("favorited").isBool()) return invalid();
    if (!ready(database())) return databaseError();
    const qint64 stationId = qint64(payload.value("stationId").toDouble());
    const bool target = payload.value("favorited").toBool();
    StationRecord station;
    UserRecord user;
    QString error;
    if (!StationRepository(database()).findById(stationId, &station, &error)
        || !UserRepository(database()).findById(userId, &user, &error)) return databaseError();
    if (!station.id || !user.id) return failure(Charging::ErrorCode::NotFound, QStringLiteral("用户或充电站不存在"));
    FavoriteRepository favorites(database());
    bool changed = false;
    if (target ? !favorites.add(userId, stationId, &changed, &error)
               : !favorites.remove(userId, stationId, &changed, &error)) return databaseError();
    ServiceResult result;
    result.payload = {{"stationId", double(stationId)}, {"favorited", target},
                      {"updatedAt", QDateTime::currentDateTimeUtc().toString(Qt::ISODate)}};
    return result;
}

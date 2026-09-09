#include "services/pileservice.h"
#include "services/servicehelpers.h"
#include "repositories/favoriterepository.h"
#include "repositories/pilerepository.h"
#include "repositories/stationrepository.h"
#include <QJsonArray>

using namespace ServiceHelpers;

ServiceResult PileService::listForStation(const QJsonObject &payload)
{
    ConnectionCleanup cleanup(database());
    Pagination page;
    if (!identifier(payload.value("stationId")) || !pagination(payload, &page)) return invalid();
    if (!ready(database())) return databaseError();
    const qint64 stationId = qint64(payload.value("stationId").toDouble());
    QString error;
    StationRecord station;
    if (!StationRepository(database()).findById(stationId, &station, &error)) return databaseError();
    if (!station.id) return failure(Charging::ErrorCode::NotFound, QStringLiteral("充电站不存在"));
    PileRepository piles(database());
    QList<PileRecord> records;
    int total = 0;
    if (!piles.countByStation(stationId, {}, &total, &error)
        || !piles.listByStation(stationId, {}, page.pageSize, page.offset, &records, &error)) return databaseError();
    QJsonArray items;
    for (const auto &pile : records) {
        items.append(QJsonObject{{"stationId", double(stationId)}, {"pileId", double(pile.id)},
            {"pileCode", pile.pileCode}, {"type", pile.chargeType}, {"powerKw", pile.powerKw},
            {"status", pile.status == "idle" ? QString("available") : pile.status},
            {"priceCentsPerKwh", double(station.priceCentsPerKwh)}, {"updatedAt", pile.updatedAt}});
    }
    ServiceResult result;
    result.payload = {{"stationId", double(stationId)}, {"items", items},
                      {"page", page.page}, {"pageSize", page.pageSize}, {"total", total}};
    return result;
}

ServiceResult PileService::findByCode(qint64 userId, const QJsonObject &payload)
{
    ConnectionCleanup cleanup(database());
    if (userId <= 0 || !payload.value("pileCode").isString()) return invalid();
    const QString pileCode = payload.value("pileCode").toString().trimmed();
    if (pileCode.isEmpty() || pileCode.size() > 64) return invalid();
    if (!ready(database())) return databaseError();

    QString error;
    PileRecord pile;
    if (!PileRepository(database()).findByCode(pileCode, &pile, &error)) return databaseError();
    if (!pile.id)
        return failure(Charging::ErrorCode::NotFound, QStringLiteral("未找到该电桩编号"),
                       QStringLiteral("pile_code_not_found"));

    StationRecord station;
    bool favorite = false;
    if (!StationRepository(database()).findById(pile.stationId, &station, &error)
        || !FavoriteRepository(database()).contains(userId, pile.stationId, &favorite, &error))
        return databaseError();
    if (!station.id)
        return failure(Charging::ErrorCode::NotFound, QStringLiteral("电桩所属充电站不存在"));

    const QJsonObject stationObject{
        {"stationId", double(station.id)}, {"name", station.name},
        {"address", station.address}, {"latitude", station.latitude},
        {"longitude", station.longitude},
        {"priceCentsPerKwh", double(station.priceCentsPerKwh)},
        {"totalPiles", station.totalPileCount}, {"availablePiles", station.idlePileCount},
        {"favorited", favorite}, {"status", station.status}, {"distanceKm", QJsonValue()}};
    const QJsonObject pileObject{
        {"stationId", double(station.id)}, {"pileId", double(pile.id)},
        {"pileCode", pile.pileCode}, {"type", pile.chargeType},
        {"powerKw", pile.powerKw},
        {"status", pile.status == "idle" ? QStringLiteral("available") : pile.status},
        {"priceCentsPerKwh", double(station.priceCentsPerKwh)},
        {"updatedAt", pile.updatedAt}};
    ServiceResult result;
    result.payload = {{"station", stationObject}, {"pile", pileObject}};
    return result;
}

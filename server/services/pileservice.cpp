#include "services/pileservice.h"
#include "services/servicehelpers.h"
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

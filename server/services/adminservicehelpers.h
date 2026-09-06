#pragma once
#include "services/servicehelpers.h"
#include "repositories/orderrepository.h"
#include "repositories/stationrepository.h"
#include "repositories/pilerepository.h"
#include <QJsonArray>

namespace AdminServiceHelpers
{
inline bool allOrders(DatabaseManager *database, QList<OrderRecord> *records, QString *error)
{
    records->clear();
    OrderRepository repository(database);
    for (int offset = 0;; offset += 200)
    {
        QList<OrderRecord> batch;
        if (!repository.list({}, 200, offset, &batch, error))
            return false;
        records->append(batch);
        if (batch.size() < 200)
            return true;
        if (offset > std::numeric_limits<int>::max() - 200)
            return false;
    }
}
inline bool allPiles(DatabaseManager *database, QList<PileRecord> *records, QString *error)
{
    records->clear();
    QList<StationRecord> stations;
    if (!StationRepository(database).list({}, &stations, error))
        return false;
    for (const auto &station : stations)
    {
        QList<PileRecord> batch;
        if (!PileRepository(database).listByStation(station.id, {}, &batch, error))
            return false;
        records->append(batch);
    }
    return true;
}
inline ServiceResult pageResult(const QJsonArray &records, const ServiceHelpers::Pagination &page)
{
    QJsonArray items;
    for (qsizetype i = page.offset; i < records.size() && items.size() < page.pageSize; ++i)
        items.append(records.at(i));
    ServiceResult result;
    result.payload = {
        {"items", items},
        {"page", page.page},
        {"pageSize", page.pageSize},
        {"total", records.size()},
        {"totalPages", qMax(qsizetype(1), (records.size() + page.pageSize - 1) / page.pageSize)}};
    return result;
}
inline bool textFields(const QJsonObject &payload, const QStringList &fields)
{
    for (const auto &field : fields)
        if (payload.contains(field) && !payload.value(field).isString())
            return false;
    return true;
}
inline QJsonObject stationJson(const StationRecord &s)
{
    return {{"stationId", double(s.id)},
            {"name", s.name},
            {"stationName", s.name},
            {"address", s.address},
            {"longitude", s.longitude},
            {"latitude", s.latitude},
            {"priceCentsPerKwh", double(s.priceCentsPerKwh)},
            {"status", s.status},
            {"totalPileCount", s.totalPileCount},
            {"pileCount", s.totalPileCount},
            {"idlePileCount", s.idlePileCount},
            {"onlinePileCount", s.onlinePileCount},
            {"createdAt", s.createdAt},
            {"updatedAt", s.updatedAt}};
}
inline QJsonObject pileJson(const PileRecord &p)
{
    return {{"pileId", double(p.id)},
            {"stationId", double(p.stationId)},
            {"pileCode", p.pileCode},
            {"type", p.chargeType},
            {"chargeType", p.chargeType},
            {"powerKw", p.powerKw},
            {"status", p.status == "idle" ? QString("available") : p.status},
            {"totalChargeCount", p.totalChargeCount},
            {"totalChargeSeconds", double(p.totalChargeSeconds)},
            {"lastHeartbeatAt", p.lastHeartbeatAt},
            {"updatedAt", p.updatedAt}};
}
} // namespace AdminServiceHelpers

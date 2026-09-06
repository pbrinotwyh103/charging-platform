#include "services/adminservice.h"
#include "services/adminservicehelpers.h"
#include "services/statisticsservice.h"
#include "services/chargingservice.h"
#include "services/orderservice.h"
#include "repositories/userrepository.h"
#include "repositories/alarmrepository.h"
#include "repositories/controlrecordrepository.h"
#include "protocol/message.h"
#include <QDateTime>
#include <QHash>
#include <QJsonDocument>
#include <QMutex>

using namespace ServiceHelpers;
using namespace AdminServiceHelpers;
using Charging::ErrorCode;

namespace
{
ServiceResult missing(const QString &entity)
{
    return failure(ErrorCode::NotFound, entity + QStringLiteral("不存在"));
}
ServiceResult commandConflict()
{
    return failure(ErrorCode::Conflict, QStringLiteral("当前状态不允许执行该操作"), "command_conflict");
}
QJsonObject userJson(const UserRecord &u)
{
    return {{"userId", double(u.id)},
            {"phone", u.phone},
            {"nickname", u.nickname},
            {"avatar", u.avatarPath},
            {"balanceCents", double(u.balanceCents)},
            {"status", u.status},
            {"createdAt", u.createdAt},
            {"updatedAt", u.updatedAt}};
}
QJsonObject alarmJson(const AlarmRecord &a)
{
    return {{"alarmId", double(a.id)},      {"pileId", double(a.pileId)},
            {"orderId", double(a.orderId)}, {"alarmType", a.alarmType},
            {"severity", a.severity},       {"message", a.message},
            {"status", a.status},           {"occurredAt", a.occurredAt},
            {"recoveredAt", a.recoveredAt}, {"handledByAdminId", double(a.handledByAdminId)}};
}
QString csvCell(QString text)
{
    // Spreadsheet applications must treat user-controlled values as text.
    if (!text.isEmpty() && QStringLiteral("=+-@\t\r").contains(text.front()))
        text.prepend('\'');
    text.replace('"', "\"\"");
    return '"' + text + '"';
}
bool dateFilter(const QJsonObject &p, QDate *from, QDate *to)
{
    if (!textFields(p, {"from", "to"}))
        return false;
    for (const auto &key : QStringList{"from", "to"})
    {
        const auto value = p.value(key).toString();
        const auto date = QDate::fromString(value, Qt::ISODate);
        if (!value.isEmpty() && (!date.isValid() || date.toString(Qt::ISODate) != value))
            return false;
        if (key == "from")
            *from = date;
        else
            *to = date;
    }
    return !from->isValid() || !to->isValid() || *from <= *to;
}
} // namespace

ServiceResult AdminService::execute(qint64 adminId, quint32 requestId, const QJsonObject &payload,
                                    ChargingService *charging)
{
    ConnectionCleanup cleanup(database());
    if (adminId <= 0)
        return failure(ErrorCode::Unauthorized, QStringLiteral("请先登录管理员账号"));
    if (!requestId || !payload.value("action").isString() ||
        payload.value("action").toString().trimmed().isEmpty())
        return invalid();
    QJsonObject request = payload;
    QString action = payload.value("action").toString().trimmed();
    static const QHash<QString, QString> aliases{
        {"dashboard.revenue", "revenue.trend"}, {"stations.list", "station.list"},
        {"stations.detail", "station.detail"},  {"stations.create", "station.create"},
        {"stations.update", "station.update"},  {"piles.list", "pile.list"},
        {"piles.detail", "pile.detail"},        {"users.list", "user.list"},
        {"orders.list", "order.list"},          {"alarms.list", "alarm.list"},
        {"alarms.detail", "alarm.detail"},      {"charging.stop", "pile.stop"}};
    if (action == "users.freeze")
    {
        if (!payload.value("frozen").isBool())
            return invalid();
        action = payload.value("frozen").toBool() ? "user.freeze" : "user.unfreeze";
    }
    else if (action == "piles.control")
    {
        const auto command = payload.value("command").toString();
        if (!QStringList{"stop", "restart", "enable", "disable"}.contains(command))
        {
            // Invalid device commands still enter the audit path.
            request.insert("action", "pile.invalid");
            return control(adminId, requestId, request, charging);
        }
        action = "pile." + command;
    }
    else
        action = aliases.value(action, action);
    request.insert("action", action);
    static const QHash<QString, Handler> handlers{{"dashboard.summary", &AdminService::statistics},
                                                  {"revenue.trend", &AdminService::statistics},
                                                  {"pile.status.summary", &AdminService::statistics},
                                                  {"station.list", &AdminService::stations},
                                                  {"station.detail", &AdminService::stations},
                                                  {"station.create", &AdminService::stations},
                                                  {"station.update", &AdminService::stations},
                                                  {"pile.list", &AdminService::piles},
                                                  {"pile.detail", &AdminService::piles},
                                                  {"pile.stop", &AdminService::control},
                                                  {"pile.restart", &AdminService::control},
                                                  {"pile.enable", &AdminService::control},
                                                  {"pile.disable", &AdminService::control},
                                                  {"user.list", &AdminService::users},
                                                  {"user.freeze", &AdminService::users},
                                                  {"user.unfreeze", &AdminService::users},
                                                  {"order.list", &AdminService::orders},
                                                  {"charging.active.list", &AdminService::orders},
                                                  {"report.export", &AdminService::orders},
                                                  {"alarm.list", &AdminService::alarms},
                                                  {"alarm.detail", &AdminService::alarms},
                                                  {"alarm.handle", &AdminService::alarms}};
    const auto handler = handlers.constFind(action);
    if (handler == handlers.constEnd())
        return failure(ErrorCode::UnsupportedMessage, QStringLiteral("不支持的管理操作"));
    if (!ready(database()))
        return databaseError();
    auto result = (this->*handler.value())(adminId, requestId, request, charging);
    result.payload.insert("action", payload.value("action"));
    if (result.payload.contains("page"))
    {
        QJsonObject meta;
        for (const auto &key : QStringList{"page", "pageSize", "total", "totalPages"})
            meta.insert(key, result.payload.value(key));
        result.payload.insert("meta", meta);
    }
    // The existing admin client renders 'idle'; canonical public actions expose
    // 'available' as agreed by the user-facing contract.
    if (payload.value("action").toString().startsWith("piles."))
    {
        if (result.payload.value("status") == "available")
            result.payload.insert("status", "idle");
        if (result.payload.contains("items"))
        {
            QJsonArray items;
            for (const auto &value : result.payload.value("items").toArray())
            {
                auto item = value.toObject();
                if (item.value("status") == "available")
                    item.insert("status", "idle");
                items.append(item);
            }
            result.payload.insert("items", items);
        }
    }
    if (result.succeeded() && result.message.isEmpty())
        result.message = result.payload.value("message").toString(QStringLiteral("操作成功"));
    if (action == "report.export" && result.succeeded())
    {
        // Match PacketCodec's compact UTF-8 JSON, including the envelope fields
        // the dispatcher adds. Reserve 1 KiB plus the binary header so the
        // entire reply remains below the 4 MiB transport ceiling.
        auto envelope = result.payload;
        envelope.insert("message", result.message);
        const qsizetype encodedBytes = QJsonDocument(envelope).toJson(QJsonDocument::Compact).size();
        constexpr qsizetype reserve = Charging::MessageHeader::SerializedSize + 1024;
        if (encodedBytes > Charging::MessageHeader::MaxPayloadLength - reserve)
        {
            result = failure(ErrorCode::RateLimited, QStringLiteral("导出结果过大，请缩小日期或订单筛选范围"),
                             "export_too_large");
            result.payload.insert("action", payload.value("action"));
        }
    }
    return result;
}

ServiceResult AdminService::statistics(qint64, quint32, const QJsonObject &payload, ChargingService *)
{
    StatisticsService service(database());
    const auto action = payload.value("action").toString();
    if (action == "revenue.trend")
        return service.revenueTrend(payload);
    if (action == "pile.status.summary")
        return service.pileStatus(payload);
    return service.summary(payload);
}

ServiceResult AdminService::stations(qint64, quint32, const QJsonObject &payload, ChargingService *)
{
    const auto action = payload.value("action").toString();
    StationRepository repository(database());
    QString error;
    if (!textFields(payload, {"name", "address", "keyword", "status"}))
        return invalid();
    if (payload.contains("status") &&
        !QStringList{"", "online", "offline"}.contains(payload.value("status").toString()))
        return invalid();
    if (action == "station.list")
    {
        Pagination page;
        if (!pagination(payload, &page))
            return invalid();
        QList<StationRecord> records;
        if (!repository.list(payload.value("status").toString(), &records, &error))
            return databaseError();
        QJsonArray items;
        const auto keyword = payload.value("keyword").toString().trimmed();
        for (const auto &s : records)
            if (keyword.isEmpty() || s.name.contains(keyword, Qt::CaseInsensitive) ||
                s.address.contains(keyword, Qt::CaseInsensitive))
                items.append(stationJson(s));
        return pageResult(items, page);
    }
    StationRecord station;
    if (action != "station.create")
    {
        if (!identifier(payload.value("stationId")))
            return invalid();
        if (!repository.findById(qint64(payload.value("stationId").toDouble()), &station, &error))
            return databaseError();
        if (!station.id)
            return missing(QStringLiteral("充电站"));
    }
    if (action == "station.detail")
    {
        ServiceResult result;
        result.payload = stationJson(station);
        return result;
    }
    const bool create = action == "station.create";
    bool changed = false;
    for (const auto &key :
         QStringList{"name", "address", "longitude", "latitude", "priceCentsPerKwh", "status"})
    {
        if (payload.contains(key))
            changed = true;
        else if (create && key != "status")
            return invalid();
    }
    if (!changed)
        return invalid();
    if (payload.contains("name"))
        station.name = payload.value("name").toString().trimmed();
    if (payload.contains("address"))
        station.address = payload.value("address").toString().trimmed();
    if (station.name.isEmpty() || station.name.size() > 100 || station.address.isEmpty() ||
        station.address.size() > 500)
        return invalid();
    for (const auto &key : QStringList{"longitude", "latitude"})
    {
        if (!payload.contains(key))
            continue;
        const auto value = payload.value(key);
        const double bound = key == "longitude" ? 180 : 90;
        if (!value.isDouble() || !std::isfinite(value.toDouble()) || std::abs(value.toDouble()) > bound)
            return invalid();
        if (key == "longitude")
            station.longitude = value.toDouble();
        else
            station.latitude = value.toDouble();
    }
    if (payload.contains("priceCentsPerKwh"))
    {
        if (!integer(payload.value("priceCentsPerKwh"), 0, 1000000))
            return invalid();
        station.priceCentsPerKwh = qint64(payload.value("priceCentsPerKwh").toDouble());
    }
    if (payload.contains("status"))
        station.status = payload.value("status").toString();
    if (station.status.isEmpty())
        station.status = "online";
    if (create)
    {
        if (!repository.insert(station, &station.id, &error))
            return databaseError();
    }
    else
    {
        StationPatch patch;
        if (payload.contains("name")) patch.name = station.name;
        if (payload.contains("address")) patch.address = station.address;
        if (payload.contains("longitude")) patch.longitude = station.longitude;
        if (payload.contains("latitude")) patch.latitude = station.latitude;
        if (payload.contains("priceCentsPerKwh")) patch.priceCentsPerKwh = station.priceCentsPerKwh;
        if (payload.contains("status")) patch.status = station.status;
        if (!repository.updateFields(station.id, patch, &error))
            return databaseError();
    }
    if (!repository.findById(station.id, &station, &error))
        return databaseError();
    ServiceResult result;
    result.payload = stationJson(station);
    return result;
}

ServiceResult AdminService::piles(qint64, quint32, const QJsonObject &payload, ChargingService *)
{
    QString error;
    const bool detail = payload.value("action") == "pile.detail";
    QList<PileRecord> records;
    Pagination page;
    if (detail)
    {
        if (!identifier(payload.value("pileId")))
            return invalid();
        PileRecord pile;
        if (!PileRepository(database()).findById(qint64(payload.value("pileId").toDouble()), &pile, &error))
            return databaseError();
        if (!pile.id)
            return missing(QStringLiteral("电桩"));
        records.append(pile);
    }
    else
    {
        if (!pagination(payload, &page) || !textFields(payload, {"status"}) ||
            (payload.contains("stationId") && !integer(payload.value("stationId"), 0, 9007199254740991.0)))
            return invalid();
        if (!allPiles(database(), &records, &error))
            return databaseError();
    }
    const auto stationId = qint64(payload.value("stationId").toDouble());
    QString status = payload.value("status").toString();
    if (status == "available")
        status = "idle";
    if (!QStringList{"", "idle", "reserved", "charging", "fault", "offline", "disabled"}.contains(status))
        return invalid();
    QJsonArray items;
    for (const auto &pile : records)
    {
        if (!detail &&
            ((stationId && stationId != pile.stationId) || (!status.isEmpty() && status != pile.status)))
            continue;
        StationRecord station;
        if (!StationRepository(database()).findById(pile.stationId, &station, &error))
            return databaseError();
        auto item = pileJson(pile);
        item.insert("stationName", station.name);
        item.insert("priceCentsPerKwh", double(station.priceCentsPerKwh));
        items.append(item);
    }
    if (detail)
    {
        ServiceResult result;
        result.payload = items.first().toObject();
        return result;
    }
    return pageResult(items, page);
}

ServiceResult AdminService::users(qint64, quint32, const QJsonObject &payload, ChargingService *)
{
    UserRepository repository(database());
    QString error;
    if (payload.value("action") == "user.list")
    {
        Pagination page;
        if (!pagination(payload, &page) || !textFields(payload, {"phone", "phoneKeyword"}))
            return invalid();
        const auto phone =
            payload.value("phone").toString(payload.value("phoneKeyword").toString()).trimmed();
        QList<UserRecord> users;
        int total = 0;
        if (!repository.search(phone, page.pageSize, page.offset, &users, &error) ||
            !repository.count(phone, &total, &error))
            return databaseError();
        QJsonArray items;
        for (const auto &user : users)
            items.append(userJson(user));
        ServiceResult result;
        result.payload = {{"items", items},
                          {"page", page.page},
                          {"pageSize", page.pageSize},
                          {"total", total},
                          {"totalPages", qMax(1, (total + page.pageSize - 1) / page.pageSize)}};
        return result;
    }
    if (!identifier(payload.value("userId")))
        return invalid();
    UserRecord user;
    if (!repository.findById(qint64(payload.value("userId").toDouble()), &user, &error))
        return databaseError();
    if (!user.id)
        return missing(QStringLiteral("用户"));
    const QString status = payload.value("action") == "user.freeze" ? "frozen" : "normal";
    if (!repository.setStatus(user.id, status, &error) || !repository.findById(user.id, &user, &error))
        return databaseError();
    ServiceResult result;
    result.payload = userJson(user);
    return result;
}

ServiceResult AdminService::orders(qint64, quint32, const QJsonObject &payload, ChargingService *)
{
    Pagination page;
    QDate from, to;
    if (!pagination(payload, &page) || !textFields(payload, {"status", "phone", "orderNo"}) ||
        !dateFilter(payload, &from, &to))
        return invalid();
    QString status = payload.value("status").toString();
    if (!QStringList{"", "charging", "completed", "fault_stopped", "cancelled"}.contains(status))
        return invalid();
    if (payload.value("action") == "charging.active.list")
        status = "charging";
    QList<OrderRecord> records;
    QString error;
    if (!allOrders(database(), &records, &error))
        return databaseError();
    QJsonArray items;
    for (const auto &order : records)
    {
        const auto date = QDateTime::fromString(order.startedAt, Qt::ISODate).date();
        if ((!status.isEmpty() && order.status != status) || (from.isValid() && date < from) ||
            (to.isValid() && date > to) ||
            !order.orderNo.contains(payload.value("orderNo").toString(), Qt::CaseInsensitive))
            continue;
        UserRecord user;
        if (!UserRepository(database()).findById(order.userId, &user, &error))
            return databaseError();
        if (!user.phone.contains(payload.value("phone").toString()))
            continue;
        PileRecord pile;
        StationRecord station;
        if (!PileRepository(database()).findById(order.pileId, &pile, &error) ||
            !StationRepository(database()).findById(order.stationId, &station, &error))
            return databaseError();
        auto item = OrderService::snapshot(order);
        // Preserve integer values in CSV and JSON even beyond double's exact
        // integer range; never round a qint64 through double and cast it back.
        item.insert("energyWh", order.energyWh);
        item.insert("feeCents", order.feeCents);
        item.insert("phone", user.phone);
        item.insert("nickname", user.nickname);
        item.insert("pileCode", pile.pileCode);
        item.insert("stationName", station.name);
        item.insert("powerKw", pile.powerKw);
        item.insert("durationSeconds", double(order.durationSeconds));
        items.append(item);
    }
    if (payload.value("action") != "report.export")
        return pageResult(items, page);
    QString content =
        QChar(0xfeff) +
        QStringLiteral("orderNo,phone,stationName,pileCode,status,energyWh,feeCents,startedAt,stoppedAt\r\n");
    for (const auto &value : items)
    {
        const auto item = value.toObject();
        QStringList cells;
        for (const auto &key : QStringList{"orderNo", "phone", "stationName", "pileCode", "status",
                                           "energyWh", "feeCents", "startedAt", "stoppedAt"})
            cells.append(csvCell(item.value(key).isDouble() ? QString::number(item.value(key).toInteger())
                                                            : item.value(key).toString()));
        content += cells.join(',') + "\r\n";
        if (content.size() > Charging::MessageHeader::MaxPayloadLength)
            return failure(ErrorCode::RateLimited, QStringLiteral("导出结果过大，请缩小日期或订单筛选范围"),
                           "export_too_large");
    }
    const auto filename = "orders-" + QDateTime::currentDateTimeUtc().toString("yyyyMMdd-HHmmss") + ".csv";
    ServiceResult result;
    result.payload = {{"filename", filename},
                      {"fileName", filename},
                      {"content", content},
                      {"encoding", "UTF-8"},
                      {"mimeType", "text/csv; charset=utf-8"},
                      {"total", items.size()}};
    return result;
}

ServiceResult AdminService::alarms(qint64 adminId, quint32, const QJsonObject &payload, ChargingService *)
{
    AlarmRepository repository(database());
    const auto action = payload.value("action").toString();
    QString error;
    if (!textFields(payload, {"status", "severity"}))
        return invalid();
    const auto status = payload.value("status").toString();
    const auto severity = payload.value("severity").toString();
    if (!QStringList{"", "open", "acknowledged", "resolved"}.contains(status) ||
        !QStringList{"", "info", "warning", "critical"}.contains(severity))
        return invalid();
    QList<AlarmRecord> records;
    Pagination page;
    if (action == "alarm.list")
    {
        if (!pagination(payload, &page))
            return invalid();
        if (!repository.visitSnapshot(
                status,
                [&](const AlarmRecord &alarm)
                {
                    records.append(alarm);
                    return true;
                },
                &error))
            return databaseError();
    }
    else
    {
        if (!identifier(payload.value("alarmId")))
            return invalid();
        if (action == "alarm.handle" && !QStringList{"acknowledged", "resolved"}.contains(status))
            return invalid();
        AlarmRecord alarm;
        if (!repository.findById(qint64(payload.value("alarmId").toDouble()), &alarm, &error))
            return databaseError();
        if (!alarm.id)
            return missing(QStringLiteral("告警"));
        if (action == "alarm.handle" && alarm.status != status)
        {
            if (alarm.status == "resolved")
                return commandConflict();
            if (!repository.updateStatus(alarm.id, status, adminId, &error) ||
                !repository.findById(alarm.id, &alarm, &error))
                return databaseError();
        }
        records.append(alarm);
    }
    QJsonArray items;
    for (const auto &alarm : records)
    {
        if (action == "alarm.list" && !severity.isEmpty() && alarm.severity != severity)
            continue;
        auto item = alarmJson(alarm);
        PileRecord pile;
        OrderRecord order;
        if ((alarm.pileId && !PileRepository(database()).findById(alarm.pileId, &pile, &error)) ||
            (alarm.orderId && !OrderRepository(database()).findById(alarm.orderId, &order, &error)))
            return databaseError();
        item.insert("pileCode", pile.pileCode);
        item.insert("orderNo", order.orderNo);
        items.append(item);
    }
    if (action == "alarm.list")
        return pageResult(items, page);
    ServiceResult result;
    result.payload = items.first().toObject();
    return result;
}

ServiceResult AdminService::control(qint64 adminId, quint32 requestId, const QJsonObject &payload,
                                    ChargingService *charging)
{
    // Different dispatcher workers share one control lock, including audit/replay.
    // ChargingService separately serializes sampling and settlement.
    static QMutex mutex;
    QMutexLocker lock(&mutex);
    if (!ready(database()))
        return databaseError();
    const auto action = payload.value("action").toString();
    ControlRecordRepository audit(database());
    QString error;
    qint64 pileId = identifier(payload.value("pileId")) ? qint64(payload.value("pileId").toDouble()) : 0;
    qint64 orderId = identifier(payload.value("orderId")) ? qint64(payload.value("orderId").toDouble()) : 0;
    PileRecord pile;
    OrderRecord order;
    ServiceResult result;
    ControlAttemptRecord previous;
    if (!audit.findAttempt(adminId, requestId, action, pileId, orderId,
                           payload.value("_requestScope").toString(), &previous, &error))
        return databaseError();
    bool replay = false;
    if (previous.id)
    {
        // Resolve the original durable attempt before inspecting today's pile
        // state. A pending stop is permanently bound to its original order.
        pileId = qint64(previous.detail.value("pileId").toDouble());
        orderId = qint64(previous.detail.value("orderId").toDouble());
        if (previous.result == "success" || !previous.detail.value("eligible").toBool(orderId > 0))
        {
            result.error = ErrorCode(previous.detail.value("statusCode").toInt());
            result.message = previous.detail.value("message").toString();
            result.reason = previous.detail.value("reason").toString();
            result.payload = previous.detail.value("payload").toObject();
            replay = true;
        }
    }
    if (!previous.id)
    {
        if (action == "pile.invalid" || (payload.contains("pileId") && !pileId) ||
            (payload.contains("orderId") && !orderId) || (!pileId && !(action == "pile.stop" && orderId)))
            result = invalid();
        if (result.succeeded() && orderId)
        {
            if (!OrderRepository(database()).findById(orderId, &order, &error))
                result = databaseError();
            else if (!order.id)
                result = missing(QStringLiteral("订单"));
            else if (pileId && order.pileId != pileId)
                result = commandConflict();
            else
                pileId = order.pileId;
        }
        if (result.succeeded())
        {
            if (!PileRepository(database()).findById(pileId, &pile, &error))
                result = databaseError();
            else if (!pile.id)
                result = missing(QStringLiteral("电桩"));
        }
        if (result.succeeded() && action == "pile.stop" && !orderId)
        {
            QList<OrderRecord> active;
            if (!OrderRepository(database()).listActive(&active, &error))
                result = databaseError();
            else
            {
                for (const auto &candidate : active)
                    if (candidate.pileId == pileId)
                    {
                        order = candidate;
                        orderId = order.id;
                        break;
                    }
                if (!orderId)
                    result =
                        failure(ErrorCode::Conflict, QStringLiteral("电桩没有活动订单"), "order_not_active");
            }
        }
    }
    QJsonObject detail{{"request", payload},
                       {"adminId", double(adminId)},
                       {"pileId", double(pileId)},
                       {"orderId", double(orderId)},
                       {"requestId", double(requestId)}};
    detail.insert("eligible", result.succeeded());
    auto rememberOutcome = [&]
    {
        detail.insert("payload", result.payload);
        detail.insert("statusCode", int(result.error));
        detail.insert("reason", result.reason);
        detail.insert("message", result.message);
    };
    rememberOutcome();
    auto encode = [](const QJsonObject &object)
    { return QString::fromUtf8(QJsonDocument(object).toJson(QJsonDocument::Compact)); };
    qint64 recordId = 0;
    // Record the attempt before touching the device or settling funds. Invalid
    // target IDs are retained in detail while foreign keys remain nullable.
    if (!audit.insert(adminId, pileId, orderId, action, requestId, "pending", encode(detail), &recordId,
                      &error))
        return databaseError();
    bool finalized = false;
    if (!replay && result.succeeded() && action == "pile.stop")
    {
        if (!charging)
            result = failure(ErrorCode::InternalError, QStringLiteral("充电服务不可用"));
        else
            result = charging->stop(adminId, Charging::Role::Administrator, {{"orderId", double(orderId)}});
    }
    else if (!replay && result.succeeded())
    {
        QString reason;
        const auto persistResult = [&](const PileRecord &savedPile, QString *auditError)
        {
            result.payload = pileJson(savedPile);
            rememberOutcome();
            return audit.finish(recordId, "success", encode(detail), auditError);
        };
        if (!audit.applyPileCommand(pileId, action, persistResult, &reason, &error))
            result = databaseError();
        else if (reason == "not_found")
            result = missing(QStringLiteral("电桩"));
        else if (!reason.isEmpty())
            result = commandConflict();
        else
            finalized = true;
    }
    if (finalized)
        return result;
    rememberOutcome();
    if (!audit.finish(recordId, result.succeeded() ? "success" : "failure", encode(detail), &error))
    {
        // Preserve the exact result as an append-only recovery record when an
        // UPDATE is unavailable. If this insert also fails, the original pending
        // record still binds a stop to the settled order and its payment ledger.
        detail.insert("recoveryOf", double(recordId));
        audit.insert(adminId, pileId, orderId, action, requestId, result.succeeded() ? "success" : "failure",
                     encode(detail), nullptr, &error);
        return databaseError();
    }
    return result;
}

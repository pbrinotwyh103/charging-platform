#include "services/alarmservice.h"
#include "services/servicehelpers.h"
#include "repositories/alarmrepository.h"
#include <QMutex>
#include <QMutexLocker>

using namespace ServiceHelpers;

ServiceResult AlarmService::raiseOnce(qint64 pileId, qint64 orderId, const QString &type,
                                      const QString &severity, const QString &message)
{
    // Serialize check/insert across service instances in this server process.
    static QMutex mutex;
    QMutexLocker lock(&mutex);
    ConnectionCleanup cleanup(database());
    if (pileId <= 0 || orderId <= 0 || type.trimmed().isEmpty()
        || severity.trimmed().isEmpty() || message.trimmed().isEmpty()) return invalid();
    if (!ready(database())) return databaseError();
    AlarmRepository repository(database());
    QString error;
    AlarmRecord record;
    bool created = true;
    for (int offset = 0; ; offset += 200) {
        QList<AlarmRecord> records;
        if (!repository.list({}, 200, offset, &records, &error)) return databaseError();
        for (const auto &candidate : records) {
            if (candidate.orderId == orderId && candidate.alarmType == type && candidate.status != "resolved") {
                record = candidate;
                created = false;
                break;
            }
        }
        if (!created || records.size() < 200) break;
    }
    if (created) {
        record.pileId = pileId;
        record.orderId = orderId;
        record.alarmType = type;
        record.severity = severity;
        record.message = message;
        record.status = "open";
        if (!repository.insert(record, &record.id, &error)) return databaseError();
    }
    ServiceResult result;
    result.payload = {{"alarmId", double(record.id)}, {"pileId", double(record.pileId)},
        {"orderId", double(record.orderId)}, {"alarmType", record.alarmType},
        {"severity", record.severity}, {"message", record.message}, {"status", record.status},
        {"created", created}};
    return result;
}

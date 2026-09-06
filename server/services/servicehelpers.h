#pragma once

#include "database/databasemanager.h"
#include "services/serviceresult.h"
#include <cmath>
#include <limits>

namespace ServiceHelpers {
inline ServiceResult failure(Charging::ErrorCode code, const QString &message,
                             const QString &reason = {})
{
    ServiceResult result;
    result.error = code;
    result.message = message;
    result.reason = reason;
    return result;
}

inline ServiceResult invalid()
{
    return failure(Charging::ErrorCode::ValidationFailed, QStringLiteral("请求参数无效"));
}

inline ServiceResult databaseError()
{
    return failure(Charging::ErrorCode::DatabaseError, QStringLiteral("数据处理失败"));
}

inline bool integer(const QJsonValue &value, double minimum, double maximum)
{
    return value.isDouble() && std::isfinite(value.toDouble())
        && value.toDouble() >= minimum && value.toDouble() <= maximum
        && std::floor(value.toDouble()) == value.toDouble();
}

inline bool identifier(const QJsonValue &value)
{
    return integer(value, 1, 9007199254740991.0);
}

struct Pagination {
    int page = 1;
    int pageSize = 20;
    int offset = 0;
};

inline bool pagination(const QJsonObject &payload, Pagination *result)
{
    if ((payload.contains("page") && !integer(payload.value("page"), 1, std::numeric_limits<int>::max()))
        || (payload.contains("pageSize") && !integer(payload.value("pageSize"), 1, 100)))
        return false;
    result->page = payload.value("page").toInt(1);
    result->pageSize = payload.value("pageSize").toInt(20);
    const qint64 offset = qint64(result->page - 1) * result->pageSize;
    if (offset > std::numeric_limits<int>::max()) return false;
    result->offset = int(offset);
    return true;
}

inline bool ready(DatabaseManager *database)
{
    return database && !database->databasePath().isEmpty();
}

class ConnectionCleanup final
{
public:
    explicit ConnectionCleanup(DatabaseManager *database) : m_database(database) {}
    ~ConnectionCleanup() { if (m_database) m_database->releaseCurrentThreadConnection(); }
private:
    DatabaseManager *m_database;
};
}

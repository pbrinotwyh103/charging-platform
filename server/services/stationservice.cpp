#include "services/stationservice.h"

#include "database/databasemanager.h"
#include "repositories/stationrepository.h"

#include <QDebug>

namespace {

class WorkerConnectionCleanup final
{
public:
    explicit WorkerConnectionCleanup(DatabaseManager *database) : m_database(database) {}
    ~WorkerConnectionCleanup() { m_database->releaseCurrentThreadConnection(); }

private:
    DatabaseManager *m_database;
};

} // namespace

StationListResult StationService::listForUser(qint64 userId, bool favoritesOnly)
{
    WorkerConnectionCleanup cleanup(database());
    StationListResult result;
    QString error;
    StationRepository stations(database());
    if (!stations.listForUser(userId, favoritesOnly, &result.items, &error)) {
        qWarning().noquote() << QStringLiteral("站点列表查询失败：%1").arg(error);
        result.error = Charging::ErrorCode::DatabaseError;
        result.message = QStringLiteral("站点列表读取失败");
    }
    return result;
}

FavoriteToggleResult StationService::setFavorite(qint64 userId, qint64 stationId,
                                                  bool favorited)
{
    WorkerConnectionCleanup cleanup(database());
    FavoriteToggleResult result;
    result.stationId = stationId;
    result.favorited = favorited;
    if (stationId <= 0) {
        result.error = Charging::ErrorCode::ValidationFailed;
        result.message = QStringLiteral("充电站编号无效");
        return result;
    }

    StationRepository stations(database());
    QString error;
    bool found = false;
    if (!stations.exists(stationId, &found, &error)) {
        qWarning().noquote() << QStringLiteral("收藏站点校验失败：%1").arg(error);
        result.error = Charging::ErrorCode::DatabaseError;
        result.message = QStringLiteral("收藏状态更新失败");
        return result;
    }
    if (!found) {
        result.error = Charging::ErrorCode::NotFound;
        result.message = QStringLiteral("充电站不存在");
        return result;
    }
    if (!stations.setFavorite(userId, stationId, favorited,
                              &result.updatedAt, &error)) {
        qWarning().noquote() << QStringLiteral("收藏状态写入失败：%1").arg(error);
        result.error = Charging::ErrorCode::DatabaseError;
        result.message = QStringLiteral("收藏状态更新失败");
    }
    return result;
}

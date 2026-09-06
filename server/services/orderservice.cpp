#include "services/orderservice.h"

#include "database/databasemanager.h"
#include "repositories/orderrepository.h"

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

OrderHistoryResult OrderService::historyForUser(qint64 userId)
{
    WorkerConnectionCleanup cleanup(database());
    OrderHistoryResult result;
    QString error;
    OrderRepository orders(database());
    if (!orders.findHistoryByUser(userId, &result.items, &error)) {
        qWarning().noquote()
            << QStringLiteral("历史订单查询数据库错误：%1").arg(error);
        result.error = Charging::ErrorCode::DatabaseError;
        result.message = QStringLiteral("历史订单读取失败");
    }
    return result;
}

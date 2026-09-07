#include "services/userservice.h"

#include "database/databasemanager.h"
#include "repositories/userrepository.h"
#include "repositories/walletrepository.h"

#include <QJsonArray>
#include <QRegularExpression>

#include <cmath>
#include <limits>

namespace {

class WorkerConnectionCleanup final
{
public:
    explicit WorkerConnectionCleanup(DatabaseManager *database)
        : m_database(database) {}
    ~WorkerConnectionCleanup()
    {
        if (m_database) m_database->releaseCurrentThreadConnection();
    }

private:
    DatabaseManager *m_database = nullptr;
};

bool integerInRange(const QJsonValue &value, double minimum, double maximum)
{
    return value.isDouble() && std::isfinite(value.toDouble())
        && value.toDouble() >= minimum && value.toDouble() <= maximum
        && std::floor(value.toDouble()) == value.toDouble();
}

UserServiceResult failure(Charging::ErrorCode error, const QString &message)
{
    UserServiceResult result;
    result.error = error;
    result.message = message;
    return result;
}

QJsonObject walletItem(const WalletRecord &record)
{
    return {{QStringLiteral("recordId"), static_cast<double>(record.id)},
            {QStringLiteral("transactionId"), record.recordNo},
            {QStringLiteral("recordType"), record.recordType},
            {QStringLiteral("amountCents"),
             static_cast<double>(record.amountCents)},
            {QStringLiteral("balanceAfterCents"),
             static_cast<double>(record.balanceAfterCents)},
            {QStringLiteral("status"), record.status},
            {QStringLiteral("createdAt"), record.createdAt}};
}

} // namespace

UserServiceResult UserService::updateProfile(qint64 userId,
                                             const QJsonObject &payload)
{
    WorkerConnectionCleanup cleanup(database());
    if (userId <= 0)
        return failure(Charging::ErrorCode::Unauthorized,
                       QStringLiteral("用户登录状态无效"));
    if (payload.contains(QStringLiteral("avatarBase64")))
        return failure(Charging::ErrorCode::UnsupportedMessage,
                       QStringLiteral("头像上传服务尚未开放"));

    const QJsonValue nicknameValue = payload.value(QStringLiteral("nickname"));
    if (!nicknameValue.isString())
        return failure(Charging::ErrorCode::InvalidPayload,
                       QStringLiteral("昵称参数格式错误"));

    const QString nickname = nicknameValue.toString().trimmed();
    if (nickname.size() < 2 || nickname.size() > 20)
        return failure(Charging::ErrorCode::ValidationFailed,
                       QStringLiteral("昵称长度需为 2—20 个字符"));
    static const QRegularExpression illegalCharacters(
        QStringLiteral("[\\x{0000}-\\x{001f}\\x{007f}/\\\\:*?\"<>|]"));
    if (nickname.contains(illegalCharacters))
        return failure(Charging::ErrorCode::ValidationFailed,
                       QStringLiteral("昵称不能包含控制字符或 / \\ : * ? \" < > |"));
    if (!database())
        return failure(Charging::ErrorCode::DatabaseError,
                       QStringLiteral("用户资料更新失败"));

    UserRepository users(database());
    UserRecord user;
    QString error;
    if (!users.findById(userId, &user, &error))
        return failure(Charging::ErrorCode::DatabaseError,
                       QStringLiteral("用户资料读取失败"));
    if (user.id == 0)
        return failure(Charging::ErrorCode::NotFound,
                       QStringLiteral("用户不存在"));
    if (user.status != QStringLiteral("normal"))
        return failure(Charging::ErrorCode::AccountDisabled,
                       QStringLiteral("用户已冻结"));
    if (user.nickname == nickname)
        return failure(Charging::ErrorCode::Conflict,
                       QStringLiteral("新昵称与当前昵称相同"));
    if (!users.updateNickname(userId, nickname, &error)
        || !users.findById(userId, &user, &error)) {
        return failure(Charging::ErrorCode::DatabaseError,
                       QStringLiteral("用户资料更新失败"));
    }
    if (user.id == 0)
        return failure(Charging::ErrorCode::NotFound,
                       QStringLiteral("用户不存在"));

    UserServiceResult result;
    result.payload = {
        {QStringLiteral("userId"), static_cast<double>(user.id)},
        {QStringLiteral("phone"), user.phone},
        {QStringLiteral("nickname"), user.nickname},
        {QStringLiteral("avatarPath"), user.avatarPath},
        {QStringLiteral("balanceCents"), static_cast<double>(user.balanceCents)},
        {QStringLiteral("status"), user.status},
        {QStringLiteral("role"), QStringLiteral("user")}
    };
    return result;
}

UserServiceResult UserService::recharge(qint64 userId,
                                        const QJsonObject &payload)
{
    WorkerConnectionCleanup cleanup(database());
    const QJsonValue transaction =
        payload.value(QStringLiteral("transactionId"));
    if (userId <= 0
        || !integerInRange(payload.value(QStringLiteral("amountCents")),
                           100, 1'000'000)
        || !transaction.isString()
        || transaction.toString().trimmed().isEmpty()
        || transaction.toString().size() > 128) {
        return failure(Charging::ErrorCode::ValidationFailed,
                       QStringLiteral("充值金额或交易号无效"));
    }
    if (!database())
        return failure(Charging::ErrorCode::DatabaseError,
                       QStringLiteral("充值数据处理失败"));

    const QString transactionId = transaction.toString().trimmed();
    const qint64 amountCents = static_cast<qint64>(
        payload.value(QStringLiteral("amountCents")).toDouble());
    WalletRepository wallet(database());
    WalletRecord record;
    QString error;
    if (!wallet.findByRecordNo(transactionId, QStringLiteral("recharge"),
                               &record, &error)) {
        return failure(Charging::ErrorCode::DatabaseError,
                       QStringLiteral("充值记录读取失败"));
    }
    if (record.id != 0
        && (record.userId != userId || record.amountCents != amountCents)) {
        return failure(Charging::ErrorCode::Conflict,
                       QStringLiteral("交易号已用于其他交易"));
    }
    if (record.id == 0) {
        UserRecord user;
        if (!UserRepository(database()).findById(userId, &user, &error))
            return failure(Charging::ErrorCode::DatabaseError,
                           QStringLiteral("用户资料读取失败"));
        if (user.id == 0)
            return failure(Charging::ErrorCode::NotFound,
                           QStringLiteral("用户不存在"));
        if (user.status != QStringLiteral("normal"))
            return failure(Charging::ErrorCode::AccountDisabled,
                           QStringLiteral("用户已冻结"));
        qint64 balanceAfterCents = 0;
        if (!wallet.recharge(transactionId, userId, amountCents,
                             &balanceAfterCents, &error)) {
            return failure(Charging::ErrorCode::DatabaseError,
                           QStringLiteral("充值入账失败"));
        }
        if (!wallet.findByRecordNo(transactionId, QStringLiteral("recharge"),
                                   &record, &error)) {
            return failure(Charging::ErrorCode::DatabaseError,
                           QStringLiteral("充值记录读取失败"));
        }
    }

    UserServiceResult result;
    result.payload = walletItem(record);
    result.payload.insert(QStringLiteral("balanceCents"),
                          static_cast<double>(record.balanceAfterCents));
    return result;
}

UserServiceResult UserService::walletLedger(qint64 userId,
                                            const QJsonObject &payload)
{
    WorkerConnectionCleanup cleanup(database());
    if (!database())
        return failure(Charging::ErrorCode::DatabaseError,
                       QStringLiteral("充值记录读取失败"));
    if (userId <= 0
        || (payload.contains(QStringLiteral("page"))
            && !integerInRange(payload.value(QStringLiteral("page")), 1,
                               std::numeric_limits<int>::max()))
        || (payload.contains(QStringLiteral("pageSize"))
            && !integerInRange(payload.value(QStringLiteral("pageSize")),
                               1, 100))) {
        return failure(Charging::ErrorCode::ValidationFailed,
                       QStringLiteral("分页参数无效"));
    }
    const int page = payload.value(QStringLiteral("page")).toInt(1);
    const int pageSize =
        payload.value(QStringLiteral("pageSize")).toInt(20);
    const qint64 offsetValue = qint64(page - 1) * pageSize;
    if (offsetValue > std::numeric_limits<int>::max())
        return failure(Charging::ErrorCode::ValidationFailed,
                       QStringLiteral("分页参数无效"));

    WalletRepository wallet(database());
    QList<WalletRecord> records;
    int total = 0;
    QString error;
    if (!wallet.countByUser(userId, &total, &error)
        || !wallet.listByUser(userId, pageSize,
                              static_cast<int>(offsetValue),
                              &records, &error)) {
        return failure(Charging::ErrorCode::DatabaseError,
                       QStringLiteral("充值记录读取失败"));
    }
    QJsonArray items;
    for (const WalletRecord &record : records)
        items.append(walletItem(record));

    UserServiceResult result;
    result.payload = {{QStringLiteral("items"), items},
                      {QStringLiteral("page"), page},
                      {QStringLiteral("pageSize"), pageSize},
                      {QStringLiteral("total"), total}};
    return result;
}

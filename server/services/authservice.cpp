#include "services/authservice.h"

#include "database/databasemanager.h"
#include "repositories/adminrepository.h"
#include "repositories/userrepository.h"
#include "security/passwordhasher.h"

#include <QDebug>
#include <QRegularExpression>

namespace {

class WorkerConnectionCleanup final
{
public:
    explicit WorkerConnectionCleanup(DatabaseManager *database) : m_database(database) {}
    ~WorkerConnectionCleanup() { m_database->releaseCurrentThreadConnection(); }
private:
    DatabaseManager *m_database;
};

AuthResult databaseFailure(const QString &detail)
{
    qWarning().noquote() << QStringLiteral("认证数据库错误：%1").arg(detail);
    AuthResult result;
    result.error = Charging::ErrorCode::DatabaseError;
    result.message = QStringLiteral("认证数据处理失败");
    return result;
}

AuthResult failure(Charging::ErrorCode error, const QString &message)
{
    AuthResult result;
    result.error = error;
    result.message = message;
    return result;
}

} // namespace

bool AuthService::initialize(QString *error)
{
    AdminRepository admins(database());
    AdminRecord admin;
    if (!admins.findByUsername(QStringLiteral("admin"), &admin, error)) {
        return false;
    }
    if (admin.id != 0) {
        return true;
    }

    const QString salt = PasswordHasher::generateSalt();
    const QString passwordHash = PasswordHasher::hash(QStringLiteral("123456"), salt);
    return admins.insert(QStringLiteral("admin"), passwordHash, salt,
                         QStringLiteral("all"), error);
}

AuthResult AuthService::loginUser(const QString &phone)
{
    WorkerConnectionCleanup cleanup(database());
    static const QRegularExpression phonePattern(QStringLiteral("^1\\d{10}$"));
    if (!phonePattern.match(phone).hasMatch()) {
        return failure(Charging::ErrorCode::ValidationFailed,
                       QStringLiteral("请输入11位中国大陆手机号"));
    }

    UserRepository users(database());
    UserRecord user;
    bool created = false;
    QString error;
    if (!users.findOrCreate(phone, &user, &created, &error)) {
        return databaseFailure(error);
    }
    if (user.status != QStringLiteral("normal")) {
        return failure(Charging::ErrorCode::AccountDisabled,
                       QStringLiteral("该用户账号已冻结"));
    }

    AuthResult result;
    result.role = Charging::Role::User;
    result.principalId = user.id;
    result.identity = user.phone;
    result.payload = userPayload(user, created);
    return result;
}

AuthResult AuthService::loginAdmin(const QString &username, const QString &password)
{
    WorkerConnectionCleanup cleanup(database());
    static const QRegularExpression usernamePattern(QStringLiteral("^[A-Za-z0-9_.-]{3,32}$"));
    if (!usernamePattern.match(username).hasMatch() || password.isEmpty()
        || password.size() > 128) {
        return failure(Charging::ErrorCode::ValidationFailed,
                       QStringLiteral("请输入有效的管理员账号和密码"));
    }

    AdminRepository admins(database());
    AdminRecord admin;
    QString error;
    if (!admins.findByUsername(username, &admin, &error)) {
        return databaseFailure(error);
    }
    if (admin.id == 0
        || !PasswordHasher::verify(password, admin.passwordSalt, admin.passwordHash)) {
        return failure(Charging::ErrorCode::InvalidCredentials,
                       QStringLiteral("账号或密码错误"));
    }
    if (admin.status != QStringLiteral("normal")) {
        return failure(Charging::ErrorCode::AccountDisabled,
                       QStringLiteral("管理员账号已停用"));
    }
    if (!admins.updateLastLogin(admin.id, &error)) {
        return databaseFailure(error);
    }

    AuthResult result;
    result.role = Charging::Role::Administrator;
    result.principalId = admin.id;
    result.identity = admin.username;
    result.payload = {
        {QStringLiteral("adminId"), static_cast<double>(admin.id)},
        {QStringLiteral("username"), admin.username},
        {QStringLiteral("permissions"), admin.permissions},
        {QStringLiteral("role"), QStringLiteral("administrator")}
    };
    return result;
}

AuthResult AuthService::userProfile(qint64 userId)
{
    WorkerConnectionCleanup cleanup(database());
    UserRepository users(database());
    UserRecord user;
    QString error;
    if (!users.findById(userId, &user, &error)) {
        return databaseFailure(error);
    }
    if (user.id == 0) {
        return failure(Charging::ErrorCode::NotFound,
                       QStringLiteral("用户资料不存在"));
    }
    AuthResult result;
    result.role = Charging::Role::User;
    result.principalId = user.id;
    result.identity = user.phone;
    result.payload = userPayload(user, false);
    return result;
}

QJsonObject AuthService::userPayload(const UserRecord &user, bool created)
{
    return {
        {QStringLiteral("userId"), static_cast<double>(user.id)},
        {QStringLiteral("phone"), user.phone},
        {QStringLiteral("nickname"), user.nickname},
        {QStringLiteral("avatarPath"), user.avatarPath},
        {QStringLiteral("balanceCents"), static_cast<double>(user.balanceCents)},
        {QStringLiteral("status"), user.status},
        {QStringLiteral("created"), created},
        {QStringLiteral("role"), QStringLiteral("user")}
    };
}

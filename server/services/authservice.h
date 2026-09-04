#pragma once

#include "models/role.h"
#include "protocol/errorcodes.h"
#include "services/servicebase.h"

#include <QJsonObject>
#include <QString>

struct AuthResult {
    Charging::ErrorCode error = Charging::ErrorCode::Success;
    QString message;
    Charging::Role role = Charging::Role::Anonymous;
    qint64 principalId = 0;
    QString identity;
    QJsonObject payload;

    bool succeeded() const { return error == Charging::ErrorCode::Success; }
};

class AuthService final : public ServiceBase
{
public:
    using ServiceBase::ServiceBase;

    bool initialize(QString *error);
    AuthResult loginUser(const QString &phone);
    AuthResult loginAdmin(const QString &username, const QString &password);
    AuthResult userProfile(qint64 userId);

private:
    static QJsonObject userPayload(const struct UserRecord &user, bool created);
};

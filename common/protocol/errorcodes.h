#pragma once

#include <QtGlobal>
#include <QString>

namespace Charging {

enum class ErrorCode : qint32 {
    Success = 0,
    InvalidPacket = 1001,
    UnsupportedVersion = 1002,
    UnsupportedMessage = 1003,
    InvalidPayload = 1004,
    Unauthorized = 1101,
    Forbidden = 1102,
    SessionExpired = 1103,
    InvalidCredentials = 1104,
    AccountDisabled = 1105,
    ValidationFailed = 1201,
    Conflict = 1202,
    NotFound = 1203,
    DuplicateRequest = 1204,
    RateLimited = 1205,
    DatabaseError = 2001,
    NetworkUnavailable = 3001,
    RequestTimeout = 3002,
    InternalError = 9000
};

QString errorMessage(ErrorCode code, const QString &detail = {});

} // namespace Charging

#pragma once

#include <QtGlobal>

namespace Charging {

enum class ErrorCode : qint32 {
    Success = 0,
    InvalidPacket = 1001,
    UnsupportedVersion = 1002,
    UnsupportedMessage = 1003,
    InvalidPayload = 1004,
    Unauthorized = 1101,
    Forbidden = 1102,
    ValidationFailed = 1201,
    Conflict = 1202,
    NotFound = 1203,
    DatabaseError = 2001,
    InternalError = 9000
};

} // namespace Charging

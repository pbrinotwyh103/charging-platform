#pragma once

#include "protocol/errorcodes.h"
#include "protocol/messagetypes.h"

#include <QJsonObject>
#include <QtGlobal>

namespace Charging {

struct MessageHeader {
    static constexpr quint32 Magic = 0x43485031; // ASCII: CHP1
    static constexpr quint16 Version = 1;
    static constexpr quint32 MaxPayloadLength = 4 * 1024 * 1024;
    static constexpr qsizetype SerializedSize = 20;

    quint32 magic = Magic;
    quint16 version = Version;
    MessageType messageType = MessageType::Invalid;
    quint32 requestId = 0;
    quint32 payloadLength = 0;
    ErrorCode statusCode = ErrorCode::Success;
};

struct Message {
    MessageHeader header;
    QJsonObject payload;
};

} // namespace Charging

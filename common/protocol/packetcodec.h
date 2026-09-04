#pragma once

#include "protocol/message.h"

#include <QByteArray>

namespace Charging {

enum class DecodeStatus {
    NeedMoreData,
    Complete,
    Invalid
};

struct DecodeResult {
    DecodeStatus status = DecodeStatus::NeedMoreData;
    Message message;
    QString error;
};

class PacketCodec final
{
public:
    static QByteArray encode(MessageType type,
                             quint32 requestId,
                             const QJsonObject &payload = {},
                             ErrorCode status = ErrorCode::Success);

    static DecodeResult tryDecode(QByteArray &buffer);
};

} // namespace Charging

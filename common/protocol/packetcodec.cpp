#include "protocol/packetcodec.h"

#include <QBuffer>
#include <QDataStream>
#include <QJsonDocument>
#include <QJsonParseError>

namespace Charging {

QByteArray PacketCodec::encode(MessageType type,
                               quint32 requestId,
                               const QJsonObject &payload,
                               ErrorCode status)
{
    const QByteArray payloadBytes = QJsonDocument(payload).toJson(QJsonDocument::Compact);
    QByteArray packet;
    packet.reserve(MessageHeader::SerializedSize + payloadBytes.size());

    QDataStream stream(&packet, QIODevice::WriteOnly);
    stream.setVersion(QDataStream::Qt_6_2);
    stream.setByteOrder(QDataStream::BigEndian);
    stream << MessageHeader::Magic
           << MessageHeader::Version
           << static_cast<quint16>(type)
           << requestId
           << static_cast<quint32>(payloadBytes.size())
           << static_cast<qint32>(status);
    packet.append(payloadBytes);
    return packet;
}

DecodeResult PacketCodec::tryDecode(QByteArray &buffer)
{
    DecodeResult result;
    if (buffer.size() < MessageHeader::SerializedSize) {
        return result;
    }

    QBuffer device(&buffer);
    device.open(QIODevice::ReadOnly);
    QDataStream stream(&device);
    stream.setVersion(QDataStream::Qt_6_2);
    stream.setByteOrder(QDataStream::BigEndian);

    quint16 rawType = 0;
    qint32 rawStatus = 0;
    stream >> result.message.header.magic
           >> result.message.header.version
           >> rawType
           >> result.message.header.requestId
           >> result.message.header.payloadLength
           >> rawStatus;
    result.message.header.messageType = static_cast<MessageType>(rawType);
    result.message.header.statusCode = static_cast<ErrorCode>(rawStatus);

    if (result.message.header.magic != MessageHeader::Magic) {
        result.status = DecodeStatus::Invalid;
        result.error = QStringLiteral("消息魔数错误");
        buffer.clear();
        return result;
    }
    if (result.message.header.version != MessageHeader::Version) {
        result.status = DecodeStatus::Invalid;
        result.error = QStringLiteral("不支持的协议版本");
        buffer.clear();
        return result;
    }
    if (result.message.header.payloadLength > MessageHeader::MaxPayloadLength) {
        result.status = DecodeStatus::Invalid;
        result.error = QStringLiteral("消息体长度超过限制");
        buffer.clear();
        return result;
    }

    const qsizetype packetSize = MessageHeader::SerializedSize
        + static_cast<qsizetype>(result.message.header.payloadLength);
    if (buffer.size() < packetSize) {
        return result;
    }

    const QByteArray payloadBytes = buffer.mid(MessageHeader::SerializedSize,
                                               result.message.header.payloadLength);
    if (!payloadBytes.isEmpty()) {
        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(payloadBytes, &parseError);
        if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
            result.status = DecodeStatus::Invalid;
            result.error = QStringLiteral("JSON消息体格式错误");
            buffer.remove(0, packetSize);
            return result;
        }
        result.message.payload = document.object();
    }

    buffer.remove(0, packetSize);
    result.status = DecodeStatus::Complete;
    return result;
}

} // namespace Charging

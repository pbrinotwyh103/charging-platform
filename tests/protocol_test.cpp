#include "protocol/packetcodec.h"

#include <QJsonObject>
#include <QtTest>

class ProtocolTest final : public QObject
{
    Q_OBJECT

private slots:
    void roundTrip();
    void splitPacket();
    void stickyPackets();
    void invalidMagic();
    void unsupportedVersion();
    void oversizedPayload();
    void invalidJsonPayload();
    void serverCoreMessageIds();
};

void ProtocolTest::roundTrip()
{
    QByteArray buffer = Charging::PacketCodec::encode(
        Charging::MessageType::UserLoginRequest,
        42,
        {{QStringLiteral("phone"), QStringLiteral("13800138000")}});

    const Charging::DecodeResult result = Charging::PacketCodec::tryDecode(buffer);
    QCOMPARE(result.status, Charging::DecodeStatus::Complete);
    QCOMPARE(result.message.header.requestId, quint32(42));
    QCOMPARE(result.message.header.messageType, Charging::MessageType::UserLoginRequest);
    QCOMPARE(result.message.payload.value(QStringLiteral("phone")).toString(),
             QStringLiteral("13800138000"));
    QVERIFY(buffer.isEmpty());
}

void ProtocolTest::splitPacket()
{
    const QByteArray packet = Charging::PacketCodec::encode(
        Charging::MessageType::Ping, 7, {{QStringLiteral("client"), QStringLiteral("user")}});
    QByteArray buffer = packet.left(8);
    QCOMPARE(Charging::PacketCodec::tryDecode(buffer).status,
             Charging::DecodeStatus::NeedMoreData);
    buffer.append(packet.mid(8));
    QCOMPARE(Charging::PacketCodec::tryDecode(buffer).status,
             Charging::DecodeStatus::Complete);
}

void ProtocolTest::stickyPackets()
{
    QByteArray buffer = Charging::PacketCodec::encode(Charging::MessageType::Ping, 1)
        + Charging::PacketCodec::encode(Charging::MessageType::Ping, 2);
    QCOMPARE(Charging::PacketCodec::tryDecode(buffer).message.header.requestId, quint32(1));
    QCOMPARE(Charging::PacketCodec::tryDecode(buffer).message.header.requestId, quint32(2));
    QVERIFY(buffer.isEmpty());
}

void ProtocolTest::invalidMagic()
{
    QByteArray buffer = Charging::PacketCodec::encode(Charging::MessageType::Ping, 1);
    buffer[0] = '\0';
    const Charging::DecodeResult result = Charging::PacketCodec::tryDecode(buffer);
    QCOMPARE(result.status, Charging::DecodeStatus::Invalid);
    QVERIFY(!result.error.isEmpty());
}

void ProtocolTest::unsupportedVersion()
{
    QByteArray buffer = Charging::PacketCodec::encode(Charging::MessageType::Ping, 1);
    buffer[4] = '\0';
    buffer[5] = '\2';
    const Charging::DecodeResult result = Charging::PacketCodec::tryDecode(buffer);
    QCOMPARE(result.status, Charging::DecodeStatus::Invalid);
    QVERIFY(result.error.contains(QStringLiteral("版本")));
}

void ProtocolTest::oversizedPayload()
{
    QByteArray buffer = Charging::PacketCodec::encode(Charging::MessageType::Ping, 1);
    for (int i = 12; i < 16; ++i) buffer[i] = static_cast<char>(0xff);
    const Charging::DecodeResult result = Charging::PacketCodec::tryDecode(buffer);
    QCOMPARE(result.status, Charging::DecodeStatus::Invalid);
    QVERIFY(result.error.contains(QStringLiteral("长度")));
}

void ProtocolTest::invalidJsonPayload()
{
    QByteArray buffer = Charging::PacketCodec::encode(
        Charging::MessageType::Ping, 1, {{QStringLiteral("ok"), true}});
    buffer[Charging::MessageHeader::SerializedSize] = '[';
    const Charging::DecodeResult result = Charging::PacketCodec::tryDecode(buffer);
    QCOMPARE(result.status, Charging::DecodeStatus::Invalid);
    QVERIFY(result.error.contains(QStringLiteral("JSON")));
}

void ProtocolTest::serverCoreMessageIds()
{
    using Charging::MessageType;
    QCOMPARE(static_cast<quint16>(MessageType::UserProfileUpdateRequest),
             quint16(1020));
    QCOMPARE(static_cast<quint16>(MessageType::WalletRechargeRequest),
             quint16(1100));
    QCOMPARE(static_cast<quint16>(MessageType::WalletLedgerRequest),
             quint16(1110));
    QCOMPARE(static_cast<quint16>(MessageType::ReservationCreateRequest),
             quint16(3001));
    QCOMPARE(static_cast<quint16>(MessageType::ChargingStartRequest),
             quint16(3010));
    QCOMPARE(static_cast<quint16>(MessageType::ChargingStopRequest),
             quint16(3020));
    QCOMPARE(static_cast<quint16>(MessageType::ActiveOrderRequest),
             quint16(3030));
    QCOMPARE(static_cast<quint16>(MessageType::ReservationCancelRequest),
             quint16(3040));
}

QTEST_APPLESS_MAIN(ProtocolTest)
#include "protocol_test.moc"

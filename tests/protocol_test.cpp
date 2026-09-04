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

QTEST_APPLESS_MAIN(ProtocolTest)
#include "protocol_test.moc"

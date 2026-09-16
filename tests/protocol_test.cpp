#include "protocol/packetcodec.h"
#include "../server/services/serviceresult.h"

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
    void businessProtocolRoundTrip();
    void serviceResultSucceedsOnlyForSuccess();
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

void ProtocolTest::businessProtocolRoundTrip()
{
    QCOMPARE(static_cast<quint16>(Charging::MessageType::WalletRechargeRequest), quint16(1100));
    QCOMPARE(static_cast<quint16>(Charging::MessageType::ReservationCancelResponse), quint16(3041));
    QCOMPARE(static_cast<quint16>(Charging::MessageType::PileCodeLookupRequest), quint16(2030));
    QCOMPARE(static_cast<quint16>(Charging::MessageType::PileCodeLookupResponse), quint16(2031));
    QCOMPARE(static_cast<quint16>(Charging::MessageType::CustomerServiceRequest), quint16(4001));
    QCOMPARE(static_cast<quint16>(Charging::MessageType::CustomerServiceResponse), quint16(4002));
    QCOMPARE(static_cast<quint16>(Charging::MessageType::LoadPredictionRequest), quint16(4100));
    QCOMPARE(static_cast<quint16>(Charging::MessageType::LoadPredictionResponse), quint16(4101));
    QCOMPARE(static_cast<quint16>(Charging::MessageType::StationRecommendationRequest), quint16(4110));
    QCOMPARE(static_cast<quint16>(Charging::MessageType::StationRecommendationResponse), quint16(4111));
    QCOMPARE(static_cast<quint16>(Charging::MessageType::LoadWarningRequest), quint16(5100));
    QCOMPARE(static_cast<quint16>(Charging::MessageType::LoadWarningResponse), quint16(5101));
    QCOMPARE(static_cast<quint16>(Charging::MessageType::AnalyticsStatusRequest), quint16(5110));
    QCOMPARE(static_cast<quint16>(Charging::MessageType::AnalyticsStatusResponse), quint16(5111));
    QCOMPARE(static_cast<quint16>(Charging::MessageType::ModelComparisonRequest), quint16(5120));
    QCOMPARE(static_cast<quint16>(Charging::MessageType::ModelComparisonResponse), quint16(5121));
    QCOMPARE(static_cast<quint16>(Charging::MessageType::ModelDriftRequest), quint16(5130));
    QCOMPARE(static_cast<quint16>(Charging::MessageType::ModelDriftResponse), quint16(5131));
    QCOMPARE(static_cast<quint16>(Charging::MessageType::SchedulingAdviceRequest), quint16(5140));
    QCOMPARE(static_cast<quint16>(Charging::MessageType::SchedulingAdviceResponse), quint16(5141));
    QCOMPARE(static_cast<quint16>(Charging::MessageType::MaintenanceAdviceRequest), quint16(5150));
    QCOMPARE(static_cast<quint16>(Charging::MessageType::MaintenanceAdviceResponse), quint16(5151));
    QCOMPARE(static_cast<quint16>(Charging::MessageType::ExpansionAdviceRequest), quint16(5160));
    QCOMPARE(static_cast<quint16>(Charging::MessageType::ExpansionAdviceResponse), quint16(5161));
    QCOMPARE(static_cast<quint16>(Charging::MessageType::RegulatorSummaryRequest), quint16(5170));
    QCOMPARE(static_cast<quint16>(Charging::MessageType::RegulatorSummaryResponse), quint16(5171));

    QByteArray buffer = Charging::PacketCodec::encode(
        Charging::MessageType::WalletRechargeResponse,
        86,
        {{QStringLiteral("reason"), QStringLiteral("insufficient_balance")}},
        Charging::ErrorCode::Conflict);

    const Charging::DecodeResult result = Charging::PacketCodec::tryDecode(buffer);
    QCOMPARE(result.status, Charging::DecodeStatus::Complete);
    QCOMPARE(result.message.header.requestId, quint32(86));
    QCOMPARE(result.message.header.statusCode, Charging::ErrorCode::Conflict);
    QCOMPARE(result.message.payload.value(QStringLiteral("reason")).toString(),
             QStringLiteral("insufficient_balance"));
    QVERIFY(buffer.isEmpty());
}

void ProtocolTest::serviceResultSucceedsOnlyForSuccess()
{
    ServiceResult result;
    QVERIFY(result.succeeded());

    result.error = Charging::ErrorCode::Conflict;
    QVERIFY(!result.succeeded());
}

QTEST_APPLESS_MAIN(ProtocolTest)
#include "protocol_test.moc"

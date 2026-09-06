#include "pages/homepage.h"
#include "pages/chargingpage.h"
#include "pages/stationdetailpage.h"
#include "pages/profilepage.h"
#include "stores/snapshotstore.h"

#include <QComboBox>
#include <QDateTime>
#include <QInputMethodEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSignalSpy>
#include <QSpinBox>
#include <QtTest>

#include <cmath>

class HomePageTest final : public QObject
{
    Q_OBJECT

private slots:
    void emptyAddressIsRejected();
    void manualAddressIsResolvedBeforeStationQuery();
    void geocodeFailureRestoresControls();
    void missingGeocoderFallsBackToTextSearch();
    void simulatedGpsBypassesWebService();
    void chineseAddressInputIsAccepted();
    void numericOrderIdResetsSnapshotSequence();
    void idlePileCanBeReserved();
    void chargingActionsFollowTaskState();
    void rechargeResponseUpdatesDisplayedBalance();
};

void HomePageTest::emptyAddressIsRejected()
{
    HomePage page;
    QSignalSpy geocode(&page, &HomePage::geocodeRequested);
    page.findChild<QPushButton *>(QStringLiteral("searchButton"))->click();
    QCOMPARE(geocode.count(), 0);
    QVERIFY(page.findChild<QLabel *>(QStringLiteral("homeStatusLabel"))
                ->text().contains(QStringLiteral("请先输入地址")));
}

void HomePageTest::manualAddressIsResolvedBeforeStationQuery()
{
    HomePage page;
    auto *address = page.findChild<QLineEdit *>(QStringLiteral("addressEdit"));
    auto *region = page.findChild<QComboBox *>(QStringLiteral("regionCombo"));
    auto *search = page.findChild<QPushButton *>(QStringLiteral("searchButton"));
    address->setText(QStringLiteral("南京西路100号"));
    region->setCurrentText(QStringLiteral("静安区"));

    QSignalSpy geocode(&page, &HomePage::geocodeRequested);
    QSignalSpy stations(&page, &HomePage::stationsRequested);
    search->click();
    QCOMPARE(geocode.count(), 1);
    QCOMPARE(stations.count(), 0);
    QCOMPARE(geocode.first().at(0).toString(), QStringLiteral("南京西路100号"));
    QCOMPARE(geocode.first().at(1).toString(), QStringLiteral("静安区"));
    QVERIFY(!search->isEnabled());

    page.setResolvedLocation(31.2304, 121.4737,
                             QStringLiteral("上海市静安区南京西路100号"));
    QCOMPARE(stations.count(), 1);
    QCOMPARE(stations.first().at(2).toDouble(), 31.2304);
    QCOMPARE(stations.first().at(3).toDouble(), 121.4737);
    QVERIFY(!search->isEnabled());

    page.setStations({});
    QVERIFY(search->isEnabled());
}

void HomePageTest::geocodeFailureRestoresControls()
{
    HomePage page;
    auto *address = page.findChild<QLineEdit *>(QStringLiteral("addressEdit"));
    auto *search = page.findChild<QPushButton *>(QStringLiteral("searchButton"));
    address->setText(QStringLiteral("测试地址"));
    search->click();
    QVERIFY(!search->isEnabled());

    page.showAddressResolutionError(QStringLiteral("腾讯地图请求过于频繁"));
    QVERIFY(search->isEnabled());
    QVERIFY(address->isEnabled());
    QVERIFY(page.findChild<QLabel *>(QStringLiteral("homeStatusLabel"))
                ->text().contains(QStringLiteral("请求过于频繁")));
}

void HomePageTest::missingGeocoderFallsBackToTextSearch()
{
    HomePage page;
    auto *address = page.findChild<QLineEdit *>(QStringLiteral("addressEdit"));
    address->setText(QStringLiteral("大连市星海广场"));
    QSignalSpy stations(&page, &HomePage::stationsRequested);
    page.useTextSearchFallback();
    QCOMPARE(stations.count(), 1);
    QCOMPARE(stations.first().at(1).toString(),
             QStringLiteral("大连市星海广场"));
    QVERIFY(std::isnan(stations.first().at(2).toDouble()));
    QVERIFY(std::isnan(stations.first().at(3).toDouble()));
}

void HomePageTest::simulatedGpsBypassesWebService()
{
    HomePage page;
    QSignalSpy geocode(&page, &HomePage::geocodeRequested);
    QSignalSpy stations(&page, &HomePage::stationsRequested);
    page.findChild<QPushButton *>(QStringLiteral("gpsButton"))->click();
    page.findChild<QPushButton *>(QStringLiteral("searchButton"))->click();
    QCOMPARE(geocode.count(), 0);
    QCOMPARE(stations.count(), 1);
    QCOMPARE(stations.first().at(2).toDouble(), 31.2304);
    QCOMPARE(stations.first().at(3).toDouble(), 121.4737);
}

void HomePageTest::chineseAddressInputIsAccepted()
{
    HomePage page;
    auto *address = page.findChild<QLineEdit *>(QStringLiteral("addressEdit"));
    QVERIFY(address->testAttribute(Qt::WA_InputMethodEnabled));
    QCOMPARE(address->inputMethodHints(), Qt::InputMethodHints(Qt::ImhNone));

    QInputMethodEvent inputEvent;
    inputEvent.setCommitString(QStringLiteral("上海市浦东新区世纪大道100号"));
    QApplication::sendEvent(address, &inputEvent);
    QCOMPARE(address->text(), QStringLiteral("上海市浦东新区世纪大道100号"));
}

void HomePageTest::numericOrderIdResetsSnapshotSequence()
{
    SnapshotStore store;
    QVERIFY(store.apply({{QStringLiteral("orderId"), 101},
                         {QStringLiteral("seq"), 8}}));
    QVERIFY(!store.apply({{QStringLiteral("orderId"), 101},
                          {QStringLiteral("seq"), 7}}));
    QVERIFY(store.apply({{QStringLiteral("orderId"), 102},
                         {QStringLiteral("seq"), 1}}));
    QCOMPARE(store.current().value(QStringLiteral("orderId")).toInt(), 102);
}

void HomePageTest::idlePileCanBeReserved()
{
    StationDetailPage page;
    page.setStation({{QStringLiteral("stationId"), 12},
                     {QStringLiteral("name"), QStringLiteral("测试站")}});
    page.setPiles({QJsonObject{{QStringLiteral("pileId"), 21},
                               {QStringLiteral("pileCode"), QStringLiteral("A01")},
                               {QStringLiteral("status"), QStringLiteral("reserved")}},
                   QJsonObject{{QStringLiteral("pileId"), 22},
                               {QStringLiteral("pileCode"), QStringLiteral("A02")},
                               {QStringLiteral("status"), QStringLiteral("available")}}});
    auto *duration = page.findChild<QSpinBox *>(
        QStringLiteral("reservationDuration"));
    auto *reserve = page.findChild<QPushButton *>(
        QStringLiteral("reservePileButton"));
    duration->setValue(20);
    QSignalSpy requested(&page, &StationDetailPage::reservationRequested);
    reserve->click();
    QCOMPARE(requested.count(), 1);
    QCOMPARE(requested.first().at(0).toLongLong(), 12);
    QCOMPARE(requested.first().at(1).toLongLong(), 22);
    QCOMPARE(requested.first().at(2).toInt(), 20);
    QVERIFY(!reserve->isEnabled());
    page.reservationFailed(QStringLiteral("余额不足"));
    QVERIFY(reserve->isEnabled());
}

void HomePageTest::chargingActionsFollowTaskState()
{
    ChargingPage page;
    auto *start = page.findChild<QPushButton *>(
        QStringLiteral("startChargingButton"));
    auto *cancel = page.findChild<QPushButton *>(
        QStringLiteral("cancelReservationButton"));
    auto *stop = page.findChild<QPushButton *>(
        QStringLiteral("stopChargingButton"));
    page.setReservation({
        {QStringLiteral("reservationId"), 31},
        {QStringLiteral("stationId"), 12},
        {QStringLiteral("pileId"), 22},
        {QStringLiteral("expiresAt"),
         QDateTime::currentDateTimeUtc().addSecs(900).toString(Qt::ISODate)}});
    QVERIFY(!start->isHidden());
    QVERIFY(!cancel->isHidden());
    QVERIFY(stop->isHidden());
    QSignalSpy startRequested(&page, &ChargingPage::startRequested);
    start->click();
    QCOMPARE(startRequested.count(), 1);
    QCOMPARE(startRequested.first().at(0).toLongLong(), 31);
    QVERIFY(!start->isEnabled());
    page.showActionError(QStringLiteral("开始失败"));
    QVERIFY(start->isEnabled());

    page.setSnapshot({{QStringLiteral("orderId"), 41},
                      {QStringLiteral("seq"), 1},
                      {QStringLiteral("status"), QStringLiteral("charging")}});
    QVERIFY(start->isHidden());
    QVERIFY(cancel->isHidden());
    QVERIFY(!stop->isHidden());
    QSignalSpy stopRequested(&page, &ChargingPage::stopRequested);
    stop->click();
    QCOMPARE(stopRequested.count(), 1);
    QCOMPARE(stopRequested.first().at(0).toLongLong(), 41);

    page.setNoActiveTask();
    QVERIFY(start->isHidden());
    QVERIFY(cancel->isHidden());
    QVERIFY(stop->isHidden());
}

void HomePageTest::rechargeResponseUpdatesDisplayedBalance()
{
    ProfilePage page;
    page.setProfile({{QStringLiteral("nickname"), QStringLiteral("测试用户")},
                     {QStringLiteral("phone"), QStringLiteral("13500135000")},
                     {QStringLiteral("balanceCents"), 0}});
    auto *balance = page.findChild<QLabel *>(
        QStringLiteral("profileBalanceLabel"));
    QVERIFY(balance);
    QCOMPARE(balance->text(), QStringLiteral("钱包余额：¥0.00"));
    page.setBalanceCents(12345);
    QCOMPARE(balance->text(), QStringLiteral("钱包余额：¥123.45"));
}

QTEST_MAIN(HomePageTest)
#include "homepage_test.moc"

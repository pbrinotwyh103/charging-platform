#include "userui_test.h"

#include "map/mapnavigator.h"
#include "pages/chargingpage.h"
#include "pages/homepage.h"
#include "pages/profilepage.h"
#include "pages/stationdetailpage.h"
#include "ui/usermainwindow.h"

#include <QDoubleSpinBox>
#include <QJsonArray>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QSignalSpy>
#include <QStackedWidget>
#include <QUrlQuery>
#include <QtTest>

void UserUiTest::demoWorkspaceShowsCoreFeatures()
{
    UserMainWindow window;
    window.showDemoWorkspace();
    auto *pages = window.findChild<QStackedWidget *>(QStringLiteral("userPages"));
    auto *stations = window.findChild<QListWidget *>(QStringLiteral("stationList"));
    QVERIFY(pages);
    QVERIFY(stations);
    QCOMPARE(pages->currentWidget()->objectName(), QStringLiteral("homePage"));
    QCOMPARE(stations->count(), 4);
    QVERIFY(window.findChild<QWidget *>(QStringLiteral("userBottomNavigation"))->isVisibleTo(&window));
}

void UserUiTest::reservationCanEnterChargingAndSettle()
{
    StationDetailPage stationPage;
    const QJsonObject station{{"stationId", 1}, {"name", QStringLiteral("测试充电站")},
                              {"address", QStringLiteral("测试路1号")},
                              {"priceCentsPerKwh", 168}, {"availablePiles", 1},
                              {"totalPiles", 1}, {"distanceKm", 1.2}};
    const QJsonObject pile{{"pileId", 11}, {"pileCode", QStringLiteral("TEST-01")},
                          {"type", QStringLiteral("fast")}, {"powerKw", 60.0},
                          {"status", QStringLiteral("available")}};
    QSignalSpy reservationSpy(&stationPage, &StationDetailPage::reservationRequested);
    stationPage.setStation(station);
    stationPage.setPiles(QJsonArray{pile});
    auto *pileList = stationPage.findChild<QListWidget *>(QStringLiteral("pileList"));
    auto *reserve = stationPage.findChild<QPushButton *>(QStringLiteral("reserveButton"));
    pileList->setCurrentRow(0);
    QVERIFY(reserve->isEnabled());
    QTest::mouseClick(reserve, Qt::LeftButton);
    QCOMPARE(reservationSpy.count(), 1);

    ChargingPage chargingPage;
    chargingPage.setDemoMode(true);
    chargingPage.setReservation(station, pile);
    auto *start = chargingPage.findChild<QPushButton *>(QStringLiteral("startChargingButton"));
    auto *stop = chargingPage.findChild<QPushButton *>(QStringLiteral("stopChargingButton"));
    auto *stage = chargingPage.findChild<QLabel *>(QStringLiteral("chargingStageLabel"));
    QVERIFY(start->isEnabled());
    QTest::mouseClick(start, Qt::LeftButton);
    QVERIFY(chargingPage.hasActiveOrder());
    QVERIFY(stage->text().contains(QStringLiteral("充电中")));
    QTest::mouseClick(stop, Qt::LeftButton);
    QVERIFY(!chargingPage.hasActiveOrder());
    QVERIFY(stage->text().contains(QStringLiteral("已完成")));
}

void UserUiTest::demoRechargeUpdatesBalanceAndLedger()
{
    ProfilePage profile;
    profile.setDemoMode(true);
    profile.setProfile({{QStringLiteral("nickname"), QStringLiteral("测试用户")},
                        {QStringLiteral("phone"), QStringLiteral("138****0000")},
                        {QStringLiteral("balanceCents"), 28650}});
    auto *amount = profile.findChild<QDoubleSpinBox *>(QStringLiteral("rechargeAmount"));
    auto *button = profile.findChild<QPushButton *>(QStringLiteral("rechargeButton"));
    auto *balance = profile.findChild<QLabel *>(QStringLiteral("profileBalance"));
    auto *ledger = profile.findChild<QListWidget *>(QStringLiteral("walletLedgerList"));
    amount->setValue(50.0);
    const int before = ledger->count();
    QTest::mouseClick(button, Qt::LeftButton);
    QVERIFY(balance->text().contains(QStringLiteral("336.50")));
    QCOMPARE(ledger->count(), before + 1);
}

void UserUiTest::mapUrlCarriesTravelModeAndCoordinates()
{
    qputenv("TENCENT_MAP_KEY", "demo-key");
    const QUrl url = MapNavigator::navigationUrl(31.2304, 121.4737, 31.2397,
                                                 121.5056, QStringLiteral("walking"));
    const QUrlQuery query(url);
    QVERIFY(url.path().contains(QStringLiteral("routeplan")));
    QCOMPARE(query.queryItemValue(QStringLiteral("type")), QStringLiteral("walk"));
    QCOMPARE(query.queryItemValue(QStringLiteral("fromcoord")), QStringLiteral("31.230400,121.473700"));
    qunsetenv("TENCENT_MAP_KEY");
}

void UserUiTest::compactAppVisualShellIsPresent()
{
    UserMainWindow window;
    auto *shell = window.findChild<QWidget *>(QStringLiteral("userAppShell"));
    auto *card = window.findChild<QWidget *>(QStringLiteral("userLoginCard"));
    QVERIFY(shell);
    QVERIFY(card);
    QVERIFY(card->minimumHeight() >= 360);
    QVERIFY(window.minimumWidth() <= 360);
}

QTEST_MAIN(UserUiTest)

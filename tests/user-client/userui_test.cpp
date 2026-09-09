#include "userui_test.h"

#include "map/mapnavigator.h"
#include "pages/chargingpage.h"
#include "pages/homepage.h"
#include "pages/profilepage.h"
#include "pages/stationdetailpage.h"
#include "ui/usermainwindow.h"

#include <QDoubleSpinBox>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QSignalSpy>
#include <QStackedWidget>
#include <QTemporaryDir>
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

void UserUiTest::mapKeyCanBeLoadedFromWorkingDirectoryConfig()
{
    qunsetenv("TENCENT_MAP_KEY");
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QVERIFY(QDir(directory.path()).mkpath(QStringLiteral("config")));
    QFile config(directory.path() + QStringLiteral("/config/app.ini"));
    QVERIFY(config.open(QIODevice::WriteOnly | QIODevice::Text));
    config.write("[map]\ntencent_key=file-demo-key\n");
    config.close();

    const QString previousPath = QDir::currentPath();
    QVERIFY(QDir::setCurrent(directory.path()));

    QCOMPARE(MapNavigator::apiKey(), QStringLiteral("file-demo-key"));

    QVERIFY(QDir::setCurrent(previousPath));
}

void UserUiTest::geocodingRequestAndResponseUseTencentWebService()
{
    qputenv("TENCENT_MAP_KEY", "demo-key");
    const QUrl url = MapNavigator::geocodingUrl(QStringLiteral("北京理工大学深圳校区"),
                                                QStringLiteral("深圳市"));
    const QUrlQuery query(url);
    QVERIFY(url.path().contains(QStringLiteral("geocoder")));
    QCOMPARE(query.queryItemValue(QStringLiteral("address")),
             QStringLiteral("北京理工大学深圳校区"));
    QCOMPARE(query.queryItemValue(QStringLiteral("region")), QStringLiteral("深圳市"));
    QCOMPARE(query.queryItemValue(QStringLiteral("key")), QStringLiteral("demo-key"));

    double latitude = 0.0;
    double longitude = 0.0;
    QString error;
    const QByteArray response = R"({"status":0,"message":"query ok","result":{"location":{"lat":22.600001,"lng":113.900002}}})";
    QVERIFY(MapNavigator::parseGeocodingResponse(response, &latitude, &longitude, &error));
    QCOMPARE(latitude, 22.600001);
    QCOMPARE(longitude, 113.900002);
    qunsetenv("TENCENT_MAP_KEY");
}

void UserUiTest::navigationUsesExpandedMapMode()
{
    qputenv("TENCENT_MAP_KEY", "demo-key");
    StationDetailPage page;
    page.setStation({{"stationId", 1}, {"name", QStringLiteral("测试站")},
                     {"address", QStringLiteral("深圳市")}, {"latitude", 22.55},
                     {"longitude", 114.08}, {"originLatitude", 22.5431},
                     {"originLongitude", 114.0579}, {"originName", QStringLiteral("测试起点")},
                     {"priceCentsPerKwh", 150}, {"availablePiles", 1},
                     {"totalPiles", 1}, {"distanceKm", 2.1}});
    auto *walk = page.findChild<QPushButton *>(QStringLiteral("walkNavigationButton"));
    auto *close = page.findChild<QPushButton *>(QStringLiteral("closeNavigationButton"));
    auto *map = page.findChild<QWidget *>(QStringLiteral("navigationWebView"));
    auto *piles = page.findChild<QListWidget *>(QStringLiteral("pileList"));
    QVERIFY(walk);
    QVERIFY(close);
    QVERIFY(map);
    QVERIFY(piles);
    QTest::mouseClick(walk, Qt::LeftButton);
    QVERIFY(!map->isHidden());
    QVERIFY(piles->isHidden());
    QVERIFY(map->minimumHeight() >= 420);
    QTest::mouseClick(close, Qt::LeftButton);
    QVERIFY(map->isHidden());
    QVERIFY(!piles->isHidden());
    qunsetenv("TENCENT_MAP_KEY");
}

void UserUiTest::emptyStationDoesNotRequestPiles()
{
    StationDetailPage stationPage;
    QSignalSpy pilesSpy(&stationPage, &StationDetailPage::pilesRequested);

    stationPage.setStation({});

    QCOMPARE(pilesSpy.count(), 0);
    QVERIFY(stationPage.findChild<QLabel *>(QStringLiteral("stationStatusLabel"))
                ->text()
                .contains(QStringLiteral("请先选择有效的充电站")));
}

void UserUiTest::stationSearchUsesSeedDataCityByDefault()
{
    HomePage home;
    QSignalSpy stationSpy(&home, &HomePage::stationsRequested);
    auto *search = home.findChild<QPushButton *>(QStringLiteral("stationSearchButton"));
    QVERIFY(search);

    QTest::mouseClick(search, Qt::LeftButton);

    QCOMPARE(stationSpy.count(), 1);
    QCOMPARE(stationSpy.at(0).at(2).toDouble(), 22.5431);
    QCOMPARE(stationSpy.at(0).at(3).toDouble(), 114.0579);
}

void UserUiTest::simulatedLocationDoesNotFilterByDisplayText()
{
    HomePage home;
    QSignalSpy stationSpy(&home, &HomePage::stationsRequested);
    const auto buttons = home.findChildren<QPushButton *>();
    QPushButton *gps = nullptr;
    for (auto *button : buttons) {
        if (button->text().contains(QStringLiteral("模拟定位"))) {
            gps = button;
            break;
        }
    }
    auto *search = home.findChild<QPushButton *>(QStringLiteral("stationSearchButton"));
    QVERIFY(gps);
    QVERIFY(search);

    QTest::mouseClick(gps, Qt::LeftButton);
    QTest::mouseClick(search, Qt::LeftButton);

    QCOMPARE(stationSpy.count(), 1);
    QCOMPARE(stationSpy.at(0).at(1).toString(), QString());
}

void UserUiTest::pileCodeEntryEmitsTrimmedIdentifier()
{
    HomePage home;
    QSignalSpy lookupSpy(&home, &HomePage::pileCodeRequested);
    auto *input = home.findChild<QLineEdit *>(QStringLiteral("pileCodeInput"));
    auto *button = home.findChild<QPushButton *>(QStringLiteral("pileCodeConnectButton"));
    QVERIFY(input);
    QVERIFY(button);
    input->setText(QStringLiteral("  DL-SP-001  "));
    QTest::mouseClick(button, Qt::LeftButton);
    QCOMPARE(lookupSpy.count(), 1);
    QCOMPARE(lookupSpy.takeFirst().at(0).toString(), QStringLiteral("DL-SP-001"));
}

void UserUiTest::directPileIsSelectedEvenOutsideLoadedPage()
{
    StationDetailPage page;
    const QJsonObject station{{"stationId", 1}, {"name", QStringLiteral("测试站")},
                              {"address", QStringLiteral("深圳市")},
                              {"priceCentsPerKwh", 150}, {"availablePiles", 1},
                              {"totalPiles", 30}};
    const QJsonObject first{{"pileId", 1}, {"pileCode", QStringLiteral("FIRST")},
                            {"type", QStringLiteral("slow")}, {"powerKw", 7},
                            {"status", QStringLiteral("offline")}};
    const QJsonObject target{{"pileId", 30}, {"pileCode", QStringLiteral("TARGET-030")},
                             {"type", QStringLiteral("fast")}, {"powerKw", 60},
                             {"status", QStringLiteral("available")}};
    page.setDirectPile(station, target);
    page.setPiles(QJsonArray{first});
    auto *list = page.findChild<QListWidget *>(QStringLiteral("pileList"));
    auto *reserve = page.findChild<QPushButton *>(QStringLiteral("reserveButton"));
    QVERIFY(list);
    QCOMPARE(list->count(), 2);
    QCOMPARE(list->currentItem()->data(Qt::UserRole).toJsonObject()
                 .value(QStringLiteral("pileId")).toInteger(), qint64(30));
    QVERIFY(reserve->isEnabled());
    QVERIFY(page.findChild<QLabel *>(QStringLiteral("stationStatusLabel"))
                ->text().contains(QStringLiteral("已通过编号连接")));
}

void UserUiTest::chargingEntryChecksActiveOrder()
{
    UserMainWindow window;
    QSignalSpy checkSpy(&window, &UserMainWindow::activeOrderCheckRequested);
    auto *button = window.findChild<QPushButton *>(
        QStringLiteral("chargingNavigationButton"));
    auto *pages = window.findChild<QStackedWidget *>(QStringLiteral("userPages"));
    QVERIFY(button);
    QVERIFY(pages);

    button->click();
    QCOMPARE(checkSpy.count(), 1);
    window.handleActiveOrderCheck(false, {});
    QCOMPARE(pages->currentWidget()->objectName(), QStringLiteral("chargingPage"));
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

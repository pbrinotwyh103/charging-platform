#include "userui_test.h"

#include "map/mapnavigator.h"
#include "map/tencentgeocoder.h"
#include "pages/chargingpage.h"
#include "pages/homepage.h"
#include "pages/navigationpage.h"
#include "pages/profilepage.h"
#include "pages/stationdetailpage.h"
#include "pages/stationutils.h"
#include "ui/usermainwindow.h"

#include <QDoubleSpinBox>
#include <QBuffer>
#include <QBoxLayout>
#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QInputMethodEvent>
#include <QInputMethod>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QProgressBar>
#include <QPushButton>
#include <QSignalSpy>
#include <QStackedWidget>
#include <QTemporaryDir>
#include <QTabWidget>
#include <QUrlQuery>
#include <QWebEngineView>
#include <QtTest>
#include <cmath>

void UserUiTest::demoWorkspaceShowsCoreFeatures()
{
    UserMainWindow window;
    window.showDemoWorkspace();
    auto *pages = window.findChild<QStackedWidget *>(QStringLiteral("userPages"));
    auto *stations = window.findChild<QListWidget *>(QStringLiteral("stationList"));
    QVERIFY(pages);
    QVERIFY(stations);
    QCOMPARE(pages->currentWidget()->objectName(), QStringLiteral("homePage"));
    QCOMPARE(stations->count(), 10);
    QVERIFY(window.findChild<QWidget *>(QStringLiteral("userBottomNavigation"))->isVisibleTo(&window));
}

void UserUiTest::reservationCanEnterChargingAndSettle()
{
    StationDetailPage stationPage;
    stationPage.setDemoMode(true);
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
    QVERIFY(stationPage.findChild<QLabel *>(QStringLiteral("stationStatusLabel"))
                ->text()
                .contains(QStringLiteral("演示模拟数据")));
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
    auto *battery = chargingPage.findChild<QProgressBar *>(QStringLiteral("batteryProgress"));
    QVERIFY(start->isEnabled());
    QTest::mouseClick(start, Qt::LeftButton);
    QVERIFY(chargingPage.hasActiveOrder());
    QVERIFY(stage->text().contains(QStringLiteral("充电中")));
    const int settledBattery = battery->value();
    QTest::mouseClick(stop, Qt::LeftButton);
    QVERIFY(!chargingPage.hasActiveOrder());
    QCOMPARE(stage->text(), QStringLiteral("暂无进行中的充电任务"));
    QCOMPARE(start->text(), QStringLiteral("等待预约"));
    QVERIFY(!start->isEnabled());
    QVERIFY(!stop->isEnabled());
    QCOMPARE(battery->value(), settledBattery);
    QCOMPARE(chargingPage.findChild<QLabel *>(QStringLiteral("chargingMetricsLabel"))->text(),
             QStringLiteral("电量 0.00 kWh\n实时功率 0.0 kW\n已充时长 00:00\n当前费用 ¥0.00"));

    chargingPage.setReservation(station, pile);
    chargingPage.setSnapshot({{QStringLiteral("orderId"), 42},
                              {QStringLiteral("status"), QStringLiteral("charging")},
                              {QStringLiteral("energyKwh"), 2.5},
                              {QStringLiteral("feeCents"), 420}});
    QVERIFY(chargingPage.hasActiveOrder());
    chargingPage.setSnapshot({{QStringLiteral("orderId"), 42},
                              {QStringLiteral("status"), QStringLiteral("completed")}});
    QVERIFY(!chargingPage.hasActiveOrder());
    QCOMPARE(stage->text(), QStringLiteral("暂无进行中的充电任务"));
}

void UserUiTest::chargingUpdatesDoNotInterruptNavigation()
{
    UserMainWindow window;
    window.showDemoWorkspace();
    auto *pages = window.findChild<QStackedWidget *>(QStringLiteral("userPages"));
    auto *home = window.findChild<QPushButton *>(QStringLiteral("homeNavigationButton"));
    auto *mine = window.findChild<QPushButton *>(QStringLiteral("profileNavigationButton"));
    auto *stage = window.findChild<QLabel *>(QStringLiteral("chargingStageLabel"));
    QVERIFY(pages);
    QVERIFY(home);
    QVERIFY(mine);

    const QJsonObject active{{QStringLiteral("orderId"), 42},
                             {QStringLiteral("status"), QStringLiteral("charging")},
                             {QStringLiteral("energyKwh"), 2.5},
                             {QStringLiteral("feeCents"), 420}};
    window.showChargingSnapshot(active);
    QCOMPARE(pages->currentWidget()->objectName(), QStringLiteral("chargingPage"));

    home->click();
    QCOMPARE(pages->currentWidget()->objectName(), QStringLiteral("homePage"));
    window.showChargingSnapshot(active);
    QCOMPARE(pages->currentWidget()->objectName(), QStringLiteral("homePage"));

    mine->click();
    QCOMPARE(pages->currentWidget()->objectName(), QStringLiteral("profilePage"));
    window.showChargingStopped({{QStringLiteral("orderId"), 42},
                                {QStringLiteral("status"), QStringLiteral("completed")}});
    QCOMPARE(pages->currentWidget()->objectName(), QStringLiteral("profilePage"));
    QCOMPARE(stage->text(), QStringLiteral("暂无进行中的充电任务"));
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

void UserUiTest::mapNavigationRejectsUnsafeUrlsAndCoordinates()
{
    QVERIFY(MapNavigator::isAllowedNavigationUrl(
        QUrl(QStringLiteral("https://apis.map.qq.com/uri/v1/routeplan"))));
    QVERIFY(MapNavigator::isAllowedNavigationUrl(
        QUrl(QStringLiteral("https://map.qq.com/"))));
    QVERIFY(!MapNavigator::isAllowedNavigationUrl(
        QUrl(QStringLiteral("http://map.qq.com/"))));
    QVERIFY(!MapNavigator::isAllowedNavigationUrl(
        QUrl(QStringLiteral("https://map.qq.com.attacker.example/"))));

    qputenv("TENCENT_MAP_KEY", "demo-key");
    QVERIFY(MapNavigator::navigationUrl(91.0, 114.0, 22.5, 114.1).isEmpty());
    QVERIFY(MapNavigator::navigationUrl(22.5, 114.0, 22.6, 181.0).isEmpty());
    qunsetenv("TENCENT_MAP_KEY");
}

void UserUiTest::travelModeOpensStandaloneNavigationPage()
{
    qputenv("TENCENT_MAP_KEY", "standalone-page-test-key");
    UserMainWindow window;
    window.showDemoWorkspace();
    auto *pages = window.findChild<QStackedWidget *>(QStringLiteral("userPages"));
    auto *detail = window.findChild<StationDetailPage *>(QStringLiteral("stationDetailPage"));
    QVERIFY(pages);
    QVERIFY(detail);
    detail->setStation(QJsonObject{
        {QStringLiteral("stationId"), 101},
        {QStringLiteral("name"), QStringLiteral("市民中心超级充电站")},
        {QStringLiteral("latitude"), 22.5431},
        {QStringLiteral("longitude"), 114.0579},
        {QStringLiteral("distanceKm"), 0.8}});
    pages->setCurrentWidget(detail);
    QCOMPARE(pages->currentWidget()->objectName(), QStringLiteral("stationDetailPage"));
    QVERIFY(!pages->currentWidget()->findChild<QWebEngineView *>(
        QStringLiteral("navigationWebView")));

    auto *walk = pages->currentWidget()->findChild<QPushButton *>(
        QStringLiteral("walkingNavigationButton"));
    QVERIFY(walk);
    walk->click();
    QCOMPARE(pages->currentWidget()->objectName(), QStringLiteral("navigationPage"));
    QVERIFY(pages->currentWidget()->findChild<QWebEngineView *>(
        QStringLiteral("navigationWebView")));
    QVERIFY(window.findChild<QLabel *>(QStringLiteral("navigationTitle"))
                ->text().contains(QStringLiteral("步行导航")));
    auto *navigationPage = window.findChild<NavigationPage *>(
        QStringLiteral("navigationPage"));
    QVERIFY(navigationPage);
    const QUrlQuery routeQuery(navigationPage->routeUrl());
    QCOMPARE(routeQuery.queryItemValue(QStringLiteral("type")), QStringLiteral("walk"));
    QCOMPARE(routeQuery.queryItemValue(QStringLiteral("fromcoord")),
             QStringLiteral("22.543100,114.057900"));
    QVERIFY(!window.findChild<QLabel *>(QStringLiteral("navigationStatusLabel"))
                 ->text().isEmpty());
    QVERIFY(!window.findChild<QWidget *>(QStringLiteral("userBottomNavigation"))
                 ->isVisibleTo(&window));

    auto *back = window.findChild<QPushButton *>(QStringLiteral("navigationBackButton"));
    QVERIFY(back);
    back->click();
    QCOMPARE(pages->currentWidget()->objectName(), QStringLiteral("stationDetailPage"));
    qunsetenv("TENCENT_MAP_KEY");
}

void UserUiTest::navigationPageReportsLoadResultAndRetry()
{
    qputenv("TENCENT_MAP_KEY", "load-result-test-key");
    NavigationPage page;
    page.showRoute(QJsonObject{
        {QStringLiteral("stationId"), 8},
        {QStringLiteral("name"), QStringLiteral("测试站")},
        {QStringLiteral("latitude"), 22.60},
        {QStringLiteral("longitude"), 114.10}},
        QStringLiteral("driving"), 22.54, 114.05, true);
    auto *webView = page.findChild<QWebEngineView *>(QStringLiteral("navigationWebView"));
    auto *status = page.findChild<QLabel *>(QStringLiteral("navigationStatusLabel"));
    auto *retry = page.findChild<QPushButton *>(QStringLiteral("navigationRetryButton"));
    QVERIFY(webView);
    QVERIFY(status);
    QVERIFY(retry);

    QVERIFY(QMetaObject::invokeMethod(webView, "loadFinished", Qt::DirectConnection,
                                      Q_ARG(bool, false)));
    QVERIFY(status->text().contains(QStringLiteral("加载失败")));
    QVERIFY(!retry->isHidden());
    QVERIFY(QMetaObject::invokeMethod(webView, "loadFinished", Qt::DirectConnection,
                                      Q_ARG(bool, true)));
    QVERIFY(status->text().contains(QStringLiteral("加载完成")));
    QVERIFY(retry->isHidden());
    qunsetenv("TENCENT_MAP_KEY");
}

void UserUiTest::invalidCoordinatesSortLastAndRemainStable()
{
    QVERIFY(std::isinf(StationUtils::distanceKm(22.5, 114.0, 91.0, 114.0)));
    QVERIFY(std::isinf(StationUtils::distanceKm(22.5, 114.0,
                                               qQNaN(), 114.0)));
    const QJsonArray sorted = StationUtils::sortByDistance(QJsonArray{
        QJsonObject{{QStringLiteral("stationId"), 1},
                    {QStringLiteral("latitude"), 200.0},
                    {QStringLiteral("longitude"), 114.0}},
        QJsonObject{{QStringLiteral("stationId"), 2},
                    {QStringLiteral("latitude"), 22.51},
                    {QStringLiteral("longitude"), 114.0}},
        QJsonObject{{QStringLiteral("stationId"), 3}},
        QJsonObject{{QStringLiteral("stationId"), 4},
                    {QStringLiteral("latitude"), 22.50},
                    {QStringLiteral("longitude"), 114.0}}}, 22.50, 114.0);
    QCOMPARE(sorted.at(0).toObject().value(QStringLiteral("stationId")).toInt(), 4);
    QCOMPARE(sorted.at(1).toObject().value(QStringLiteral("stationId")).toInt(), 2);
    QCOMPARE(sorted.at(2).toObject().value(QStringLiteral("stationId")).toInt(), 1);
    QCOMPARE(sorted.at(3).toObject().value(QStringLiteral("stationId")).toInt(), 3);
    QVERIFY(sorted.at(2).toObject().value(QStringLiteral("distanceKm")).isNull());
    QVERIFY(sorted.at(3).toObject().value(QStringLiteral("distanceKm")).isNull());
}

void UserUiTest::stationCardsHandleMissingFieldsAndRefreshLifecycle()
{
    HomePage home;
    auto *search = home.findChild<QPushButton *>(QStringLiteral("stationSearchButton"));
    auto *list = home.findChild<QListWidget *>(QStringLiteral("stationList"));
    auto *status = home.findChild<QLabel *>(QStringLiteral("stationSearchStatus"));
    QVERIFY(search);
    QVERIFY(list);
    QVERIFY(status);

    home.showLoading();
    QVERIFY(!search->isEnabled());
    QVERIFY(search->text().contains(QStringLiteral("正在搜索")));
    home.setStations(QJsonArray{QJsonObject{
        {QStringLiteral("stationId"), 9},
        {QStringLiteral("latitude"), 999.0},
        {QStringLiteral("longitude"), 114.0}}});
    QVERIFY(search->isEnabled());
    QCOMPARE(search->text(), QStringLiteral("刷新站点"));
    QCOMPARE(list->count(), 1);
    QVERIFY(list->item(0)->text().contains(QStringLiteral("未命名充电站")));
    QVERIFY(list->item(0)->text().contains(QStringLiteral("地址暂未提供")));
    QVERIFY(list->item(0)->text().contains(QStringLiteral("电价未知")));
    QVERIFY(list->item(0)->text().contains(QStringLiteral("空闲桩数未知")));
    QVERIFY(list->item(0)->text().contains(QStringLiteral("距离未知")));
    QVERIFY(status->text().contains(QStringLiteral("更新于刚刚")));

    home.showError(QStringLiteral("请求超时"));
    QVERIFY(search->isEnabled());
    QCOMPARE(search->text(), QStringLiteral("重新搜索"));
    QVERIFY(status->text().contains(QStringLiteral("请求超时")));
}

void UserUiTest::pileDetailsShowAvailabilityAndRefreshLifecycle()
{
    StationDetailPage page;
    QSignalSpy refreshSpy(&page, &StationDetailPage::pilesRequested);
    page.setStation(QJsonObject{{QStringLiteral("stationId"), 5}});
    auto *refresh = page.findChild<QPushButton *>(QStringLiteral("pileRefreshButton"));
    auto *list = page.findChild<QListWidget *>(QStringLiteral("pileList"));
    auto *status = page.findChild<QLabel *>(QStringLiteral("stationStatusLabel"));
    QVERIFY(refresh);
    QVERIFY(list);
    QVERIFY(status);
    QVERIFY(!refresh->isEnabled());
    QCOMPARE(refreshSpy.count(), 1);

    page.setPiles(QJsonArray{
        QJsonObject{{QStringLiteral("pileId"), 51},
                    {QStringLiteral("pileCode"), QStringLiteral("P-51")},
                    {QStringLiteral("type"), QStringLiteral("fast")},
                    {QStringLiteral("powerKw"), 120.0},
                    {QStringLiteral("status"), QStringLiteral("charging")},
                    {QStringLiteral("estimatedAvailableMinutes"), 18.2}},
        QJsonObject{{QStringLiteral("pileId"), 52},
                    {QStringLiteral("status"), QStringLiteral("disabled")}},
        QJsonObject{{QStringLiteral("pileId"), 53},
                    {QStringLiteral("status"), QStringLiteral("available")}}});
    QVERIFY(refresh->isEnabled());
    QVERIFY(status->text().contains(QStringLiteral("服务端状态")));
    QVERIFY(!status->text().contains(QStringLiteral("演示模拟")));
    QVERIFY(list->item(0)->text().contains(QStringLiteral("预计约 19 分钟")));
    QVERIFY(list->item(1)->text().contains(QStringLiteral("已停用")));
    QVERIFY(list->item(1)->text().contains(QStringLiteral("功率未知")));
    QVERIFY(list->item(1)->text().contains(QStringLiteral("预计可用时间：暂未提供")));
    QVERIFY(list->item(2)->text().contains(QStringLiteral("可立即预约")));

    refresh->click();
    QCOMPARE(refreshSpy.count(), 2);
    QVERIFY(!refresh->isEnabled());
    page.showError(QStringLiteral("电桩状态查询超时"));
    QVERIFY(refresh->isEnabled());
    QCOMPARE(refresh->text(), QStringLiteral("重新刷新"));
    QVERIFY(status->text().contains(QStringLiteral("超时")));
}

void UserUiTest::userWindowAdaptsBetweenCompactAndWideLayouts()
{
    UserMainWindow window;
    window.showDemoWorkspace();
    window.resize(400, 680);
    window.show();
    QCoreApplication::processEvents();

    auto *content = window.findChild<QWidget *>(QStringLiteral("userContentColumn"));
    auto *filterLayout = window.findChild<QBoxLayout *>(QStringLiteral("stationFilterLayout"));
    auto *actionLayout = window.findChild<QBoxLayout *>(QStringLiteral("stationActionLayout"));
    auto *rechargeLayout = window.findChild<QBoxLayout *>(QStringLiteral("rechargeLayout"));
    QVERIFY(content);
    QVERIFY(filterLayout);
    QVERIFY(actionLayout);
    QVERIFY(rechargeLayout);
    QCOMPARE(content->property("compact").toBool(), true);
    QVERIFY(content->width() <= window.width());
    QCOMPARE(filterLayout->direction(), QBoxLayout::TopToBottom);
    QCOMPARE(actionLayout->direction(), QBoxLayout::TopToBottom);
    QCOMPARE(rechargeLayout->direction(), QBoxLayout::TopToBottom);

    window.resize(1280, 900);
    QCoreApplication::processEvents();
    QCOMPARE(content->property("compact").toBool(), false);
    QCOMPARE(content->width(), 1080);
    QCOMPARE(filterLayout->direction(), QBoxLayout::LeftToRight);
    QCOMPARE(actionLayout->direction(), QBoxLayout::LeftToRight);
    QCOMPARE(rechargeLayout->direction(), QBoxLayout::LeftToRight);
    QVERIFY(qAbs(content->geometry().center().x()
                 - window.centralWidget()->rect().center().x()) <= 2);
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

void UserUiTest::geocoderBuildsTencentRequestAndParsesCoordinates()
{
    const QUrl url = TencentGeocoder::requestUrl(QStringLiteral("深圳北站"),
                                                  QStringLiteral("深圳市"),
                                                  QStringLiteral("test-key"));
    const QUrlQuery query(url);
    QCOMPARE(url.path(), QStringLiteral("/ws/geocoder/v1/"));
    QCOMPARE(query.queryItemValue(QStringLiteral("address")), QStringLiteral("深圳北站"));
    QCOMPARE(query.queryItemValue(QStringLiteral("region")), QStringLiteral("深圳市"));
    QCOMPARE(query.queryItemValue(QStringLiteral("key")), QStringLiteral("test-key"));

    double latitude = 0;
    double longitude = 0;
    QString error;
    QVERIFY(TencentGeocoder::parseResponse(
        R"({"status":0,"result":{"location":{"lat":22.6099,"lng":114.0295}}})",
        &latitude, &longitude, &error));
    QCOMPARE(latitude, 22.6099);
    QCOMPARE(longitude, 114.0295);
}

void UserUiTest::geocoderInvalidatesSupersededRequests()
{
    const QByteArray previousKey = qgetenv("TENCENT_MAP_KEY");
    qputenv("TENCENT_MAP_KEY", "request-id-test-key");
    TencentGeocoder geocoder;
    QSignalSpy startedSpy(&geocoder, &TencentGeocoder::started);

    const quint64 first = geocoder.lookup(QStringLiteral("深圳北站"),
                                          QStringLiteral("深圳市"));
    const quint64 second = geocoder.lookup(QStringLiteral("市民中心"),
                                           QStringLiteral("深圳市"));

    QVERIFY(second > first);
    QCOMPARE(geocoder.activeRequestId(), second);
    QCOMPARE(startedSpy.count(), 2);
    QCOMPARE(startedSpy.at(0).at(0).toULongLong(), first);
    QCOMPARE(startedSpy.at(1).at(0).toULongLong(), second);
    QCOMPARE(startedSpy.at(1).at(1).toString(), QStringLiteral("市民中心"));

    geocoder.cancel();
    QCOMPARE(geocoder.activeRequestId(), quint64(0));
    if (previousKey.isNull()) qunsetenv("TENCENT_MAP_KEY");
    else qputenv("TENCENT_MAP_KEY", previousKey);
}

void UserUiTest::geocodingStatusOffersExplicitRetry()
{
    HomePage home;
    auto *status = home.findChild<QLabel *>(QStringLiteral("stationGeocodingStatus"));
    auto *retry = home.findChild<QPushButton *>(QStringLiteral("stationGeocodingRetryButton"));
    QSignalSpy retrySpy(&home, &HomePage::geocodingRetryRequested);
    QVERIFY(status);
    QVERIFY(retry);

    home.showGeocodingLoading(QStringLiteral("深圳北站"));
    QVERIFY(status->text().contains(QStringLiteral("正在解析")));
    QVERIFY(status->text().contains(QStringLiteral("深圳北站")));
    QVERIFY(retry->isHidden());

    home.showGeocodingSucceeded();
    QVERIFY(status->text().contains(QStringLiteral("解析成功")));
    QVERIFY(retry->isHidden());

    home.showGeocodingFailed(QStringLiteral("网络超时"));
    QVERIFY(status->text().contains(QStringLiteral("网络超时")));
    QVERIFY(!retry->isHidden());
    retry->click();
    QCOMPARE(retrySpy.count(), 1);
    QVERIFY(status->text().contains(QStringLiteral("正在解析")));
    QVERIFY(retry->isHidden());

    home.clearGeocodingStatus();
    QVERIFY(status->text().isEmpty());
    QVERIFY(retry->isHidden());
}

void UserUiTest::addressInputAcceptsChineseInputMethodText()
{
    HomePage home;
    auto *address = home.findChild<QLineEdit *>(QStringLiteral("stationAddressInput"));
    QVERIFY(address);
    QVERIFY(address->testAttribute(Qt::WA_InputMethodEnabled));
    address->setFocus();
    QInputMethodEvent event;
    event.setCommitString(QStringLiteral("深圳北站"));
    QApplication::sendEvent(address, &event);
    QCOMPARE(address->text(), QStringLiteral("深圳北站"));
}

void UserUiTest::regionSelectorCoversShenzhenDistricts()
{
    UserMainWindow window;
    window.showDemoWorkspace();
    auto *regions = window.findChild<QComboBox *>(QStringLiteral("stationRegionComboBox"));
    auto *stations = window.findChild<QListWidget *>(QStringLiteral("stationList"));
    auto *search = window.findChild<QPushButton *>(QStringLiteral("stationSearchButton"));
    QVERIFY(regions);
    QVERIFY(stations);
    QVERIFY(search);
    QCOMPARE(regions->count(), 12);
    QVERIFY(regions->findText(QStringLiteral("南山区")) >= 0);
    QVERIFY(regions->findText(QStringLiteral("大鹏新区")) >= 0);

    regions->setCurrentText(QStringLiteral("南山区"));
    search->click();
    QCOMPARE(stations->count(), 1);
    QVERIFY(stations->item(0)->text().contains(QStringLiteral("南山科技园充电站")));
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

void UserUiTest::favoriteResponseUpdatesProfileList()
{
    UserMainWindow window;
    auto *favorites = window.findChild<QListWidget *>(QStringLiteral("favoriteStationList"));
    QVERIFY(favorites);

    window.showFavoriteChanged({{QStringLiteral("name"), QStringLiteral("测试充电站")},
                                {QStringLiteral("favorited"), true}});
    QCOMPARE(favorites->count(), 1);
    QCOMPARE(favorites->item(0)->text(), QStringLiteral("测试充电站"));

    window.showFavoriteChanged({{QStringLiteral("name"), QStringLiteral("测试充电站")},
                                {QStringLiteral("favorited"), false}});
    QCOMPARE(favorites->count(), 0);
}

void UserUiTest::favoriteToggleDisablesAndRollsBack()
{
    StationDetailPage page;
    page.setStation({{QStringLiteral("stationId"), 7},
                     {QStringLiteral("name"), QStringLiteral("测试站")},
                     {QStringLiteral("address"), QStringLiteral("测试路")},
                     {QStringLiteral("favorited"), false}});
    auto *button = page.findChild<QPushButton *>(QStringLiteral("favoriteButton"));
    QVERIFY(button);
    QSignalSpy requestSpy(&page, &StationDetailPage::favoriteRequested);

    button->click();
    QCOMPARE(requestSpy.count(), 1);
    QCOMPARE(requestSpy.first().at(0).toLongLong(), qint64(7));
    QVERIFY(!button->isEnabled());
    QVERIFY(button->text().contains(QStringLiteral("正在收藏")));

    page.favoriteUpdateFailed(QStringLiteral("网络错误"));
    QVERIFY(button->isEnabled());
    QCOMPARE(button->text(), QStringLiteral("☆ 收藏站点"));

    button->click();
    QVERIFY(!button->isEnabled());
    page.favoriteUpdateSucceeded(true);
    QVERIFY(button->isEnabled());
    QCOMPARE(button->text(), QStringLiteral("★ 已收藏"));

    page.setStation({{QStringLiteral("stationId"), 7},
                     {QStringLiteral("name"), QStringLiteral("测试站")},
                     {QStringLiteral("address"), QStringLiteral("测试路")},
                     {QStringLiteral("favorited"), true}});
    QVERIFY(button->isEnabled());
    QCOMPARE(button->text(), QStringLiteral("★ 已收藏"));
}

void UserUiTest::favoriteListHasLoadingErrorAndRetryStates()
{
    ProfilePage profile;
    auto *list = profile.findChild<QListWidget *>(QStringLiteral("favoriteStationList"));
    auto *retry = profile.findChild<QPushButton *>(QStringLiteral("favoriteRetryButton"));
    QVERIFY(list);
    QVERIFY(retry);
    QSignalSpy refreshSpy(&profile, &ProfilePage::favoritesRefreshRequested);

    profile.showFavoritesLoading();
    QCOMPARE(list->count(), 1);
    QVERIFY(list->item(0)->text().contains(QStringLiteral("正在加载")));
    QVERIFY(retry->isHidden());

    profile.setFavoriteStations(QJsonArray{QJsonObject{
        {QStringLiteral("stationId"), 9}, {QStringLiteral("name"), QStringLiteral("常用站")},
        {QStringLiteral("address"), QStringLiteral("常用路")}}});
    QCOMPARE(list->count(), 1);
    QVERIFY(list->item(0)->text().contains(QStringLiteral("常用站")));

    profile.showFavoritesError(QStringLiteral("请求超时"));
    QVERIFY(list->item(0)->text().contains(QStringLiteral("请求超时")));
    QVERIFY(!retry->isHidden());
    retry->click();
    QCOMPARE(refreshSpy.count(), 1);
    QVERIFY(list->item(0)->text().contains(QStringLiteral("正在加载")));
}

void UserUiTest::favoriteResponseUpdatesHomeStationMarker()
{
    HomePage home;
    home.setStations(QJsonArray{QJsonObject{
        {QStringLiteral("stationId"), 12}, {QStringLiteral("name"), QStringLiteral("标记站")},
        {QStringLiteral("address"), QStringLiteral("测试路")}, {QStringLiteral("favorited"), false},
        {QStringLiteral("latitude"), 22.5}, {QStringLiteral("longitude"), 114.0},
        {QStringLiteral("priceCentsPerKwh"), 150}, {QStringLiteral("availablePiles"), 1},
        {QStringLiteral("totalPiles"), 2}}});
    auto *list = home.findChild<QListWidget *>(QStringLiteral("stationList"));
    QVERIFY(list);
    QVERIFY(list->item(0)->text().startsWith(QStringLiteral("⚡")));
    home.updateFavoriteState(12, true);
    QVERIFY(list->item(0)->text().startsWith(QStringLiteral("★")));
    QCOMPARE(list->item(0)->data(Qt::UserRole).toJsonObject()
                 .value(QStringLiteral("favorited")).toBool(), true);
}

void UserUiTest::avatarIsCompressedAndConfirmed()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("large-avatar.jpg"));
    QImage source(1600, 1200, QImage::Format_RGB32);
    source.fill(QColor(QStringLiteral("#2563eb")));
    QVERIFY(source.save(path, "JPEG", 100));

    ProfilePage profile;
    QSignalSpy uploadSpy(&profile, &ProfilePage::avatarUpdateRequested);
    QVERIFY(profile.updateAvatarFromFile(path));
    QCOMPARE(uploadSpy.count(), 1);
    const QString dataUrl = uploadSpy.first().first().toString();
    QVERIFY(dataUrl.startsWith(QStringLiteral("data:image/png;base64,")));
    const QByteArray bytes = QByteArray::fromBase64(dataUrl.section(',', 1).toLatin1());
    QVERIFY(bytes.size() <= 2 * 1024 * 1024);
    const QImage normalized = QImage::fromData(bytes, "PNG");
    QVERIFY(!normalized.isNull());
    QVERIFY(normalized.width() <= 512);
    QVERIFY(normalized.height() <= 512);
    QVERIFY(!profile.findChild<QPushButton *>(QStringLiteral("avatarButton"))->isEnabled());

    profile.avatarUpdateSucceeded({{QStringLiteral("avatar"), QStringLiteral("avatars/new.png")}});
    QVERIFY(profile.findChild<QPushButton *>(QStringLiteral("avatarButton"))->isEnabled());
    QVERIFY(profile.findChild<QLabel *>(QStringLiteral("profileAccountNote"))
                ->text().contains(QStringLiteral("已保存")));
}

void UserUiTest::avatarFailureRestoresPreviousPreview()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString originalPath = directory.filePath(QStringLiteral("original.png"));
    const QString replacementPath = directory.filePath(QStringLiteral("replacement.png"));
    QImage original(320, 240, QImage::Format_ARGB32);
    original.fill(QColor(QStringLiteral("#16a34a")));
    QVERIFY(original.save(originalPath, "PNG"));
    QImage replacement(320, 240, QImage::Format_ARGB32);
    replacement.fill(QColor(QStringLiteral("#dc2626")));
    QVERIFY(replacement.save(replacementPath, "PNG"));

    ProfilePage profile;
    auto *avatar = profile.findChild<QLabel *>(QStringLiteral("profileAvatar"));
    QVERIFY(avatar);
    QCOMPARE(avatar->text(), QStringLiteral("用户"));
    QVERIFY(profile.updateAvatarFromFile(originalPath));
    QVERIFY(!avatar->pixmap(Qt::ReturnByValue).isNull());
    profile.avatarUpdateSucceeded({{QStringLiteral("avatar"), QStringLiteral("avatars/original.png")}});
    const QColor confirmedColor = avatar->pixmap(Qt::ReturnByValue).toImage().pixelColor(20, 20);

    QVERIFY(profile.updateAvatarFromFile(replacementPath));
    QVERIFY(avatar->pixmap(Qt::ReturnByValue).toImage().pixelColor(20, 20) != confirmedColor);

    profile.avatarUpdateFailed(QStringLiteral("网络错误"));
    QCOMPARE(avatar->pixmap(Qt::ReturnByValue).toImage().pixelColor(20, 20), confirmedColor);
    QVERIFY(profile.findChild<QLabel *>(QStringLiteral("profileAccountNote"))
                ->text().contains(QStringLiteral("已恢复原头像")));
}

void UserUiTest::nicknameValidationAndConfirmation()
{
    ProfilePage profile;
    profile.setProfile({{QStringLiteral("nickname"), QStringLiteral("原昵称")},
                        {QStringLiteral("phone"), QStringLiteral("138****0000")},
                        {QStringLiteral("balanceCents"), 0}});
    auto *edit = profile.findChild<QLineEdit *>(QStringLiteral("nicknameEdit"));
    auto *button = profile.findChild<QPushButton *>(QStringLiteral("saveNicknameButton"));
    auto *label = profile.findChild<QLabel *>(QStringLiteral("profileNickname"));
    QVERIFY(edit);
    QVERIFY(button);
    QVERIFY(label);
    QSignalSpy updateSpy(&profile, &ProfilePage::nicknameUpdateRequested);

    edit->setText(QStringLiteral("非法😀昵称"));
    button->click();
    QCOMPARE(updateSpy.count(), 0);
    QVERIFY(profile.findChild<QLabel *>(QStringLiteral("profileAccountNote"))
                ->text().contains(QStringLiteral("中文、字母、数字")));

    edit->setText(QStringLiteral("原昵称"));
    button->click();
    QCOMPARE(updateSpy.count(), 0);
    QVERIFY(profile.findChild<QLabel *>(QStringLiteral("profileAccountNote"))
                ->text().contains(QStringLiteral("没有变化")));

    edit->setText(QStringLiteral("新昵称_7-test"));
    button->click();
    QCOMPARE(updateSpy.count(), 1);
    QCOMPARE(updateSpy.first().first().toString(), QStringLiteral("新昵称_7-test"));
    QVERIFY(!edit->isEnabled());
    QVERIFY(!button->isEnabled());

    profile.nicknameUpdateSucceeded(
        {{QStringLiteral("nickname"), QStringLiteral("新昵称_7-test")}});
    QVERIFY(edit->isEnabled());
    QVERIFY(button->isEnabled());
    QCOMPARE(edit->text(), QStringLiteral("新昵称_7-test"));
    QCOMPARE(label->text(), QStringLiteral("新昵称_7-test"));
}

void UserUiTest::nicknameFailureRestoresOriginalValue()
{
    ProfilePage profile;
    profile.setProfile({{QStringLiteral("nickname"), QStringLiteral("原昵称")},
                        {QStringLiteral("phone"), QStringLiteral("138****0000")},
                        {QStringLiteral("balanceCents"), 0}});
    auto *edit = profile.findChild<QLineEdit *>(QStringLiteral("nicknameEdit"));
    auto *button = profile.findChild<QPushButton *>(QStringLiteral("saveNicknameButton"));
    auto *label = profile.findChild<QLabel *>(QStringLiteral("profileNickname"));
    edit->setText(QStringLiteral("待保存昵称"));
    button->click();

    profile.nicknameUpdateFailed(QStringLiteral("网络错误"));
    QCOMPARE(edit->text(), QStringLiteral("原昵称"));
    QCOMPARE(label->text(), QStringLiteral("原昵称"));
    QVERIFY(edit->isEnabled());
    QVERIFY(button->isEnabled());
    QVERIFY(profile.findChild<QLabel *>(QStringLiteral("profileAccountNote"))
                ->text().contains(QStringLiteral("已恢复原昵称")));
}

void UserUiTest::rechargeUsesIntegerCentsAndPreventsDuplicates()
{
    ProfilePage profile;
    profile.setProfile({{QStringLiteral("nickname"), QStringLiteral("测试用户")},
                        {QStringLiteral("phone"), QStringLiteral("138****0000")},
                        {QStringLiteral("balanceCents"), 1000}});
    auto *amount = profile.findChild<QDoubleSpinBox *>(QStringLiteral("rechargeAmount"));
    auto *button = profile.findChild<QPushButton *>(QStringLiteral("rechargeButton"));
    auto *balance = profile.findChild<QLabel *>(QStringLiteral("profileBalance"));
    QVERIFY(amount);
    QVERIFY(button);
    QCOMPARE(button->text(), QStringLiteral("充值"));
    QCOMPARE(amount->minimum(), 1.0);
    QCOMPARE(amount->maximum(), 5000.0);
    QSignalSpy rechargeSpy(&profile, &ProfilePage::rechargeRequested);

    amount->setValue(12.34);
    button->click();
    QCOMPARE(rechargeSpy.count(), 1);
    QCOMPARE(rechargeSpy.first().first().toLongLong(), 1234);
    QVERIFY(!amount->isEnabled());
    QVERIFY(!button->isEnabled());
    button->click();
    QCOMPARE(rechargeSpy.count(), 1);

    profile.applyWalletResult({{QStringLiteral("balanceCents"), 2234}});
    QVERIFY(amount->isEnabled());
    QVERIFY(button->isEnabled());
    QCOMPARE(button->text(), QStringLiteral("充值"));
    QVERIFY(balance->text().contains(QStringLiteral("22.34")));
}

void UserUiTest::rechargeFailureKeepsDisplayedBalance()
{
    ProfilePage profile;
    profile.setProfile({{QStringLiteral("nickname"), QStringLiteral("测试用户")},
                        {QStringLiteral("phone"), QStringLiteral("138****0000")},
                        {QStringLiteral("balanceCents"), 28650}});
    auto *amount = profile.findChild<QDoubleSpinBox *>(QStringLiteral("rechargeAmount"));
    auto *button = profile.findChild<QPushButton *>(QStringLiteral("rechargeButton"));
    auto *balance = profile.findChild<QLabel *>(QStringLiteral("profileBalance"));
    const QString balanceBefore = balance->text();
    amount->setValue(5000.0);
    button->click();

    profile.rechargeFailed(QStringLiteral("服务器连接已断开"));
    QCOMPARE(balance->text(), balanceBefore);
    QVERIFY(amount->isEnabled());
    QVERIFY(button->isEnabled());
    QVERIFY(profile.findChild<QLabel *>(QStringLiteral("profileAccountNote"))
                ->text().contains(QStringLiteral("页面余额未更新")));
}

void UserUiTest::ledgerStatesAndMissingFieldsAreReadable()
{
    ProfilePage profile;
    auto *ledger = profile.findChild<QListWidget *>(QStringLiteral("walletLedgerList"));
    auto *retry = profile.findChild<QPushButton *>(QStringLiteral("ledgerRetryButton"));
    QVERIFY(ledger);
    QVERIFY(retry);

    profile.showLedgerLoading();
    QCOMPARE(ledger->count(), 1);
    QVERIFY(ledger->item(0)->text().contains(QStringLiteral("正在同步")));
    QVERIFY(retry->isHidden());

    profile.setLedger(QJsonArray{
        QJsonObject{{QStringLiteral("createdAt"), QStringLiteral("2026-09-09 10:00")},
                    {QStringLiteral("amountCents"), 1234},
                    {QStringLiteral("balanceAfterCents"), 5678}},
        QJsonObject{{QStringLiteral("recordId"), 2}}});
    QCOMPARE(ledger->count(), 2);
    QVERIFY(ledger->item(0)->text().contains(QStringLiteral("+¥12.34")));
    QVERIFY(ledger->item(0)->text().contains(QStringLiteral("余额 ¥56.78")));
    QVERIFY(ledger->item(1)->text().contains(QStringLiteral("时间未知")));
    QVERIFY(ledger->item(1)->text().contains(QStringLiteral("金额未知")));
    QVERIFY(ledger->item(1)->text().contains(QStringLiteral("余额未知")));

    profile.setLedger({});
    QCOMPARE(ledger->count(), 1);
    QCOMPARE(ledger->item(0)->text(), QStringLiteral("暂无钱包流水"));
}

void UserUiTest::ledgerFailureOffersRetry()
{
    ProfilePage profile;
    auto *ledger = profile.findChild<QListWidget *>(QStringLiteral("walletLedgerList"));
    auto *retry = profile.findChild<QPushButton *>(QStringLiteral("ledgerRetryButton"));
    QSignalSpy refreshSpy(&profile, &ProfilePage::ledgerRefreshRequested);

    profile.showLedgerError(QStringLiteral("请求超时"));
    QVERIFY(ledger->item(0)->text().contains(QStringLiteral("请求超时")));
    QVERIFY(!retry->isHidden());
    retry->click();
    QCOMPARE(refreshSpy.count(), 1);
    QVERIFY(ledger->item(0)->text().contains(QStringLiteral("正在同步")));
    QVERIFY(retry->isHidden());
}

void UserUiTest::orderHistoryTabRequestsRefresh()
{
    ProfilePage profile;
    auto *tabs = profile.findChild<QTabWidget *>(QStringLiteral("profileTabs"));
    auto *orders = profile.findChild<QListWidget *>(QStringLiteral("orderHistoryList"));
    QSignalSpy refreshSpy(&profile, &ProfilePage::orderHistoryRefreshRequested);
    QVERIFY(tabs);
    QVERIFY(orders);

    tabs->setCurrentIndex(3);
    QCOMPARE(refreshSpy.count(), 1);
    QCOMPARE(orders->count(), 1);
    QVERIFY(orders->item(0)->text().contains(QStringLiteral("正在加载订单")));
}

void UserUiTest::addressInputIsBoundedAndReturnStartsSearch()
{
    HomePage home;
    auto *address = home.findChild<QLineEdit *>(QStringLiteral("stationAddressInput"));
    QSignalSpy searchSpy(&home, &HomePage::stationsRequested);
    QVERIFY(address);
    QCOMPARE(address->maxLength(), 200);

    address->setText(QString(240, QChar(0x6DF1)));
    QCOMPARE(address->text().size(), 200);
    QTest::keyClick(address, Qt::Key_Return);
    QCOMPARE(searchSpy.count(), 1);
    QCOMPARE(searchSpy.first().at(1).toString().size(), 200);
}

void UserUiTest::textSearchFallbackDoesNotInventDistance()
{
    HomePage home;
    home.showTextSearchFallback(QStringLiteral("已改用文本搜索"));
    home.setStations(QJsonArray{QJsonObject{
        {QStringLiteral("stationId"), 1},
        {QStringLiteral("name"), QStringLiteral("深圳北站充电站")},
        {QStringLiteral("address"), QStringLiteral("深圳北站西广场")},
        {QStringLiteral("latitude"), 22.6099},
        {QStringLiteral("longitude"), 114.0295},
        {QStringLiteral("priceCentsPerKwh"), 168},
        {QStringLiteral("availablePiles"), 3},
        {QStringLiteral("totalPiles"), 6}}});

    auto *stations = home.findChild<QListWidget *>(QStringLiteral("stationList"));
    auto *status = home.findChild<QLabel *>(QStringLiteral("stationSearchStatus"));
    QCOMPARE(stations->count(), 1);
    QVERIFY(stations->item(0)->text().contains(QStringLiteral("距离未知")));
    QVERIFY(!stations->item(0)->text().contains(QStringLiteral("0.0 km")));
    QVERIFY(status->text().contains(QStringLiteral("文本匹配")));
}

void UserUiTest::addressFocusAdvertisesTextInputMethod()
{
    HomePage home;
    home.show();
    auto *address = home.findChild<QLineEdit *>(QStringLiteral("stationAddressInput"));
    QVERIFY(address);
    address->setFocus(Qt::MouseFocusReason);
    QTRY_VERIFY(address->hasFocus());
    QVERIFY(address->testAttribute(Qt::WA_InputMethodEnabled));
    QCOMPARE(address->inputMethodHints(), Qt::InputMethodHints(Qt::ImhNone));
    QVERIFY(QInputMethod::queryFocusObject(Qt::ImEnabled, {}).toBool());
}

QTEST_MAIN(UserUiTest)

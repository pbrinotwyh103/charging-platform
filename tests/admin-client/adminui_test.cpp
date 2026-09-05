#include "adminui_test.h"

#include "charts/revenuechartwidget.h"
#include "pages/assetspage.h"
#include "pages/monitorpage.h"
#include "pages/overviewpage.h"
#include "pages/recordspage.h"
#include "protocol/errorcodes.h"
#include "ui/adminmainwindow.h"
#include "widgets/paginationbar.h"

#include <QFile>
#include <QDateEdit>
#include <QColor>
#include <QGroupBox>
#include <QHeaderView>
#include <QDir>
#include <QJsonArray>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSignalSpy>
#include <QStackedWidget>
#include <QStringList>
#include <QTabBar>
#include <QTabWidget>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QtTest>

void AdminUiTest::demoWorkspaceContainsAllModules() {
  AdminMainWindow window;
  window.showDemoWorkspace();
  window.resize(360, 640);

  auto *pages =
      window.findChild<QStackedWidget *>(QStringLiteral("authenticationPages"));
  auto *navigation =
      window.findChild<QTabBar *>(QStringLiteral("bottomNavigation"));
  QVERIFY(pages);
  QVERIFY(navigation);
  QCOMPARE(pages->currentIndex(), 1);
  QCOMPARE(navigation->count(), 5);
  QCOMPARE(window.minimumWidth(), 360);
  QCOMPARE(window.findChild<QTableWidget *>(QStringLiteral("chargingTable"))
               ->rowCount(),
           2);
  QCOMPARE(window.findChild<QTableWidget *>(QStringLiteral("alarmTable"))
               ->rowCount(),
           2);
  QCOMPARE(window.findChild<QTableWidget *>(QStringLiteral("stationTable"))
               ->rowCount(),
           2);
  QCOMPARE(
      window.findChild<QTableWidget *>(QStringLiteral("pileTable"))->rowCount(),
      2);
  QCOMPARE(
      window.findChild<QTableWidget *>(QStringLiteral("userTable"))->rowCount(),
      2);
  QCOMPARE(window.findChild<QTableWidget *>(QStringLiteral("orderTable"))
               ->rowCount(),
           2);
  auto *revenueChart = window.findChild<RevenueChartWidget *>();
  auto *thirtyDaysButton =
      window.findChild<QPushButton *>(QStringLiteral("thirtyDaysButton"));
  QVERIFY(revenueChart);
  QVERIFY(thirtyDaysButton);
  QTest::mouseClick(thirtyDaysButton, Qt::LeftButton);
  QCOMPARE(revenueChart->days(), 30);
  QCOMPARE(revenueChart->pointCount(), 30);
}

void AdminUiTest::mobileLayoutIsTouchFriendly() {
  AdminMainWindow window;
  window.resize(360, 640);
  window.showDemoWorkspace();
  window.show();
  QCoreApplication::processEvents();

  const QStringList tableNames = {
      QStringLiteral("chargingTable"), QStringLiteral("alarmTable"),
      QStringLiteral("stationTable"),  QStringLiteral("pileTable"),
      QStringLiteral("userTable"),     QStringLiteral("orderTable")};
  for (const QString &name : tableNames) {
    auto *table = window.findChild<QTableWidget *>(name);
    QVERIFY2(table, qPrintable(name));
    QCOMPARE(table->horizontalScrollBarPolicy(), Qt::ScrollBarAlwaysOff);
    QVERIFY(!table->showGrid());
    QVERIFY(table->verticalHeader()->defaultSectionSize() >= 52);
    int visibleColumns = 0;
    for (int column = 0; column < table->columnCount(); ++column) {
      if (!table->isColumnHidden(column))
        ++visibleColumns;
    }
    QVERIFY2(visibleColumns <= 3, qPrintable(name));
  }

  auto *navigation =
      window.findChild<QTabBar *>(QStringLiteral("bottomNavigation"));
  QVERIFY(navigation);
  QVERIFY(navigation->minimumHeight() >= 52);
  for (int index = 0; index < navigation->count(); ++index)
    QVERIFY(!navigation->tabIcon(index).isNull());
  auto *workspaceTitle =
      window.findChild<QLabel *>(QStringLiteral("workspaceTitle"));
  QVERIFY(workspaceTitle);
  navigation->setCurrentIndex(2);
  QCOMPARE(workspaceTitle->text(), QStringLiteral("异常告警"));
  auto *nextPage =
      window.findChild<QPushButton *>(QStringLiteral("nextPageButton"));
  QVERIFY(nextPage);
  QVERIFY(nextPage->width() >= 44);
  QVERIFY(nextPage->height() >= 44);

  navigation->setCurrentIndex(1);
  QCoreApplication::processEvents();
  auto *chargingTable =
      window.findChild<QTableWidget *>(QStringLiteral("chargingTable"));
  auto *monitorState =
      window.findChild<QLabel *>(QStringLiteral("monitorStateLabel"));
  chargingTable->selectRow(0);
  QCoreApplication::processEvents();
  QVERIFY(monitorState->text().contains(QStringLiteral("kWh")));

  navigation->setCurrentIndex(3);
  auto *assetTabs =
      window.findChild<QTabWidget *>(QStringLiteral("assetTabs"));
  auto *pileDetailBox =
      window.findChild<QGroupBox *>(QStringLiteral("pileDetailBox"));
  QVERIFY(assetTabs);
  QVERIFY(pileDetailBox);
  assetTabs->setCurrentIndex(1);
  QCoreApplication::processEvents();
  QVERIFY(!pileDetailBox->isVisible());
  auto *pileTable =
      window.findChild<QTableWidget *>(QStringLiteral("pileTable"));
  QVERIFY(pileTable);
  QCOMPARE(pileTable->item(0, 4)->foreground().color(),
           QColor(QStringLiteral("#1d4ed8")));
  pileTable->selectRow(0);
  QCoreApplication::processEvents();
  QVERIFY(pileDetailBox->isVisible());
  pileTable->setCurrentCell(-1, -1);
  pileTable->clearSelection();
  QCoreApplication::processEvents();
  QVERIFY(!pileDetailBox->isVisible());

  navigation->setCurrentIndex(4);
  auto *recordTabs =
      window.findChild<QTabWidget *>(QStringLiteral("recordTabs"));
  QVERIFY(recordTabs);
  recordTabs->setCurrentIndex(1);
  QCoreApplication::processEvents();
  auto *fromDate =
      window.findChild<QDateEdit *>(QStringLiteral("orderFromDate"));
  auto *toDate =
      window.findChild<QDateEdit *>(QStringLiteral("orderToDate"));
  QVERIFY(fromDate);
  QVERIFY(toDate);
  QVERIFY(!fromDate->isVisible());
  QVERIFY(!toDate->isVisible());

  const QString screenshotDirectory =
      qEnvironmentVariable("ADMIN_UI_TEST_SCREENSHOT_DIR");
  if (!screenshotDirectory.isEmpty()) {
    QDir directory;
    QVERIFY(directory.mkpath(screenshotDirectory));
    directory.setPath(screenshotDirectory);
    AdminMainWindow loginWindow;
    loginWindow.resize(360, 640);
    loginWindow.show();
    QCoreApplication::processEvents();
    QVERIFY(loginWindow.grab().save(
        directory.filePath(QStringLiteral("admin-login.png"))));
    const QStringList pageNames = {
        QStringLiteral("overview"), QStringLiteral("charging"),
        QStringLiteral("alarms"), QStringLiteral("assets"),
        QStringLiteral("records")};
    assetTabs->setCurrentIndex(0);
    recordTabs->setCurrentIndex(0);
    for (int index = 0; index < pageNames.size(); ++index) {
      navigation->setCurrentIndex(index);
      QCoreApplication::processEvents();
      QVERIFY(window.grab().save(directory.filePath(
          QStringLiteral("admin-%1.png").arg(pageNames.at(index)))));
    }

    navigation->setCurrentIndex(3);
    assetTabs->setCurrentIndex(1);
    QCoreApplication::processEvents();
    QVERIFY(window.grab().save(
        directory.filePath(QStringLiteral("admin-assets-piles.png"))));

    navigation->setCurrentIndex(4);
    recordTabs->setCurrentIndex(1);
    QCoreApplication::processEvents();
    QVERIFY(window.grab().save(
        directory.filePath(QStringLiteral("admin-records-orders.png"))));
  }
}

void AdminUiTest::overviewRendersMetricsAndTrend() {
  OverviewPage page;
  page.setSummary(
      {{QStringLiteral("data"),
        QJsonObject{
            {QStringLiteral("revenueMetrics"),
             QJsonObject{{QStringLiteral("todayRevenueCents"), 12345},
                         {QStringLiteral("monthRevenueCents"), 250000},
                         {QStringLiteral("totalRevenueCents"), 880000}}},
            {QStringLiteral("orderMetrics"),
             QJsonObject{{QStringLiteral("todayOrderCount"), 7},
                         {QStringLiteral("monthOrderCount"), 81},
                         {QStringLiteral("totalOrderCount"), 420}}},
            {QStringLiteral("pileStatus"),
             QJsonObject{{QStringLiteral("idle"), 5},
                         {QStringLiteral("charging"), 3}}}}}});
  page.setRevenue(
      {{QStringLiteral("data"),
        QJsonObject{
            {QStringLiteral("points"),
             QJsonArray{
                 QJsonObject{
                     {QStringLiteral("date"), QStringLiteral("2026-09-04")},
                     {QStringLiteral("revenueCents"), 12000}},
                 QJsonObject{
                     {QStringLiteral("date"), QStringLiteral("2026-09-05")},
                     {QStringLiteral("revenueCents"), 18000}}}}}}});

  QCOMPARE(
      page.findChild<QLabel *>(QStringLiteral("todayRevenueValue"))->text(),
      QStringLiteral("¥123.45"));
  QCOMPARE(page.findChild<QLabel *>(QStringLiteral("monthOrderValue"))->text(),
           QStringLiteral("81 单"));
  QCOMPARE(page.findChild<RevenueChartWidget *>()->pointCount(), 2);
}

void AdminUiTest::chargingPushUpdatesMatchingOrder() {
  MonitorPage page;
  page.setChargingData(
      {{QStringLiteral("data"),
        QJsonObject{{QStringLiteral("items"),
                     QJsonArray{QJsonObject{
                         {QStringLiteral("orderId"), 9},
                         {QStringLiteral("orderNo"), QStringLiteral("ORDER-9")},
                         {QStringLiteral("energyWh"), 1000},
                         {QStringLiteral("powerKw"), 7.0},
                         {QStringLiteral("durationSeconds"), 60},
                         {QStringLiteral("feeCents"), 120}}}},
                    {QStringLiteral("meta"),
                     QJsonObject{{QStringLiteral("page"), 1},
                                 {QStringLiteral("totalPages"), 1}}}}}});
  page.applyProgressPush(
      {{QStringLiteral("orderId"), 9},
       {QStringLiteral("energyWh"), 2500},
       {QStringLiteral("powerKw"), 6.8},
       {QStringLiteral("durationSeconds"), 125},
       {QStringLiteral("feeCents"), 300},
       {QStringLiteral("updatedAt"), QStringLiteral("10:30:00")}});
  auto *table = page.findChild<QTableWidget *>(QStringLiteral("chargingTable"));
  QCOMPARE(table->item(0, 3)->text(), QStringLiteral("2.50"));
  QCOMPARE(table->item(0, 5)->text(), QStringLiteral("00:02:05"));
  QCOMPARE(table->item(0, 6)->text(), QStringLiteral("¥3.00"));
}

void AdminUiTest::assetsRenderStationsPilesAndDevicePush() {
  AssetsPage page;
  page.setStations(
      {{QStringLiteral("data"),
        QJsonObject{
            {QStringLiteral("items"),
             QJsonArray{QJsonObject{
                 {QStringLiteral("stationId"), 1},
                 {QStringLiteral("name"), QStringLiteral("软件园站")},
                 {QStringLiteral("address"), QStringLiteral("软件园路")},
                 {QStringLiteral("priceCentsPerKwh"), 120},
                 {QStringLiteral("status"), QStringLiteral("online")}}}},
            {QStringLiteral("meta"),
             QJsonObject{{QStringLiteral("page"), 1},
                         {QStringLiteral("totalPages"), 1}}}}}});
  page.setPiles(
      {{QStringLiteral("data"),
        QJsonObject{{QStringLiteral("items"),
                     QJsonArray{QJsonObject{
                         {QStringLiteral("pileId"), 2},
                         {QStringLiteral("pileCode"), QStringLiteral("PILE-2")},
                         {QStringLiteral("chargeType"), QStringLiteral("fast")},
                         {QStringLiteral("powerKw"), 60.0},
                         {QStringLiteral("status"), QStringLiteral("idle")}}}},
                    {QStringLiteral("meta"),
                     QJsonObject{{QStringLiteral("page"), 1},
                                 {QStringLiteral("totalPages"), 1}}}}}});
  page.applyDevicePush(
      {{QStringLiteral("pileId"), 2},
       {QStringLiteral("status"), QStringLiteral("fault")},
       {QStringLiteral("updatedAt"), QStringLiteral("10:31")}});
  auto *stationTable =
      page.findChild<QTableWidget *>(QStringLiteral("stationTable"));
  auto *pileTable = page.findChild<QTableWidget *>(QStringLiteral("pileTable"));
  QCOMPARE(stationTable->rowCount(), 1);
  QCOMPARE(stationTable->item(0, 2)->text(), QStringLiteral("¥1.20"));
  QCOMPARE(pileTable->item(0, 4)->text(), QStringLiteral("故障"));
}

void AdminUiTest::phoneSearchIsDebouncedAndNumeric() {
  RecordsPage page;
  QSignalSpy requests(&page, &RecordsPage::commandRequested);
  auto *search = page.findChild<QLineEdit *>(QStringLiteral("userPhoneSearch"));
  QVERIFY(search);
  search->setFocus();
  QTest::keyClicks(search, QStringLiteral("138abc"));
  QCOMPARE(search->text(), QStringLiteral("138"));
  search->setText(QStringLiteral("1380013"));
  QTRY_COMPARE_WITH_TIMEOUT(requests.count(), 1, 1'000);
  QCOMPARE(requests.at(0).at(0).toString(), QStringLiteral("users.list"));
  const QJsonObject parameters =
      qvariant_cast<QJsonObject>(requests.at(0).at(1));
  QCOMPARE(parameters.value(QStringLiteral("phone")).toString(),
           QStringLiteral("1380013"));
  QCOMPARE(parameters.value(QStringLiteral("page")).toInt(), 1);
}

void AdminUiTest::csvExportIsUtf8AndEscaped() {
  RecordsPage page;
  page.setOrders(
      {{QStringLiteral("data"),
        QJsonObject{
            {QStringLiteral("items"),
             QJsonArray{QJsonObject{
                 {QStringLiteral("orderNo"), QStringLiteral("ORDER-1")},
                 {QStringLiteral("phone"), QStringLiteral("13800138000")},
                 {QStringLiteral("stationName"), QStringLiteral("软件园,东区")},
                 {QStringLiteral("pileCode"), QStringLiteral("PILE-1")},
                 {QStringLiteral("status"), QStringLiteral("completed")},
                 {QStringLiteral("energyWh"), 12500},
                 {QStringLiteral("durationSeconds"), 600},
                 {QStringLiteral("feeCents"), 1500}}}},
            {QStringLiteral("meta"),
             QJsonObject{{QStringLiteral("page"), 1},
                         {QStringLiteral("totalPages"), 1}}}}}});
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const QString path = directory.filePath(QStringLiteral("orders.csv"));
  QString error;
  QVERIFY2(page.exportOrdersCsv(path, &error), qPrintable(error));
  QFile file(path);
  QVERIFY(file.open(QIODevice::ReadOnly));
  const QByteArray bytes = file.readAll();
  QVERIFY(bytes.startsWith(QByteArray::fromHex("efbbbf")));
  QVERIFY(bytes.contains(QStringLiteral("\"软件园,东区\"").toUtf8()));
}

void AdminUiTest::paginationHonorsBoundaries() {
  PaginationBar bar;
  QSignalSpy pages(&bar, &PaginationBar::pageRequested);
  bar.setPage(2, 3, 31);
  QTest::mouseClick(
      bar.findChild<QPushButton *>(QStringLiteral("nextPageButton")),
      Qt::LeftButton);
  QCOMPARE(pages.count(), 1);
  QCOMPARE(pages.at(0).at(0).toInt(), 3);
  bar.setPage(3, 3, 31);
  QVERIFY(!bar.findChild<QPushButton *>(QStringLiteral("nextPageButton"))
               ->isEnabled());
}

void AdminUiTest::emptyListsShowStableEmptyState() {
  RecordsPage page;
  page.setUsers({{QStringLiteral("data"),
                  QJsonObject{{QStringLiteral("items"), QJsonArray{}},
                              {QStringLiteral("meta"),
                               QJsonObject{{QStringLiteral("page"), 1},
                                           {QStringLiteral("totalPages"), 0},
                                           {QStringLiteral("total"), 0}}}}}});
  QCOMPARE(
      page.findChild<QTableWidget *>(QStringLiteral("userTable"))->rowCount(),
      0);
  QCOMPARE(page.findChild<QLabel *>(QStringLiteral("userStateLabel"))->text(),
           QStringLiteral("没有匹配的用户"));
}

void AdminUiTest::sessionExpiryReturnsToLoginWithReason() {
  AdminMainWindow window;
  window.showDemoWorkspace();
  window.handleCommandFailed(
      QStringLiteral("users.list"), QStringLiteral("管理员会话已过期"),
      static_cast<int>(Charging::ErrorCode::SessionExpired));

  auto *pages =
      window.findChild<QStackedWidget *>(QStringLiteral("authenticationPages"));
  auto *loginError =
      window.findChild<QLabel *>(QStringLiteral("loginErrorLabel"));
  QVERIFY(pages);
  QVERIFY(loginError);
  QCOMPARE(pages->currentIndex(), 0);
  QCOMPARE(loginError->text(), QStringLiteral("管理员会话已过期"));
}

QTEST_MAIN(AdminUiTest)

#pragma once

#include <QObject>

class AdminUiTest final : public QObject {
  Q_OBJECT

private slots:
  void demoWorkspaceContainsAllModules();
  void mobileLayoutIsTouchFriendly();
  void overviewRendersMetricsAndTrend();
  void chargingPushUpdatesMatchingOrder();
  void assetsRenderStationsPilesAndDevicePush();
  void phoneSearchIsDebouncedAndNumeric();
  void csvExportIsUtf8AndEscaped();
  void paginationHonorsBoundaries();
  void emptyListsShowStableEmptyState();
  void sessionExpiryReturnsToLoginWithReason();
};

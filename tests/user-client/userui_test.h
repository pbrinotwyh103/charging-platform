#pragma once

#include <QObject>

class UserUiTest final : public QObject
{
    Q_OBJECT

private slots:
    void demoWorkspaceShowsCoreFeatures();
    void reservationCanEnterChargingAndSettle();
    void chargingUpdatesDoNotInterruptNavigation();
    void demoRechargeUpdatesBalanceAndLedger();
    void mapUrlCarriesTravelModeAndCoordinates();
    void mapNavigationRejectsUnsafeUrlsAndCoordinates();
    void travelModeOpensStandaloneNavigationPage();
    void navigationPageReportsLoadResultAndRetry();
    void invalidCoordinatesSortLastAndRemainStable();
    void stationCardsHandleMissingFieldsAndRefreshLifecycle();
    void pileDetailsShowAvailabilityAndRefreshLifecycle();
    void userWindowAdaptsBetweenCompactAndWideLayouts();
    void mapKeyCanBeLoadedFromWorkingDirectoryConfig();
    void geocoderBuildsTencentRequestAndParsesCoordinates();
    void geocoderInvalidatesSupersededRequests();
    void geocodingStatusOffersExplicitRetry();
    void addressInputAcceptsChineseInputMethodText();
    void regionSelectorCoversShenzhenDistricts();
    void emptyStationDoesNotRequestPiles();
    void stationSearchUsesSeedDataCityByDefault();
    void simulatedLocationDoesNotFilterByDisplayText();
    void compactAppVisualShellIsPresent();
    void favoriteResponseUpdatesProfileList();
    void favoriteToggleDisablesAndRollsBack();
    void favoriteListHasLoadingErrorAndRetryStates();
    void favoriteResponseUpdatesHomeStationMarker();
    void avatarIsCompressedAndConfirmed();
    void avatarFailureRestoresPreviousPreview();
    void nicknameValidationAndConfirmation();
    void nicknameFailureRestoresOriginalValue();
    void rechargeUsesIntegerCentsAndPreventsDuplicates();
    void rechargeFailureKeepsDisplayedBalance();
    void ledgerStatesAndMissingFieldsAreReadable();
    void ledgerFailureOffersRetry();
    void orderHistoryTabRequestsRefresh();
    void addressInputIsBoundedAndReturnStartsSearch();
    void textSearchFallbackDoesNotInventDistance();
    void addressFocusAdvertisesTextInputMethod();
};

#pragma once

#include <QObject>

class UserUiTest final : public QObject
{
    Q_OBJECT

private slots:
    void demoWorkspaceShowsCoreFeatures();
    void reservationCanEnterChargingAndSettle();
    void demoRechargeUpdatesBalanceAndLedger();
    void mapUrlCarriesTravelModeAndCoordinates();
    void mapKeyCanBeLoadedFromWorkingDirectoryConfig();
    void emptyStationDoesNotRequestPiles();
    void stationSearchUsesSeedDataCityByDefault();
    void simulatedLocationDoesNotFilterByDisplayText();
    void compactAppVisualShellIsPresent();
};

#pragma once

#include <QtGlobal>

namespace Charging {

enum class MessageType : quint16 {
    Invalid = 0,
    Ping = 1,
    Pong = 2,

    UserLoginRequest = 1001,
    UserLoginResponse = 1002,
    UserProfileRequest = 1010,
    UserProfileResponse = 1011,
    UserProfileUpdateRequest = 1020,
    UserProfileUpdateResponse = 1021,
    LogoutRequest = 1090,
    LogoutResponse = 1091,

    WalletRechargeRequest = 1100,
    WalletRechargeResponse = 1101,
    WalletLedgerRequest = 1110,
    WalletLedgerResponse = 1111,

    StationListRequest = 2001,
    StationListResponse = 2002,
    PileListRequest = 2010,
    PileListResponse = 2011,
    FavoriteToggleRequest = 2020,
    FavoriteToggleResponse = 2021,

    ReservationCreateRequest = 3001,
    ReservationCreateResponse = 3002,
    ChargingStartRequest = 3010,
    ChargingStartResponse = 3011,
    ChargingStopRequest = 3020,
    ChargingStopResponse = 3021,
    ActiveOrderRequest = 3030,
    ActiveOrderResponse = 3031,
    ReservationCancelRequest = 3040,
    ReservationCancelResponse = 3041,

    AdminLoginRequest = 5001,
    AdminLoginResponse = 5002,
    AdminCommandRequest = 5010,
    AdminCommandResponse = 5011,

    ChargingProgressPush = 8001,
    ChargingStoppedPush = 8002,
    AlarmPush = 8010,
    DeviceStatusPush = 8020
};

} // namespace Charging

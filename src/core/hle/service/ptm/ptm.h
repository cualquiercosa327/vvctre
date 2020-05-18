// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include "common/common_types.h"
#include "core/hle/ipc_helpers.h"
#include "core/hle/service/service.h"

namespace Core {
class System;
}

namespace Service::PTM {

/// Id of the SharedExtData archive used by the PTM process
constexpr std::array<u8, 12> ptm_shared_extdata_id = {0, 0, 0, 0, 0x0B, 0, 0, 0xF0, 0, 0, 0, 0};

/// Charge levels used by PTM functions
enum class ChargeLevels : u32 {
    CriticalBattery = 1,
    LowBattery = 2,
    HalfFull = 3,
    MostlyFull = 4,
    CompletelyFull = 5,
};

/**
 * Represents the gamecoin file structure in the SharedExtData archive
 * More information in 3dbrew
 * (http://www.3dbrew.org/wiki/Extdata#Shared_Extdata_0xf000000b_gamecoin.dat)
 */
struct GameCoin {
    u32 magic;               ///< Magic number: 0x4F00
    u16 total_coins;         ///< Total Play Coins
    u16 total_coins_on_date; ///< Total Play Coins obtained on the date stored below.
    u32 step_count;          ///< Total step count at the time a new Play Coin was obtained.
    u32 last_step_count;     ///< Step count for the day the last Play Coin was obtained
    u16 year;
    u8 month;
    u8 day;
};

class Module final {
public:
    Module();
    static u16 GetPlayCoins();
    static void SetPlayCoins(u16 play_coins);

    class Interface : public ServiceFramework<Interface> {
    public:
        Interface(std::shared_ptr<Module> ptm, const char* name, u32 max_session);

    protected:
        void GetAdapterState(Kernel::HLERequestContext& ctx);
        void GetShellState(Kernel::HLERequestContext& ctx);
        void GetBatteryLevel(Kernel::HLERequestContext& ctx);
        void GetBatteryChargeState(Kernel::HLERequestContext& ctx);
        void GetPedometerState(Kernel::HLERequestContext& ctx);
        void GetStepHistory(Kernel::HLERequestContext& ctx);
        void GetTotalStepCount(Kernel::HLERequestContext& ctx);
        void GetSoftwareClosedFlag(Kernel::HLERequestContext& ctx);
        void CheckNew3DS(Kernel::HLERequestContext& ctx);

    private:
        std::shared_ptr<Module> ptm;
    };

private:
    bool shell_open = true;
    bool battery_is_charging = true;
    bool pedometer_is_counting = false;
};

void InstallInterfaces(Core::System& system);

} // namespace Service::PTM

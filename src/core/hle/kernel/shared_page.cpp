// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <chrono>
#include <cstring>
#include "common/assert.h"
#include "core/core.h"
#include "core/core_timing.h"
#include "core/hle/kernel/shared_page.h"
#include "core/hle/service/ptm/ptm.h"
#include "core/movie.h"
#include "core/settings.h"

////////////////////////////////////////////////////////////////////////////////////////////////////

namespace SharedPage {

static std::chrono::seconds GetInitialTime() {
    const u64 override_initial_time = Core::Movie::GetInstance().GetOverrideInitialTime();
    if (override_initial_time != 0) {
        // Override the initial time with the one in the movie
        return std::chrono::seconds(override_initial_time);
    }

    switch (Settings::values.initial_clock) {
    case Settings::InitialClock::SystemTime: {
        auto now = std::chrono::system_clock::now();
        // If the system time is in daylight saving, we give an additional hour to console time
        std::time_t now_time_t = std::chrono::system_clock::to_time_t(now);
        std::tm* now_tm = std::localtime(&now_time_t);
        if (now_tm && now_tm->tm_isdst > 0)
            now = now + std::chrono::hours(1);
        return std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch());
    }
    case Settings::InitialClock::FixedTime:
        return std::chrono::seconds(Settings::values.unix_timestamp);
    default:
        UNREACHABLE_MSG("Invalid InitialClock value ({})",
                        static_cast<u32>(Settings::values.initial_clock));
    }
}

Handler::Handler(Core::Timing& timing) : timing(timing) {
    std::memset(&shared_page, 0, sizeof(shared_page));

    shared_page.running_hw = 0x1; // Product
    shared_page.wifi_link_level = static_cast<u8>(WifiLinkLevel::Best);
    shared_page.network_state = static_cast<u8>(NetworkState::Internet);

    // Some games wait until this value becomes 0x1, before asking running_hw
    shared_page.unknown_value = 0x1;

    // Set to a completely full battery
    shared_page.battery_state.charge_level.Assign(
        static_cast<u8>(Service::PTM::ChargeLevels::CompletelyFull));
    shared_page.battery_state.is_adapter_connected.Assign(1);
    shared_page.battery_state.is_charging.Assign(1);

    unix_timestamp = GetInitialTime();

    using namespace std::placeholders;
    update_time_event = timing.RegisterEvent("SharedPage::UpdateTimeCallback",
                                             std::bind(&Handler::UpdateTimeCallback, this, _1, _2));
    timing.ScheduleEvent(0, update_time_event);

    float sliderstate = Settings::values.factor_3d / 100.0f;
    shared_page.sliderstate_3d = static_cast<float_le>(sliderstate);
}

/// Gets system time in 3DS format. The epoch is Jan 1900, and the unit is millisecond.
u64 Handler::GetSystemTime() const {
    std::chrono::milliseconds now =
        unix_timestamp +
        std::chrono::duration_cast<std::chrono::milliseconds>(timing.GetGlobalTimeUs());

    // 3DS system does't allow user to set a time before Jan 1 2000,
    // so we use it as an auxiliary epoch to calculate the console time.
    std::tm epoch_tm;
    epoch_tm.tm_sec = 0;
    epoch_tm.tm_min = 0;
    epoch_tm.tm_hour = 0;
    epoch_tm.tm_mday = 1;
    epoch_tm.tm_mon = 0;
    epoch_tm.tm_year = 100;
    epoch_tm.tm_isdst = 0;
    u64 epoch = std::mktime(&epoch_tm) * 1000;

    // 3DS console time uses Jan 1 1900 as internal epoch,
    // so we use the milliseconds between 1900 and 2000 as base console time
    u64 console_time = 3155673600000ULL;

    // Only when system time is after 2000, we set it as 3DS system time
    if (now.count() > epoch) {
        console_time += (now.count() - epoch);
    }

    return console_time;
}

void Handler::UpdateTimeCallback(u64 userdata, int cycles_late) {
    DateTime& date_time =
        shared_page.date_time_counter % 2 ? shared_page.date_time_0 : shared_page.date_time_1;

    date_time.date_time = GetSystemTime();
    date_time.update_tick = timing.GetTicks();
    date_time.tick_to_second_coefficient = BASE_CLOCK_RATE_ARM11;
    date_time.tick_offset = 0;

    ++shared_page.date_time_counter;

    // System time is updated hourly
    timing.ScheduleEvent(msToCycles(60 * 60 * 1000) - cycles_late, update_time_event);
}

SharedPageDef& Handler::GetSharedPage() {
    return shared_page;
}

} // namespace SharedPage

// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include "core/hle/service/service.h"

namespace Core {
class System;
}

namespace Service::NDM {

class NDM_U final : public ServiceFramework<NDM_U> {
public:
    NDM_U();

private:
    void EnterExclusiveState(Kernel::HLERequestContext& ctx);
    void LeaveExclusiveState(Kernel::HLERequestContext& ctx);
    void QueryExclusiveMode(Kernel::HLERequestContext& ctx);
    void LockState(Kernel::HLERequestContext& ctx);
    void UnlockState(Kernel::HLERequestContext& ctx);
    void SuspendDaemons(Kernel::HLERequestContext& ctx);
    void ResumeDaemons(Kernel::HLERequestContext& ctx);
    void SuspendScheduler(Kernel::HLERequestContext& ctx);
    void ResumeScheduler(Kernel::HLERequestContext& ctx);
    void QueryStatus(Kernel::HLERequestContext& ctx);
    void GetDaemonDisableCount(Kernel::HLERequestContext& ctx);
    void GetSchedulerDisableCount(Kernel::HLERequestContext& ctx);
    void SetScanInterval(Kernel::HLERequestContext& ctx);
    void GetScanInterval(Kernel::HLERequestContext& ctx);
    void SetRetryInterval(Kernel::HLERequestContext& ctx);
    void GetRetryInterval(Kernel::HLERequestContext& ctx);
    void OverrideDefaultDaemons(Kernel::HLERequestContext& ctx);
    void ResetDefaultDaemons(Kernel::HLERequestContext& ctx);
    void GetDefaultDaemons(Kernel::HLERequestContext& ctx);
    void ClearHalfAwakeMacFilter(Kernel::HLERequestContext& ctx);

    enum class Daemon : u32 {
        Cec = 0,
        Boss = 1,
        Nim = 2,
        Friend = 3,
    };

    enum class DaemonMask : u32 {
        None = 0,
        Cec = (1 << static_cast<u32>(Daemon::Cec)),
        Boss = (1 << static_cast<u32>(Daemon::Boss)),
        Nim = (1 << static_cast<u32>(Daemon::Nim)),
        Friend = (1 << static_cast<u32>(Daemon::Friend)),
        Default = Cec | Friend,
        All = Cec | Boss | Nim | Friend,
    };

    enum class DaemonStatus : u32 {
        Busy = 0,
        Idle = 1,
        Suspending = 2,
        Suspended = 3,
    };

    enum class ExclusiveState : u32 {
        None = 0,
        Infrastructure = 1,
        LocalCommunications = 2,
        Streetpass = 3,
        StreetpassData = 4,
    };

    enum : u32 {
        DEFAULT_RETRY_INTERVAL = 10,
        DEFAULT_SCAN_INTERVAL = 30,
    };

    DaemonMask daemon_bit_mask = DaemonMask::Default;
    DaemonMask default_daemon_bit_mask = DaemonMask::Default;
    std::array<DaemonStatus, 4> daemon_status = {
        DaemonStatus::Idle,
        DaemonStatus::Idle,
        DaemonStatus::Idle,
        DaemonStatus::Idle,
    };
    ExclusiveState exclusive_state = ExclusiveState::None;
    u32 scan_interval = DEFAULT_SCAN_INTERVAL;
    u32 retry_interval = DEFAULT_RETRY_INTERVAL;
    bool daemon_lock_enabled = false;
};

void InstallInterfaces(Core::System& system);

} // namespace Service::NDM

// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>

#include "core/hle/service/service.h"

namespace Core {
class System;
}

namespace Service::MIC {

class MIC_U final : public ServiceFramework<MIC_U> {
public:
    explicit MIC_U(Core::System& system);
    ~MIC_U();

    void ReloadMic();

private:
    void MapSharedMem(Kernel::HLERequestContext& ctx);
    void UnmapSharedMem(Kernel::HLERequestContext& ctx);
    void StartSampling(Kernel::HLERequestContext& ctx);
    void AdjustSampling(Kernel::HLERequestContext& ctx);
    void StopSampling(Kernel::HLERequestContext& ctx);
    void IsSampling(Kernel::HLERequestContext& ctx);
    void GetBufferFullEvent(Kernel::HLERequestContext& ctx);
    void SetGain(Kernel::HLERequestContext& ctx);
    void GetGain(Kernel::HLERequestContext& ctx);
    void SetPower(Kernel::HLERequestContext& ctx);
    void GetPower(Kernel::HLERequestContext& ctx);
    void SetIirFilterMic(Kernel::HLERequestContext& ctx);
    void SetClamp(Kernel::HLERequestContext& ctx);
    void GetClamp(Kernel::HLERequestContext& ctx);
    void SetAllowShellClosed(Kernel::HLERequestContext& ctx);
    void SetClientVersion(Kernel::HLERequestContext& ctx);

    struct Impl;
    std::unique_ptr<Impl> impl;
};

void ReloadMic(Core::System& system);

void InstallInterfaces(Core::System& system);

} // namespace Service::MIC

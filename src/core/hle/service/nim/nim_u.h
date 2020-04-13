// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include "core/hle/service/service.h"

namespace Core {
class System;
}

namespace Service::NIM {

class NIM_U final : public ServiceFramework<NIM_U> {
public:
    explicit NIM_U(Core::System& system);
    ~NIM_U();

private:
    void CheckForSysUpdateEvent(Kernel::HLERequestContext& ctx);
    void CheckSysUpdateAvailable(Kernel::HLERequestContext& ctx);
    void Unknown(Kernel::HLERequestContext& ctx);

    std::shared_ptr<Kernel::Event> nim_system_update_event;
    std::shared_ptr<Kernel::Event> unknown_event;
};

} // namespace Service::NIM

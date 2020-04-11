// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/hle/service/service.h"

namespace Core {
class System;
}

namespace Service::LDR {

struct ClientSlot : public Kernel::SessionRequestHandler::SessionDataBase {
    VAddr loaded_crs = 0; ///< the virtual address of the static module
};

class RO final : public ServiceFramework<RO, ClientSlot> {
public:
    explicit RO(Core::System& system);

private:
    void Initialize(Kernel::HLERequestContext& ctx);
    void LoadCRR(Kernel::HLERequestContext& ctx);
    void UnloadCRR(Kernel::HLERequestContext& ctx);
    void LoadCRO(Kernel::HLERequestContext& ctx, bool link_on_load_bug_fix);
    template <bool link_on_load_bug_fix>
    void LoadCRO(Kernel::HLERequestContext& ctx) {
        LoadCRO(ctx, link_on_load_bug_fix);
    }
    void UnloadCRO(Kernel::HLERequestContext& self);
    void LinkCRO(Kernel::HLERequestContext& self);
    void UnlinkCRO(Kernel::HLERequestContext& self);
    void Shutdown(Kernel::HLERequestContext& self);

    Core::System& system;
};

void InstallInterfaces(Core::System& system);

} // namespace Service::LDR

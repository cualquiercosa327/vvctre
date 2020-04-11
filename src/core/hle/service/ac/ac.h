// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <memory>
#include "core/hle/service/service.h"

namespace Core {
class System;
}

namespace Kernel {
class Event;
}

namespace Service::AC {
class Module final {
public:
    class Interface : public ServiceFramework<Interface> {
    public:
        Interface(std::shared_ptr<Module> ac, const char* name, u32 max_session);

        void CreateDefaultConfig(Kernel::HLERequestContext& ctx);
        void ConnectAsync(Kernel::HLERequestContext& ctx);
        void GetConnectResult(Kernel::HLERequestContext& ctx);
        void CloseAsync(Kernel::HLERequestContext& ctx);
        void GetCloseResult(Kernel::HLERequestContext& ctx);
        void GetWifiStatus(Kernel::HLERequestContext& ctx);
        void GetInfraPriority(Kernel::HLERequestContext& ctx);
        void SetRequestEulaVersion(Kernel::HLERequestContext& ctx);
        void RegisterDisconnectEvent(Kernel::HLERequestContext& ctx);
        void IsConnected(Kernel::HLERequestContext& ctx);
        void SetClientVersion(Kernel::HLERequestContext& ctx);

    protected:
        std::shared_ptr<Module> ac;
    };

protected:
    struct ACConfig {
        std::array<u8, 0x200> data;
    };

    ACConfig default_config{};

    bool ac_connected = false;

    std::shared_ptr<Kernel::Event> close_event;
    std::shared_ptr<Kernel::Event> connect_event;
    std::shared_ptr<Kernel::Event> disconnect_event;
};

void InstallInterfaces(Core::System& system);

} // namespace Service::AC

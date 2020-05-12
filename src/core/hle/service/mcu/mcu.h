// Copyright 2020 vvctre project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include "core/hle/service/service.h"

namespace Core {
class System;
} // namespace Core

namespace Service::MCU {

class Module final {
public:
    class Interface : public ServiceFramework<Interface> {
    public:
        Interface(std::shared_ptr<Module> module, const char* name, u32 max_sessions);

    protected:
        std::shared_ptr<Module> module;
    };
};

void InstallInterfaces(Core::System& system);

} // namespace Service::MCU

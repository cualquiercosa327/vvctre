// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include "core/hle/service/service.h"

namespace Service::NEWS {

class NEWS_S final : public ServiceFramework<NEWS_S> {
public:
    NEWS_S();

private:
    void GetTotalNotifications(Kernel::HLERequestContext& ctx);
};

} // namespace Service::NEWS

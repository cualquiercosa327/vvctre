// Copyright 2020 vvctre project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <functional>
#include "core/frontend/applets/mii_selector.h"

namespace Frontend {

class SDL2_MiiSelector final : public MiiSelector {
public:
    SDL2_MiiSelector(std::function<void()> started_callback);
    void Setup(const MiiSelectorConfig& config) override;

private:
    std::function<void()> started_callback;
};

} // namespace Frontend

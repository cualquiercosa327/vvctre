// Copyright 2020 vvctre project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/frontend/applets/mii_selector.h"

namespace Frontend {

class SDL2_MiiSelector final : public MiiSelector {
public:
    void Setup(const MiiSelectorConfig& config) override;
};

} // namespace Frontend

// Copyright 2020 vvctre emulator project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <functional>
#include "core/frontend/applets/swkbd.h"

namespace Frontend {

class SDL2_SoftwareKeyboard final : public SoftwareKeyboard {
public:
    void Execute(const KeyboardConfig& config) override;
    void ShowError(const std::string& error) override;
};

} // namespace Frontend

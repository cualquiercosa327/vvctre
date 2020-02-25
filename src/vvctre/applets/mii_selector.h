// Copyright 2020 vvctre project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/frontend/applets/mii_selector.h"
#include "vvctre/emu_window/emu_window_sdl2.h"

namespace Frontend {

class SDL2_MiiSelector final : public MiiSelector {
public:
    SDL2_MiiSelector(EmuWindow_SDL2& emu_window);
    void Setup(const MiiSelectorConfig& config) override;

private:
    EmuWindow_SDL2& emu_window;
};

} // namespace Frontend

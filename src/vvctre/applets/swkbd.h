// Copyright 2020 vvctre emulator project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/frontend/applets/swkbd.h"
#include "vvctre/emu_window/emu_window_sdl2.h"

namespace Frontend {

class SDL2_SoftwareKeyboard final : public SoftwareKeyboard {
public:
    SDL2_SoftwareKeyboard(EmuWindow_SDL2& emu_window);
    void Execute(const KeyboardConfig& config) override;
    void ShowError(const std::string& error) override;

private:
    EmuWindow_SDL2& emu_window;
};

} // namespace Frontend

// Copyright 2020 vvctre emulator project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <portable-file-dialogs.h>
#include "video_core/renderer_opengl/renderer_opengl.h"
#include "vvctre/applets/swkbd.h"
#include "vvctre/emu_window/emu_window_sdl2.h"

namespace Frontend {

SDL2_SoftwareKeyboard::SDL2_SoftwareKeyboard(EmuWindow_SDL2& emu_window) : emu_window(emu_window) {}

void SDL2_SoftwareKeyboard::Execute(const KeyboardConfig& config) {
    SoftwareKeyboard::Execute(config);

    u8 code = 0;
    std::string text;

    emu_window.swkbd_config = &config;
    emu_window.swkbd_code = &code;
    emu_window.swkbd_text = &text;

    while (emu_window.IsOpen() && emu_window.swkbd_config != nullptr &&
           emu_window.swkbd_code != nullptr && emu_window.swkbd_text != nullptr) {
        VideoCore::g_renderer->SwapBuffers();
    }

    Finalize(text, code);
}

void SDL2_SoftwareKeyboard::ShowError(const std::string& error) {
    pfd::message("vvctre", error, pfd::choice::ok, pfd::icon::error);
}

} // namespace Frontend

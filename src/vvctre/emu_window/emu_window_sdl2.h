// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <map>
#include <memory>
#include <utility>
#include <imgui.h>
#include "core/frontend/applets/mii_selector.h"
#include "core/frontend/applets/swkbd.h"
#include "core/frontend/emu_window.h"
#include "core/hle/kernel/ipc_debugger/recorder.h"

struct SDL_Window;

#ifdef USE_DISCORD_PRESENCE
class DiscordRP;
#endif

namespace Core {
class System;
} // namespace Core

class EmuWindow_SDL2 : public Frontend::EmuWindow {
public:
    explicit EmuWindow_SDL2(Core::System& system, const bool fullscreen, const char* arg0);
    ~EmuWindow_SDL2();

    /// Swap buffers to display the next frame
    void SwapBuffers() override;

    /// Polls window events
    void PollEvents() override;

    /// Makes the graphics context current for the caller thread
    void MakeCurrent() override;

    /// Releases the GL context from the caller thread
    void DoneCurrent() override;

    /// Whether the window is still open, and a close request hasn't yet been sent
    bool IsOpen() const;

    void Close();

    /// Updates the disk shader cache loading progress bar
    /// If the progress bar is hidden, it will be shown
    /// If value == -1.0f, the progress bar will hide
    void DiskShaderCacheProgress(const float value);

    const Frontend::KeyboardConfig* swkbd_config = nullptr;
    u8* swkbd_code = nullptr;
    std::string* swkbd_text = nullptr;

    const Frontend::MiiSelectorConfig* mii_selector_config = nullptr;
    const std::vector<HLE::Applets::MiiData>* mii_selector_miis;
    u32* mii_selector_code = nullptr;
    HLE::Applets::MiiData* mii_selector_selected_mii = nullptr;

private:
    /// Called by PollEvents when a key is pressed or released.
    void OnKeyEvent(int key, u8 state);

    /// Called by PollEvents when the mouse moves.
    void OnMouseMotion(s32 x, s32 y);

    /// Called by PollEvents when a mouse button is pressed or released
    void OnMouseButton(u32 button, u8 state, s32 x, s32 y);

    /// Translates pixel position (0..1) to pixel positions
    std::pair<unsigned, unsigned> TouchToPixelPos(float touch_x, float touch_y) const;

    /// Called by PollEvents when a finger starts touching the touchscreen
    void OnFingerDown(float x, float y);

    /// Called by PollEvents when a finger moves while touching the touchscreen
    void OnFingerMotion(float x, float y);

    /// Called by PollEvents when a finger stops touching the touchscreen
    void OnFingerUp();

    /// Called by PollEvents when any event that may cause the window to be resized occurs
    void OnResize();

    /// Called when user passes the fullscreen parameter flag
    void Fullscreen();

    /// Is the window still open?
    bool is_open = true;

    /// Internal SDL2 render window
    SDL_Window* render_window = nullptr;

    using SDL_GLContext = void*;

    /// The OpenGL context associated with the window
    SDL_GLContext gl_context;

    std::string program_name;
    u64 program_id = 0;

    float disk_shader_cache_loading_progress = -1.0f;

#ifdef USE_DISCORD_PRESENCE
    std::unique_ptr<DiscordRP> discord_rp;
#endif

    Core::System& system;
    ImVec4 fps_color{0.0f, 1.0f, 0.0f, 1.0f}; // Green

    std::vector<std::string> messages;

    bool ipc_recorder_enabled = false;
    IPCDebugger::CallbackHandle ipc_recorder_callback;
    std::map<int, IPCDebugger::RequestRecord> ipc_records;

    bool show_cheats_window = false;

    const char* arg0 = nullptr;
};

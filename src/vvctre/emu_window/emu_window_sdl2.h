// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <utility>
#include "core/frontend/emu_window.h"
#include "video_core/rasterizer_interface.h"

struct SDL_Window;

#ifdef USE_DISCORD_PRESENCE
class DiscordRP;
#endif

class SharedContext_SDL2 : public Frontend::GraphicsContext {
public:
    using SDL_GLContext = void*;

    SharedContext_SDL2();

    ~SharedContext_SDL2() override;

    void MakeCurrent() override;

    void DoneCurrent() override;

private:
    SDL_GLContext context;
    SDL_Window* window;
};

class EmuWindow_SDL2 : public Frontend::EmuWindow {
public:
    explicit EmuWindow_SDL2(const bool headless, const bool fullscreen);
    ~EmuWindow_SDL2();

    void Present();

    /// Polls window events
    void PollEvents() override;

    /// Makes the graphics context current for the caller thread
    void MakeCurrent() override;

    /// Releases the GL context from the caller thread
    void DoneCurrent() override;

    /// Whether the window is still open, and a close request hasn't yet been sent
    bool IsOpen() const;

    void Close();

    /// Creates a new context that is shared with the current context
    std::unique_ptr<GraphicsContext> CreateSharedContext() const override;

    void DiskShaderCacheProgress(VideoCore::LoadCallbackStage stage, std::size_t value,
                                 std::size_t total);

    void SoftwareKeyboardStarted();
    void MiiPickerStarted();

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

    /// Fake hidden window for the core context
    SDL_Window* dummy_window;

    using SDL_GLContext = void*;

    /// The OpenGL context associated with the window
    SDL_GLContext window_context;

    /// The OpenGL context associated with the core
    std::unique_ptr<Frontend::GraphicsContext> core_context;

    /// Keeps track of how often to update the title bar during gameplay
    u32 last_time = 0;

    bool loading_disk_shader_cache = false;
    std::string program_name;
    u64 program_id = 0;

#ifdef USE_DISCORD_PRESENCE
    std::unique_ptr<DiscordRP> discord_rp;
#endif
};

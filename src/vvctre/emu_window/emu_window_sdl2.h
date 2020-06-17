// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <deque>
#include <map>
#include <memory>
#include <unordered_set>
#include <utility>
#include <imgui.h>
#include "core/frontend/applets/mii_selector.h"
#include "core/frontend/applets/swkbd.h"
#include "core/frontend/emu_window.h"
#include "core/hle/kernel/ipc_debugger/recorder.h"
#include "network/network.h"
#include "vvctre/common.h"

class PluginManager;
struct SDL_Window;

namespace Core {
class System;
} // namespace Core

class EmuWindow_SDL2 : public Frontend::EmuWindow {
public:
    explicit EmuWindow_SDL2(Core::System& system, PluginManager& plugin_manager,
                            SDL_Window* window);
    ~EmuWindow_SDL2();

    /// Swap buffers to display the next frame
    void SwapBuffers() override;

    /// Polls window events
    void PollEvents() override;

    /// Whether the window is still open, and a close request hasn't yet been sent
    bool IsOpen() const;

    void Close();

    const Frontend::KeyboardConfig* swkbd_config = nullptr;
    u8* swkbd_code = nullptr;
    std::string* swkbd_text = nullptr;

    const Frontend::MiiSelectorConfig* mii_selector_config = nullptr;
    const std::vector<HLE::Applets::MiiData>* mii_selector_miis;
    u32* mii_selector_code = nullptr;
    HLE::Applets::MiiData* mii_selector_selected_mii = nullptr;

    bool paused = false;

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
    void ToggleFullscreen();

    /// Called when Tools -> Copy Screenshot is clicked
    void CopyScreenshot();

    void ConnectToCitraRoom();

    /// Window
    bool is_open = true;
    SDL_Window* window = nullptr;

    Core::System& system;
    ImVec4 fps_color{0.0f, 1.0f, 0.0f, 1.0f}; // Green

    IPCDebugger::CallbackHandle ipc_recorder_callback;
    std::map<int, IPCDebugger::RequestRecord> ipc_records;

    std::string installed_query;
    std::vector<std::tuple<std::string, std::string>> installed;

    bool show_cheats_window = false;
    bool show_ipc_recorder_window = false;

    // Multiplayer
    bool show_connect_to_citra_room;
    CitraRoomList public_rooms;
    std::string public_rooms_query;
    std::string multiplayer_message;
    std::deque<std::string> multiplayer_messages;
    Network::RoomMember::CallbackHandle<Network::RoomMember::Error> multiplayer_on_error;
    Network::RoomMember::CallbackHandle<Network::ChatEntry> multiplayer_on_chat_message;
    Network::RoomMember::CallbackHandle<Network::StatusMessageEntry> multiplayer_on_status_message;
    std::unordered_set<std::string> multiplayer_blocked_nicknames;

    PluginManager& plugin_manager;
};

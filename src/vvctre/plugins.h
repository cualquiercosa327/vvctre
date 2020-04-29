// Copyright 2020 vvctre project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <unordered_map>

#ifdef _WIN32
#include <windows.h>
#endif

namespace PluginImportedFunctions {
using PluginLoaded = void (*)(void* core, void* plugin_manager); // required
using InitialSettingsOpening = void (*)();
using InitialSettingsOkPressed = void (*)();
using BeforeLoading = void (*)();
using EmulationStarting = void (*)();
using EmulatorClosing = void (*)();
using FatalError = void (*)();
using BeforeDrawingFPS = void (*)();
using AddMenu = void (*)();
using AfterSwapWindow = void (*)();
} // namespace PluginImportedFunctions

class PluginManager {
public:
    explicit PluginManager(void* core);
    ~PluginManager();

    // Calls the DLL functions
    void InitialSettingsOpening();
    void InitialSettingsOkPressed();
    void BeforeLoading();
    void EmulationStarting();
    void EmulatorClosing();
    void FatalError();
    void BeforeDrawingFPS();
    void AddMenus();
    void AfterSwapWindow();

    // DLLs can change this
    bool paused = false;

private:
#ifdef _WIN32
    struct Plugin {
        HMODULE handle;
        PluginImportedFunctions::BeforeDrawingFPS before_drawing_fps;
        PluginImportedFunctions::AddMenu add_menu;
        PluginImportedFunctions::AfterSwapWindow after_swap_window;
    };

    std::vector<Plugin> plugins;
#endif
};

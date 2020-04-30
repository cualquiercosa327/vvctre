// Copyright 2020 vvctre project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <vector>
#include "core/frontend/input.h"

#ifdef _WIN32
#include <windows.h>
#endif

namespace PluginImportedFunctions {
using PluginLoaded = void (*)(void* core, void* plugin_manager); // required
using InitialSettingsOpening = void (*)();                       // optional
using InitialSettingsOkPressed = void (*)();                     // optional
using BeforeLoading = void (*)();                                // optional
using EmulationStarting = void (*)();                            // optional
using EmulatorClosing = void (*)();                              // optional
using FatalError = void (*)();                                   // optional
using BeforeDrawingFPS = void (*)();                             // optional
using AddMenu = void (*)();                                      // optional
using AfterSwapWindow = void (*)();                              // optional
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
    void* NewButtonDevice(const char* params);
    void DeleteButtonDevice(void* device);

    // DLLs can change this
    bool paused = false;

private:
#ifdef _WIN32
    struct Plugin {
        HMODULE handle;
        PluginImportedFunctions::BeforeDrawingFPS before_drawing_fps = nullptr;
        PluginImportedFunctions::AddMenu add_menu = nullptr;
        PluginImportedFunctions::AfterSwapWindow after_swap_window = nullptr;
    };

    std::vector<Plugin> plugins;
    std::vector<std::unique_ptr<Input::ButtonDevice>> buttons;
#endif
};

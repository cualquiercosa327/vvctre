// Copyright 2020 vvctre project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <vector>
#include "core/frontend/input.h"
#include "vvctre/function_logger.h"

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
using Log = Log::FunctionLogger::Function;
} // namespace PluginImportedFunctions

class PluginManager {
public:
    explicit PluginManager(void* core);
    ~PluginManager();

    // Calls the plugin exported functions
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

    // Plugins can change this
    bool paused = false;

private:
    struct Plugin {
#ifdef _WIN32
        HMODULE handle;
#else
        void* handle;
#endif
        PluginImportedFunctions::BeforeDrawingFPS before_drawing_fps = nullptr;
        PluginImportedFunctions::AddMenu add_menu = nullptr;
        PluginImportedFunctions::AfterSwapWindow after_swap_window = nullptr;
    };

    std::vector<Plugin> plugins;
    std::vector<std::unique_ptr<Input::ButtonDevice>> buttons;
};

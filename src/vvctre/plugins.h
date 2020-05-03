// Copyright 2020 vvctre project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <unordered_map>
#include <vector>
#include "common/common_types.h"
#include "core/frontend/input.h"
#include "vvctre/function_logger.h"

#ifdef _WIN32
#include <windows.h>
#endif

namespace PluginImportedFunctions {
using GetRequiredFunctionCount = int (*)();                                         // required
using GetRequiredFunctionNames = const char** (*)();                                // required
using PluginLoaded = void (*)(void* core, void* plugin_manager, void* functions[]); // required
using InitialSettingsOpening = void (*)();                                          // optional
using InitialSettingsOkPressed = void (*)();                                        // optional
using BeforeLoading = void (*)();                                                   // optional
using EmulationStarting = void (*)();                                               // optional
using EmulatorClosing = void (*)();                                                 // optional
using FatalError = void (*)();                                                      // optional
using BeforeDrawingFPS = void (*)();                                                // optional
using AddMenu = void (*)();                                                         // optional
using AddTab = void (*)();                                                          // optional
using AfterSwapWindow = void (*)();                                                 // optional
using ScreenshotCallback = void (*)(void* data);
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
    void AddTabs();
    void AfterSwapWindow();
    void* NewButtonDevice(const char* params);
    void DeleteButtonDevice(void* device);
    void CallScreenshotCallbacks(void* data);

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
        PluginImportedFunctions::AddTab add_tab = nullptr;
        PluginImportedFunctions::AfterSwapWindow after_swap_window = nullptr;
        PluginImportedFunctions::ScreenshotCallback screenshot_callback = nullptr;
    };

    std::vector<Plugin> plugins;
    std::vector<std::unique_ptr<Input::ButtonDevice>> buttons;

    static std::unordered_map<std::string, void*> function_map;
};

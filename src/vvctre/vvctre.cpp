// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <iostream>
#include <memory>
#include <string>
#include <thread>

#ifdef _WIN32
// windows.h needs to be included before shellapi.h
#include <windows.h>

#include <shellapi.h>
#else
#include <sys/stat.h>
#endif

#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <portable-file-dialogs.h>
#include "common/detached_tasks.h"
#include "common/file_util.h"
#include "common/logging/backend.h"
#include "common/logging/filter.h"
#include "common/logging/log.h"
#include "common/param_package.h"
#include "common/scope_exit.h"
#include "common/string_util.h"
#include "core/core.h"
#include "core/hle/service/am/am.h"
#include "core/hle/service/cfg/cfg.h"
#include "core/loader/loader.h"
#include "core/movie.h"
#include "core/settings.h"
#include "input_common/main.h"
#include "video_core/renderer_base.h"
#include "video_core/video_core.h"
#include "vvctre/applets/mii_selector.h"
#include "vvctre/applets/swkbd.h"
#include "vvctre/camera/image.h"
#include "vvctre/common.h"
#include "vvctre/emu_window/emu_window_sdl2.h"
#include "vvctre/initial_settings.h"
#include "vvctre/plugins.h"

#ifndef _MSC_VER
#include <unistd.h>
#endif

#ifdef _WIN32
extern "C" {
// Tells Nvidia drivers to use the dedicated GPU by default on laptops with switchable graphics
__declspec(dllexport) unsigned long NvOptimusEnablement = 0x00000001;
}
#endif

static void InitializeLogging() {
    Log::Filter log_filter(Log::Level::Debug);
    log_filter.ParseFilterString(Settings::values.log_filter);
    Log::SetGlobalFilter(log_filter);

    Log::AddBackend(std::make_unique<Log::ColorConsoleBackend>());
}

int main(int, char**) {
    // Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK) < 0) {
        std::cerr << "Failed to initialize SDL2! Exiting..." << std::endl;
        std::exit(1);
    }

    InputCommon::Init();

    SDL_SetMainReady();
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 0);

    Core::System& system = Core::System::GetInstance();
    PluginManager plugin_manager(static_cast<void*>(&system));

    plugin_manager.InitialSettingsOpening();
    InitialSettings(plugin_manager).Run();
    plugin_manager.InitialSettingsOkPressed();

    InitializeLogging();

    Common::DetachedTasks detached_tasks;

    if (!Settings::values.record_movie.empty()) {
        Core::Movie::GetInstance().PrepareForRecording();
    }

    if (!Settings::values.play_movie.empty()) {
        Core::Movie::GetInstance().PrepareForPlayback(Settings::values.play_movie);
    }

    std::unique_ptr<EmuWindow_SDL2> emu_window =
        std::make_unique<EmuWindow_SDL2>(system, plugin_manager);

    // Register frontend applets
    system.RegisterSoftwareKeyboard(std::make_shared<Frontend::SDL2_SoftwareKeyboard>(*emu_window));
    system.RegisterMiiSelector(std::make_shared<Frontend::SDL2_MiiSelector>(*emu_window));

    // Register camera implementations
    Camera::RegisterFactory("image", std::make_unique<Camera::ImageCameraFactory>());

    plugin_manager.BeforeLoading();

    const Core::System::ResultStatus load_result =
        system.Load(*emu_window, Settings::values.file_path);

    plugin_manager.EmulationStarting();

    switch (load_result) {
    case Core::System::ResultStatus::ErrorNotInitialized:
        pfd::message("vvctre", "Not initialized", pfd::choice::ok, pfd::icon::error);
        return -1;
    case Core::System::ResultStatus::ErrorSystemMode:
        pfd::message("vvctre", "Failed to determine system mode", pfd::choice::ok,
                     pfd::icon::error);
        return -1;
    case Core::System::ResultStatus::ErrorLoader_ErrorEncrypted:
        pfd::message("vvctre", "Encrypted file", pfd::choice::ok, pfd::icon::error);
        return -1;
    case Core::System::ResultStatus::ErrorLoader_ErrorUnsupportedFormat:
        pfd::message("vvctre", "Unsupported file format", pfd::choice::ok, pfd::icon::error);
        return -1;
    default:
        break;
    }

    if (!Settings::values.play_movie.empty()) {
        Core::Movie::GetInstance().StartPlayback(Settings::values.play_movie, [&] {
            pfd::message("vvctre", "Playback finished", pfd::choice::ok);
        });
    }

    if (!Settings::values.record_movie.empty()) {
        Core::Movie::GetInstance().StartRecording(Settings::values.record_movie);
    }

    while (emu_window->IsOpen()) {
        if (emu_window->paused) {
            while (emu_window->IsOpen() && emu_window->paused) {
                VideoCore::g_renderer->SwapBuffers();
                SDL_GL_SetSwapInterval(1);
            }
            SDL_GL_SetSwapInterval(Settings::values.enable_vsync ? 1 : 0);
        }

        switch (system.RunLoop()) {
        case Core::System::ResultStatus::Success: {
            break;
        }
        case Core::System::ResultStatus::FatalError: {
            pfd::message("vvctre", "Fatal error.\nCheck the console window for more details.",
                         pfd::choice::ok, pfd::icon::error);
            plugin_manager.FatalError();
            system.SetStatus(Core::System::ResultStatus::Success);
            break;
        }
        case Core::System::ResultStatus::ShutdownRequested: {
            emu_window->Close();
            break;
        }
        default: {
            break;
        }
        }
    }

    Core::Movie::GetInstance().Shutdown();
    system.Shutdown();
    detached_tasks.WaitForAllTasks();
    plugin_manager.EmulatorClosing();

    return 0;
}

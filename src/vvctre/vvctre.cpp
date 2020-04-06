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
#include <indicators/progress_bar.hpp>
#include <portable-file-dialogs.h>
#include "common/common_paths.h"
#include "common/detached_tasks.h"
#include "common/file_util.h"
#include "common/logging/backend.h"
#include "common/logging/filter.h"
#include "common/logging/log.h"
#include "common/param_package.h"
#include "common/scope_exit.h"
#include "common/string_util.h"
#include "common/version.h"
#include "core/core.h"
#include "core/hle/service/am/am.h"
#include "core/hle/service/cfg/cfg.h"
#include "core/loader/loader.h"
#include "core/movie.h"
#include "core/rpc/server.h"
#include "core/settings.h"
#include "input_common/main.h"
#include "video_core/renderer_base.h"
#include "video_core/video_core.h"
#include "vvctre/applets/mii_selector.h"
#include "vvctre/applets/swkbd.h"
#include "vvctre/camera/image.h"
#include "vvctre/configuration.h"
#include "vvctre/emu_window/emu_window_sdl2.h"

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

#ifdef _WIN32
    Log::AddBackend(std::make_unique<Log::DebuggerBackend>());
#endif
}

int main(int, char**) {
    InputCommon::Init();

    // Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK) < 0) {
        std::cerr << "Failed to initialize SDL2! Exiting..." << std::endl;
        std::exit(1);
    }

    SDL_SetMainReady();
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 0);

    Configuration().Run();

    InitializeLogging();

    Common::DetachedTasks detached_tasks;

    if (!Settings::values.record_movie.empty()) {
        Core::Movie::GetInstance().PrepareForRecording();
    }

    if (!Settings::values.play_movie.empty()) {
        Core::Movie::GetInstance().PrepareForPlayback(Settings::values.play_movie);
    }

    Core::System& system = Core::System::GetInstance();

    std::unique_ptr<EmuWindow_SDL2> emu_window = std::make_unique<EmuWindow_SDL2>(system);

    // Register frontend applets
    system.RegisterSoftwareKeyboard(std::make_shared<Frontend::SDL2_SoftwareKeyboard>(*emu_window));
    system.RegisterMiiSelector(std::make_shared<Frontend::SDL2_MiiSelector>(*emu_window));

    // Register camera implementations
    Camera::RegisterFactory("image", std::make_unique<Camera::ImageCameraFactory>());

    const Core::System::ResultStatus load_result =
        system.Load(*emu_window, Settings::values.file_path);

    switch (load_result) {
    case Core::System::ResultStatus::ErrorNotInitialized:
        LOG_CRITICAL(Frontend, "CPU not initialized");
        return -1;
    case Core::System::ResultStatus::ErrorGetLoader:
        LOG_CRITICAL(Frontend, "Failed to obtain loader for {}!", Settings::values.file_path);
        return -1;
    case Core::System::ResultStatus::ErrorSystemMode:
        LOG_CRITICAL(Frontend, "Failed to determine system mode!");
        return -1;
    case Core::System::ResultStatus::ErrorLoader:
        LOG_CRITICAL(Frontend, "Failed to load ROM!");
        return -1;
    case Core::System::ResultStatus::ErrorLoader_ErrorEncrypted:
        LOG_CRITICAL(Frontend, "The game that you are trying to load must be decrypted before "
                               "being used with vvctre. \n\n For more information on dumping and "
                               "decrypting games, please refer to: "
                               "https://citra-emu.org/wiki/dumping-game-cartridges/");
        return -1;
    case Core::System::ResultStatus::ErrorLoader_ErrorInvalidFormat:
        LOG_CRITICAL(Frontend, "The ROM format is not supported.");
        return -1;
    case Core::System::ResultStatus::ErrorVideoCore:
        LOG_CRITICAL(Frontend, "VideoCore error. Ensure that you have the latest graphics "
                               "drivers for your GPU.");
        return -1;
    case Core::System::ResultStatus::ErrorVideoCore_ErrorGenericDrivers:
        LOG_CRITICAL(Frontend,
                     "You are running default Windows drivers "
                     "for your GPU. You need to install the "
                     "proper drivers for your graphics card from the manufacturer's website.");
        return -1;
    case Core::System::ResultStatus::ErrorVideoCore_ErrorBelowGL33:
        LOG_CRITICAL(Frontend, "Your GPU may not support OpenGL 3.3, or you do not "
                               "have the latest graphics driver.");
        return -1;
    default:
        break;
    }

    RPC::Server rpc_server(system, Settings::values.rpc_server_port);

    if (!Settings::values.play_movie.empty()) {
        Core::Movie::GetInstance().StartPlayback(Settings::values.play_movie);
    }

    if (!Settings::values.record_movie.empty()) {
        Core::Movie::GetInstance().StartRecording(Settings::values.record_movie);
    }

    while (emu_window->IsOpen()) {
        if (system.frontend_paused || system.rpc_paused) {
            while (emu_window->IsOpen() && (system.frontend_paused || system.rpc_paused)) {
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
            pfd::message("vvctre", "Fatal error.\nCheck the log for more details.", pfd::choice::ok,
                         pfd::icon::error);
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

    return 0;
}

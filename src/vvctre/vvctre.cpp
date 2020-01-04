// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <iostream>
#include <memory>
#include <regex>
#include <string>
#include <thread>

#ifdef _WIN32
// windows.h needs to be included before shellapi.h
#include <windows.h>

#include <shellapi.h>
#endif

#include <clipp.h>
#include "common/common_paths.h"
#include "common/detached_tasks.h"
#include "common/file_util.h"
#include "common/logging/backend.h"
#include "common/logging/filter.h"
#include "common/logging/log.h"
#include "common/scope_exit.h"
#include "common/string_util.h"
#include "common/version.h"
#include "core/core.h"
#include "core/dumping/backend.h"
#include "core/file_sys/cia_container.h"
#include "core/frontend/applets/default_applets.h"
#include "core/frontend/framebuffer_layout.h"
#include "core/frontend/scope_acquire_context.h"
#include "core/gdbstub/gdbstub.h"
#include "core/hle/service/am/am.h"
#include "core/hle/service/cfg/cfg.h"
#include "core/loader/loader.h"
#include "core/movie.h"
#include "core/settings.h"
#include "video_core/renderer_base.h"
#include "video_core/video_core.h"
#include "vvctre/config.h"
#include "vvctre/emu_window/emu_window_sdl2.h"
#include "vvctre/lodepng_image_interface.h"

#ifndef _MSC_VER
#include <unistd.h>
#endif

#ifdef _WIN32
extern "C" {
// tells Nvidia drivers to use the dedicated GPU by default on laptops with switchable graphics
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

bool EndsWithIgnoreCase(const std::string& str, const std::string& suffix) {
    return std::regex_search(str,
                             std::regex(std::string(suffix) + "$", std::regex_constants::icase));
}

/// Application entry point
int main(int argc, char** argv) {
    Common::DetachedTasks detached_tasks;
    Config config;

    std::string path;
    std::string movie_record;
    std::string movie_play;
    std::string dump_video;
    bool fullscreen = false;

    enum class Command {
        Boot,
        Version,
    } command;

    auto cli =
        ((clipp::value("path").set(command, Command::Boot).set(path).doc("executable or CIA path"),
          clipp::option("-g", "--gdbstub")
                  .set(Settings::values.use_gdbstub, true)
                  .doc("enable the GDB stub") &
              clipp::value("port").set(Settings::values.gdbstub_port),
          clipp::option("-r", "--movie-record").doc("record inputs to a file") &
              clipp::value("path").set(movie_record),
          clipp::option("-p", "--movie-play")
                  .set(Settings::values.use_gdbstub, true)
                  .doc("play inputs from a file") &
              clipp::value("path").set(movie_play),
          clipp::option("-d", "--dump-video").doc("dump audio and video to a file") &
              clipp::value("path").set(dump_video),
          clipp::option("-f", "--fullscreen").set(fullscreen).doc("start in fullscreen mode")) |
         clipp::command("version").set(command, Command::Version).doc("prints vvctre's version"));

    if (!clipp::parse(argc, argv, cli)) {
        std::cout << clipp::make_man_page(cli, argv[0]);
        return -1;
    }

    switch (command) {
    case Command::Boot: {
        InitializeLogging();

        if (EndsWithIgnoreCase(path, ".cia")) {
            const auto cia_progress = [](std::size_t written, std::size_t total) {
                LOG_INFO(Frontend, "{:02d}%", (written * 100 / total));
            };

            if (Service::AM::InstallCIA(path, cia_progress) ==
                Service::AM::InstallStatus::Success) {
                std::cout << "CIA installed successfully" << std::endl;
            } else {
                std::cout << "CIA installation failed" << std::endl;
                return -1;
            }
        } else {
            if (!movie_record.empty() && !movie_play.empty()) {
                LOG_CRITICAL(Frontend, "Cannot both play and record a movie");
                return -1;
            }

            if (!movie_record.empty()) {
                Core::Movie::GetInstance().PrepareForRecording();
            }

            if (!movie_play.empty()) {
                Core::Movie::GetInstance().PrepareForPlayback(movie_play);
            }

            // Apply the settings
            Settings::Apply();

            // Register frontend applets
            Frontend::RegisterDefaultApplets();

            // Register image interface
            Core::System::GetInstance().RegisterImageInterface(
                std::make_shared<LodePNGImageInterface>());

            std::unique_ptr<EmuWindow_SDL2> emu_window =
                std::make_unique<EmuWindow_SDL2>(fullscreen);
            Frontend::ScopeAcquireContext scope(*emu_window);
            Core::System& system = Core::System::GetInstance();

            const Core::System::ResultStatus load_result = system.Load(*emu_window, path);

            switch (load_result) {
            case Core::System::ResultStatus::ErrorGetLoader:
                LOG_CRITICAL(Frontend, "Failed to obtain loader for {}!", path);
                return -1;
            case Core::System::ResultStatus::ErrorLoader:
                LOG_CRITICAL(Frontend, "Failed to load ROM!");
                return -1;
            case Core::System::ResultStatus::ErrorLoader_ErrorEncrypted:
                LOG_CRITICAL(Frontend,
                             "The game that you are trying to load must be decrypted before "
                             "being used with vvctre. \n\n For more information on dumping and "
                             "decrypting games, please refer to: "
                             "https://citra-emu.org/wiki/dumping-game-cartridges/");
                return -1;
            case Core::System::ResultStatus::ErrorLoader_ErrorInvalidFormat:
                LOG_CRITICAL(Frontend, "Error while loading ROM: The ROM format is not supported.");
                return -1;
            case Core::System::ResultStatus::ErrorNotInitialized:
                LOG_CRITICAL(Frontend, "CPU not initialized");
                return -1;
            case Core::System::ResultStatus::ErrorSystemMode:
                LOG_CRITICAL(Frontend, "Failed to determine system mode!");
                return -1;
            case Core::System::ResultStatus::ErrorVideoCore:
                LOG_CRITICAL(Frontend, "VideoCore not initialized");
                return -1;
            case Core::System::ResultStatus::Success:
                break; // Expected case
            }

            if (!movie_play.empty()) {
                Core::Movie::GetInstance().StartPlayback(movie_play);
            }

            if (!movie_record.empty()) {
                Core::Movie::GetInstance().StartRecording(movie_record);
            }

            if (!dump_video.empty()) {
                const Layout::FramebufferLayout layout =
                    Layout::FrameLayoutFromResolutionScale(VideoCore::GetResolutionScaleFactor());
                system.VideoDumper().StartDumping(dump_video, "webm", layout);
            }

            std::thread render_thread([&emu_window] { emu_window->Present(); });

            std::atomic_bool stop_run;
            Core::System::GetInstance().Renderer().Rasterizer()->LoadDiskResources(
                stop_run,
                [](VideoCore::LoadCallbackStage stage, std::size_t value, std::size_t total) {
                    LOG_DEBUG(Frontend, "Loading stage {} progress {} {}", static_cast<u32>(stage),
                              value, total);
                });

            while (emu_window->IsOpen()) {
                system.RunLoop();
            }

            render_thread.join();

            Core::Movie::GetInstance().Shutdown();

            if (system.VideoDumper().IsDumping()) {
                system.VideoDumper().StopDumping();
            }

            system.Shutdown();
        }

        break;
    }
    case Command::Version: {
        std::cout << version::vvctre.to_string() << std::endl;
        break;
    }
    }

    detached_tasks.WaitForAllTasks();
    return 0;
}

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
#include "input_common/main.h"
#include "video_core/renderer_base.h"
#include "video_core/video_core.h"
#include "vvctre/applets/keyboard.h"
#include "vvctre/config.h"
#include "vvctre/emu_window/emu_window_sdl2.h"

#ifdef USE_DISCORD_PRESENCE
#include "vvctre/discord_rp.h"
#endif

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
    bool regenerate_console_id = false;

    enum class Command {
        BootOrInstall,
        Controls,
        Version,
        Usage,
    } command = Command::BootOrInstall;

    auto cli =
        ((clipp::opt_value("path").set(path).doc("executable or CIA path"),
          clipp::option("--gdbstub").doc("enable the GDB stub") &
              clipp::value("port")
                  .set(Settings::values.use_gdbstub, true)
                  .set(Settings::values.gdbstub_port),
          clipp::option("--movie-record").doc("record inputs to a file") &
              clipp::value("path").set(movie_record),
          clipp::option("--movie-play").doc("play inputs from a file") &
              clipp::value("path").set(movie_play),
          clipp::option("--dump-video").doc("dump audio and video to a file") &
              clipp::value("path").set(dump_video),
          clipp::option("--speed-limit").doc("set the speed limit") &
              clipp::value("limit")
                  .set(Settings::values.use_frame_limit, true)
                  .set(Settings::values.frame_limit),
          clipp::option("--screen-refresh-rate").doc("set a custom 3DS screen refresh rate") &
              clipp::value("rate")
                  .set(Settings::values.use_custom_screen_refresh_rate, true)
                  .set(Settings::values.custom_screen_refresh_rate),
          clipp::option("--custom-cpu-ticks").doc("set custom CPU ticks") &
              clipp::value("ticks")
                  .set(Settings::values.use_custom_cpu_ticks, true)
                  .set(Settings::values.custom_cpu_ticks),
          clipp::option("--cpu-clock-percentage").doc("set CPU clock percentage") &
              clipp::value("percentage").set(Settings::values.cpu_clock_percentage),
          clipp::option("--multiplayer-server-url").doc("set the multiplayer server URL") &
              clipp::value("url").set(Settings::values.multiplayer_url),
          clipp::option("--log-filter").doc("set the log filter") &
              clipp::value("filter").set(Settings::values.log_filter),
          clipp::option("--minimum-vertices-per-thread")
                  .doc("set minimum vertices per thread (only used for software shader)") &
              clipp::value("value").set(Settings::values.min_vertices_per_thread),
          clipp::option("--resolution").doc("set resolution") &
              clipp::value("value").set(Settings::values.resolution_factor),
          clipp::option("--audio-speed")
                  .doc("set audio speed for DSP HLE to a float, must be greater than zero and "
                       "requires audio stretching to be enabled to work properly") &
              clipp::value("value").set(Settings::values.audio_speed),
          clipp::option("--background-color-red")
                  .doc("set background color red component to a float in range 0.0-1.0") &
              clipp::value("value").set(Settings::values.bg_red),
          clipp::option("--background-color-green")
                  .doc("set background color green component to a float in range 0.0-1.0") &
              clipp::value("value").set(Settings::values.bg_green),
          clipp::option("--background-color-blue")
                  .doc("set background color blue component to a float in range 0.0-1.0") &
              clipp::value("value").set(Settings::values.bg_blue),
          clipp::option("--cpu-jit")
              .doc("force use CPU JIT")
              .set(Settings::values.use_cpu_jit, true),
          clipp::option("--cpu-interpreter")
              .doc("force use CPU interpreter")
              .set(Settings::values.use_cpu_jit, false),
          clipp::option("--dsp-hle")
              .doc("force use DSP HLE")
              .set(Settings::values.enable_dsp_lle, false),
          clipp::option("--dsp-lle")
              .doc("force use DSP LLE single-threaded")
              .set(Settings::values.enable_dsp_lle, true)
              .set(Settings::values.enable_dsp_lle_multithread, false),
          clipp::option("--dsp-lle-multi-threaded")
              .doc("force use DSP LLE multi-threaded")
              .set(Settings::values.enable_dsp_lle, true)
              .set(Settings::values.enable_dsp_lle_multithread, true),
          clipp::option("--hardware-renderer")
              .doc("force use hardware renderer")
              .set(Settings::values.use_hw_renderer, true),
          clipp::option("--software-renderer")
              .doc("force use software renderer")
              .set(Settings::values.use_hw_renderer, false),
          clipp::option("--hardware-shader")
              .doc("force use hardware shader")
              .set(Settings::values.use_hw_shader, true),
          clipp::option("--software-shader")
              .doc("force use software shader")
              .set(Settings::values.use_hw_shader, false),
          clipp::option("--accurate-multiplication")
              .doc("force use accurate multiplication if using hardware shader")
              .set(Settings::values.shaders_accurate_mul, true),
          clipp::option("--inaccurate-multiplication")
              .doc("force use inaccurate multiplication if using hardware shader")
              .set(Settings::values.shaders_accurate_mul, false),
          clipp::option("--enable-disk-shader-caching")
              .doc("force enable disk shader caching")
              .set(Settings::values.use_disk_shader_cache, true),
          clipp::option("--disable-disk-shader-caching")
              .doc("force disable disk shader caching")
              .set(Settings::values.use_disk_shader_cache, false),
          clipp::option("--enable-ignore-format-reinterpretation")
              .doc("force enable ignore format reinterpretation")
              .set(Settings::values.ignore_format_reinterpretation, true),
          clipp::option("--disable-ignore-format-reinterpretation")
              .doc("force disable ignore format reinterpretation")
              .set(Settings::values.ignore_format_reinterpretation, false),
          clipp::option("--dump-textures")
              .doc("force enable texture dumping")
              .set(Settings::values.dump_textures, true),
          clipp::option("--no-dump-textures")
              .doc("force disable texture dumping")
              .set(Settings::values.dump_textures, false),
          clipp::option("--custom-textures")
              .doc("force enable custom textures")
              .set(Settings::values.custom_textures, true),
          clipp::option("--no-custom-textures")
              .doc("force disable custom textures")
              .set(Settings::values.custom_textures, false),
          clipp::option("--preload-custom-textures")
              .doc("force enable custom texture preloading")
              .set(Settings::values.preload_textures, true),
          clipp::option("--no-preload-custom-textures")
              .doc("force disable custom texture preloading")
              .set(Settings::values.preload_textures, false),
          clipp::option("--custom-layout")
              .doc("force use custom layout")
              .set(Settings::values.custom_layout, true),
          clipp::option("--no-custom-layout")
              .doc("force disable custom layout")
              .set(Settings::values.custom_layout, false),
          clipp::option("--default-layout")
              .doc("force use default layout")
              .set(Settings::values.custom_layout, false)
              .set(Settings::values.layout_option, Settings::LayoutOption::Default),
          clipp::option("--single-screen-layout")
              .doc("force use single screen layout")
              .set(Settings::values.custom_layout, false)
              .set(Settings::values.layout_option, Settings::LayoutOption::SingleScreen),
          clipp::option("--large-screen-layout")
              .doc("force use Large Screen Small Screen layout")
              .set(Settings::values.custom_layout, false)
              .set(Settings::values.layout_option, Settings::LayoutOption::LargeScreen),
          clipp::option("--side-by-side-layout")
              .doc("force use side by side layout")
              .set(Settings::values.custom_layout, false)
              .set(Settings::values.layout_option, Settings::LayoutOption::SideScreen),
          clipp::option("--medium-screen-layout")
              .doc("force use Large Screen Medium Screen layout")
              .set(Settings::values.custom_layout, false)
              .set(Settings::values.layout_option, Settings::LayoutOption::MediumScreen),
          clipp::option("--swap-screens")
              .doc("force swap screens")
              .set(Settings::values.swap_screen, true),
          clipp::option("--no-swap-screens")
              .doc("force disable swap screens")
              .set(Settings::values.swap_screen, false),
          clipp::option("--upright-orientation")
              .doc("force upright orientation, for book style games")
              .set(Settings::values.upright_screen, true),
          clipp::option("--no-upright-orientation")
              .doc("force disable upright orientation")
              .set(Settings::values.upright_screen, false),
          clipp::option("--fullscreen").set(fullscreen).doc("start in fullscreen mode"),
          clipp::option("--regenerate-console-id")
              .set(regenerate_console_id)
              .doc("regenerate the console ID before booting"),
          clipp::option("--unlimited")
              .set(Settings::values.use_frame_limit, false)
              .doc("disable the speed limiter")) |
         clipp::command("controls").set(command, Command::Controls).doc("configure a controller") |
         clipp::command("version").set(command, Command::Version).doc("prints vvctre's version") |
         clipp::command("usage").set(command, Command::Usage).doc("prints this"));

    if (!clipp::parse(argc, argv, cli) && argc > 1) {
        std::cout << clipp::make_man_page(cli, argv[0]);
        return -1;
    }

    switch (command) {
    case Command::BootOrInstall: {
        InitializeLogging();

        if (path.empty()) {
            const std::vector<std::string> result =
                pfd::open_file(
                    "Open File", ".",
                    {"3DS Executables", "*.cci *.3ds *.cxi *.3dsx *.app *.elf *.axf *.cia"})
                    .result();

            if (result.empty()) {
                return -1;
            } else {
                path = result[0];
            }
        }

        if (EndsWithIgnoreCase(path, ".cia")) {
            const auto cia_progress = [](std::size_t written, std::size_t total) {
                LOG_INFO(Frontend, "{:d}%", (written * 100 / total));
            };

            return Service::AM::InstallCIA(path, cia_progress) ==
                           Service::AM::InstallStatus::Success
                       ? 0
                       : -1;
        } else {
            ASSERT_MSG(Settings::values.audio_speed > 0.0f,
                       "audio speed must be greater than zero");

            if (!movie_record.empty() && !movie_play.empty()) {
                LOG_CRITICAL(Frontend, "Cannot both play and record a movie");
                return -1;
            }

            if (regenerate_console_id) {
                u32 random_number;
                u64 console_id;
                std::shared_ptr<Service::CFG::Module> cfg =
                    std::make_shared<Service::CFG::Module>();
                cfg->GenerateConsoleUniqueId(random_number, console_id);
                cfg->SetConsoleUniqueId(random_number, console_id);
                cfg->UpdateConfigNANDSavegame();
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
            Core::System& system = Core::System::GetInstance();
            Frontend::RegisterDefaultApplets();
            system.RegisterSoftwareKeyboard(std::make_shared<Frontend::SDL2_SoftwareKeyboard>());

            std::unique_ptr<EmuWindow_SDL2> emu_window =
                std::make_unique<EmuWindow_SDL2>(fullscreen);
            Frontend::ScopeAcquireContext scope(*emu_window);

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

            std::string game;
            system.GetAppLoader().ReadTitle(game);
            emu_window->SetGameName(game);

#ifdef USE_DISCORD_PRESENCE
            [[maybe_unused]] DiscordRP discord_rp(game);
#endif

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
    case Command::Controls: {
        InputCommon::Init();
        const auto GetInput = [](InputCommon::Polling::DeviceType type) {
            auto pollers = InputCommon::Polling::GetPollers(type);
            for (auto& poller : pollers) {
                poller->Start();
            }
            for (;;) {
                for (auto& poller : pollers) {
                    const Common::ParamPackage params = poller->GetNextInput();
                    if (params.Has("engine")) {
                        for (auto& poller : pollers) {
                            poller->Stop();
                        }
                        return params;
                    }
                }

                using namespace std::chrono_literals;
                std::this_thread::sleep_for(250ms);
            }
        };

        std::vector<std::string> lines;

        for (const auto& mapping : Settings::NativeButton::mapping) {
            fmt::print("Current button: {}. After pressing the enter key, press the button\n",
                       Common::ToUpper(mapping));
            std::cin.get();

            const Common::ParamPackage params = GetInput(InputCommon::Polling::DeviceType::Button);
            lines.push_back(fmt::format("{}={}", mapping, params.Serialize()));
        }

        for (const auto& mapping : Settings::NativeAnalog::mapping) {
            fmt::print("Current joystick: {}. After pressing the enter key, first move your "
                       "joystick to the right, and then to the bottom\n",
                       Common::ToUpper(mapping));
            std::cin.get();

            const Common::ParamPackage params = GetInput(InputCommon::Polling::DeviceType::Analog);
            lines.push_back(fmt::format("{}={}", mapping, params.Serialize()));
        }

        fmt::print("Change the [Controls] section in the ini file to this:\n\n[Controls]\n");

        for (const std::string& line : lines) {
            fmt::print("{}\n", line);
        }

        break;
    }
    case Command::Version: {
        fmt::print("{}\n", version::vvctre.to_string());
        break;
    }
    case Command::Usage: {
        std::cout << clipp::make_man_page(cli, argv[0]);
        break;
    }
    }

    detached_tasks.WaitForAllTasks();
    return 0;
}

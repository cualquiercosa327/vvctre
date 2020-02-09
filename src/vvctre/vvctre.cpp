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

#define SDL_MAIN_HANDLED
#include <SDL.h>
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
#include "core/frontend/framebuffer_layout.h"
#include "core/frontend/scope_acquire_context.h"
#include "core/gdbstub/gdbstub.h"
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
#include "vvctre/config.h"
#include "vvctre/emu_window/emu_window_sdl2.h"

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

    enum class Command {
        BootOrInstall,
        Controls,
        DumpRomFS,
        Version,
        Usage,
    } command = Command::BootOrInstall;

    // for BootOrInstall and DumpRomFS
    std::string path;

    // for BootOrInstall
    Config config;
    std::string movie_record;
    std::string movie_play;
    std::string dump_video;
    bool headless = false;
    bool fullscreen = false;
    bool regenerate_console_id = false;
    int rpc_server_port = 47889;

    // for DumpRomFS
    std::string dump_romfs_dir;

    clipp::group controls, cameras, lle_modules;
    for (int i = 0; i < Settings::NativeButton::NumButtons; i++) {
        controls.push_back(
            clipp::option(fmt::format("--{}", Settings::NativeButton::mapping[i]))
                .doc(fmt::format("Set button {}",
                                 Common::ToUpper(Settings::NativeButton::mapping[i]))) &
            clipp::value("value").set(Settings::values.current_input_profile.buttons[i]));
    }
    for (int i = 0; i < Settings::NativeAnalog::NumAnalogs; i++) {
        controls.push_back(
            clipp::option(fmt::format("--{}", Settings::NativeAnalog::mapping[i]))
                .doc(fmt::format("Set analog {}",
                                 Common::ToUpper(Settings::NativeAnalog::mapping[i]))) &
            clipp::value("value").set(Settings::values.current_input_profile.analogs[i]));
    }
    for (int i = 0; i < Service::CAM::NumCameras; i++) {
        std::string camera;
        switch (i) {
        case Service::CAM::OuterRightCamera: {
            camera = "outer right";
            break;
        }
        case Service::CAM::InnerCamera: {
            camera = "inner";
            break;
        }
        case Service::CAM::OuterLeftCamera: {
            camera = "outer left";
            break;
        }
        }

        const std::string camera_name_for_option = Common::ReplaceAll(camera, " ", "-");

        cameras.push_back(clipp::option(fmt::format("--{}-camera-engine", camera_name_for_option))
                              .doc(fmt::format("set {} camera engine. default: blank", camera)) &
                          clipp::value("value").set(Settings::values.camera_name[i]));

        cameras.push_back(
            clipp::option(fmt::format("--{}-camera-configuration", camera_name_for_option))
                .doc(fmt::format("set {} camera configuration", camera)) &
            clipp::value("value").set(Settings::values.camera_config[i]));

        cameras.push_back(clipp::option(fmt::format("--{}-camera-flip", camera_name_for_option))
                              .doc(fmt::format("set {} camera flip (0: None (default), 1: "
                                               "Horizontal, 2: Vertical, 3: Reverse)",
                                               camera)) &
                          clipp::value("value").set(Settings::values.camera_flip[i]));
    }
    for (auto& service_module : Service::service_module_map) {
        lle_modules.push_back(
            clipp::option(fmt::format("--lle-{}-module", Common::ToLower(service_module.name)))
                .set(Settings::values.lle_modules[service_module.name], true)
                .doc(fmt::format("LLE the {} module", service_module.name)));
    }

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
          clipp::option("--audio-volume").doc("set audio volume") &
              clipp::value("value").set(Settings::values.volume),
          clipp::option("--audio-engine").doc("set audio engine") &
              clipp::value("name").set(Settings::values.sink_id),
          clipp::option("--audio-device").doc("set audio device") &
              clipp::value("name").set(Settings::values.audio_device_id),
          clipp::option("--background-color-red")
                  .doc("set background color red component to a float in range 0.0-1.0") &
              clipp::value("value").set(Settings::values.bg_red),
          clipp::option("--background-color-green")
                  .doc("set background color green component to a float in range 0.0-1.0") &
              clipp::value("value").set(Settings::values.bg_green),
          clipp::option("--background-color-blue")
                  .doc("set background color blue component to a float in range 0.0-1.0") &
              clipp::value("value").set(Settings::values.bg_blue),
          clipp::option("--start-time")
                  .doc("set start time to a Unix timestamp")
                  .set(Settings::values.init_clock, Settings::InitClock::FixedTime) &
              clipp::value("value").set(Settings::values.init_time),
          clipp::option("--real-microphone")
                  .doc("force use a real microphone")
                  .set(Settings::values.mic_input_type, Settings::MicInputType::Real) &
              clipp::value("device").set(Settings::values.mic_input_device),
          clipp::option("--post-processing-shader").doc("set the post processing shader name") &
              clipp::value("name").set(Settings::values.pp_shader_name),
          clipp::option("--rpc-server-port").doc("set RPC server port (default: 47889)") &
              clipp::value("port").set(rpc_server_port),
          clipp::option("--3d-intensity").doc("set 3D intensity") &
              clipp::value("intensity").call([](const char* value) {
                  Settings::values.factor_3d = std::atoi(value);
              }),
          controls, cameras, lle_modules,
          clipp::option("--cpu-jit")
              .doc("force use CPU JIT (default)")
              .set(Settings::values.use_cpu_jit, true),
          clipp::option("--cpu-interpreter")
              .doc("force use CPU interpreter")
              .set(Settings::values.use_cpu_jit, false),
          clipp::option("--dsp-hle")
              .doc("force use DSP HLE (default)")
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
              .doc("force use hardware renderer (default)")
              .set(Settings::values.use_hw_renderer, true),
          clipp::option("--software-renderer")
              .doc("force use software renderer")
              .set(Settings::values.use_hw_renderer, false),
          clipp::option("--hardware-shader")
              .doc("force use hardware shader (default)")
              .set(Settings::values.use_hw_shader, true),
          clipp::option("--software-shader")
              .doc("force use software shader")
              .set(Settings::values.use_hw_shader, false),
          clipp::option("--accurate-multiplication")
              .doc("force use accurate multiplication if using hardware shader")
              .set(Settings::values.shaders_accurate_mul, true),
          clipp::option("--inaccurate-multiplication")
              .doc("force use inaccurate multiplication if using hardware shader (default)")
              .set(Settings::values.shaders_accurate_mul, false),
          clipp::option("--shader-jit")
              .doc("force use JIT for software shader (default)")
              .set(Settings::values.use_shader_jit, true),
          clipp::option("--shader-interpreter")
              .doc("force use interpreter for software shader")
              .set(Settings::values.use_shader_jit, false),
          clipp::option("--enable-disk-shader-caching")
              .doc("force enable disk shader caching (default)")
              .set(Settings::values.use_disk_shader_cache, true),
          clipp::option("--disable-disk-shader-caching")
              .doc("force disable disk shader caching")
              .set(Settings::values.use_disk_shader_cache, false),
          clipp::option("--enable-ignore-format-reinterpretation")
              .doc("force enable ignore format reinterpretation")
              .set(Settings::values.ignore_format_reinterpretation, true),
          clipp::option("--disable-ignore-format-reinterpretation")
              .doc("force disable ignore format reinterpretation (default)")
              .set(Settings::values.ignore_format_reinterpretation, false),
          clipp::option("--dump-textures")
              .doc("force enable texture dumping")
              .set(Settings::values.dump_textures, true),
          clipp::option("--no-dump-textures")
              .doc("force disable texture dumping (default)")
              .set(Settings::values.dump_textures, false),
          clipp::option("--custom-textures")
              .doc("force enable custom textures")
              .set(Settings::values.custom_textures, true),
          clipp::option("--no-custom-textures")
              .doc("force disable custom textures (default)")
              .set(Settings::values.custom_textures, false),
          clipp::option("--preload-custom-textures")
              .doc("force enable custom texture preloading")
              .set(Settings::values.preload_textures, true),
          clipp::option("--no-preload-custom-textures")
              .doc("force disable custom texture preloading (default)")
              .set(Settings::values.preload_textures, false),
          clipp::option("--custom-layout")
              .doc("force use custom layout")
              .set(Settings::values.custom_layout, true),
          clipp::option("--no-custom-layout")
              .doc("force disable custom layout (default)")
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
              .doc("force disable swap screens (default)")
              .set(Settings::values.swap_screen, false),
          clipp::option("--upright-orientation")
              .doc("force upright orientation, for book style games")
              .set(Settings::values.upright_screen, true),
          clipp::option("--no-upright-orientation")
              .doc("force disable upright orientation (default)")
              .set(Settings::values.upright_screen, false),
          clipp::option("--enable-sharper-distant-objects")
              .doc("force enable sharper distant objects")
              .set(Settings::values.sharper_distant_objects, true),
          clipp::option("--disable-sharper-distant-objects")
              .doc("force disable sharper distant objects (default)")
              .set(Settings::values.sharper_distant_objects, false),
          clipp::option("--3d-off")
              .doc("force disable 3D rendering (default)")
              .set(Settings::values.render_3d, Settings::StereoRenderOption::Off),
          clipp::option("--3d-side-by-side")
              .doc("set 3D mode to Side by Side")
              .set(Settings::values.render_3d, Settings::StereoRenderOption::SideBySide),
          clipp::option("--3d-anaglyph")
              .doc("set 3D mode to Anaglyph")
              .set(Settings::values.render_3d, Settings::StereoRenderOption::Anaglyph),
          clipp::option("--use-sd-card")
              .doc("use a virtual SD card (default)")
              .set(Settings::values.use_virtual_sd, true),
          clipp::option("--no-sd-card")
              .doc("don't use a virtual SD card")
              .set(Settings::values.use_virtual_sd, false),
          clipp::option("--old-3ds")
              .doc("makes vvctre emulate a Old 3DS (default)")
              .set(Settings::values.is_new_3ds, false),
          clipp::option("--new-3ds")
              .doc("makes vvctre emulate a New 3DS (New 3DS games crash even if this option is "
                   "enabled)")
              .set(Settings::values.is_new_3ds, true),
          clipp::option("--region-auto-select")
              .doc("auto-select the system region (default)")
              .set(Settings::values.region_value, Settings::REGION_VALUE_AUTO_SELECT),
          clipp::option("--region-japan")
              .doc("set the system region to Japan")
              .set(Settings::values.region_value, 0),
          clipp::option("--region-usa")
              .doc("set the system region to USA")
              .set(Settings::values.region_value, 1),
          clipp::option("--region-europe")
              .doc("set the system region to Europe")
              .set(Settings::values.region_value, 2),
          clipp::option("--region-australia")
              .doc("set the system region to Australia")
              .set(Settings::values.region_value, 3),
          clipp::option("--region-china")
              .doc("set the system region to China")
              .set(Settings::values.region_value, 4),
          clipp::option("--region-korea")
              .doc("set the system region to Korea")
              .set(Settings::values.region_value, 5),
          clipp::option("--region-taiwan")
              .doc("set the system region to Taiwan")
              .set(Settings::values.region_value, 6),
          clipp::option("--system-clock")
              .doc("force use system clock when vvctre starts (default)")
              .set(Settings::values.init_clock, Settings::InitClock::SystemTime),
          clipp::option("--nearest-filtering")
              .doc("use nearest filtering")
              .set(Settings::values.filter_mode, false),
          clipp::option("--linear-filtering")
              .doc("use linear filtering (default)")
              .set(Settings::values.filter_mode, true),
          clipp::option("--null-microphone")
              .doc("force use a null microphone (default)")
              .set(Settings::values.mic_input_type, Settings::MicInputType::None),
          clipp::option("--static-microphone")
              .doc("force use a microphone that returns static samples")
              .set(Settings::values.mic_input_type, Settings::MicInputType::Static),
          clipp::option("--use-vsync")
              .doc("force use VSync (default)")
              .set(Settings::values.use_vsync_new, true),
          clipp::option("--no-vsync")
              .doc("force disable VSync")
              .set(Settings::values.use_vsync_new, false),
          clipp::option("--enable-audio-stretching")
              .doc("force enable audio stretching (default)")
              .set(Settings::values.enable_audio_stretching, true),
          clipp::option("--disable-audio-stretching")
              .doc("force disable audio stretching")
              .set(Settings::values.enable_audio_stretching, false),
          clipp::option("--enable-frame-time-recording")
              .doc("force enable frame time recording")
              .set(Settings::values.record_frame_times, true),
          clipp::option("--disable-frame-time-recording")
              .doc("force disable frame time recording (default)")
              .set(Settings::values.record_frame_times, false),
          clipp::option("--headless").set(headless).doc("start in headless mode"),
          clipp::option("--fullscreen").set(fullscreen).doc("start in fullscreen mode"),
          clipp::option("--regenerate-console-id")
              .set(regenerate_console_id)
              .doc("regenerate the console ID before booting"),
          clipp::option("--unlimited")
              .set(Settings::values.use_frame_limit, false)
              .doc("disable the speed limiter")) |
         clipp::command("controls").set(command, Command::Controls).doc("configure controls") |
         (clipp::command("dump-romfs").set(command, Command::DumpRomFS).doc("dump RomFS"),
          clipp::opt_value("file").set(path), clipp::opt_value("dir").set(dump_romfs_dir)) |
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

            Core::System& system = Core::System::GetInstance();

            std::unique_ptr<EmuWindow_SDL2> emu_window =
                std::make_unique<EmuWindow_SDL2>(system, headless, fullscreen);

            // Register frontend applets
            system.RegisterSoftwareKeyboard(std::make_shared<Frontend::SDL2_SoftwareKeyboard>(
                [&emu_window] { emu_window->SoftwareKeyboardStarted(); }));
            system.RegisterMiiSelector(std::make_shared<Frontend::SDL2_MiiSelector>(
                [&emu_window] { emu_window->MiiPickerStarted(); }));

            Frontend::ScopeAcquireContext scope(*emu_window);

            const Core::System::ResultStatus load_result = system.Load(*emu_window, path);

            switch (load_result) {
            case Core::System::ResultStatus::ErrorNotInitialized:
                LOG_CRITICAL(Frontend, "CPU not initialized");
                return -1;
            case Core::System::ResultStatus::ErrorGetLoader:
                LOG_CRITICAL(Frontend, "Failed to obtain loader for {}!", path);
                return -1;
            case Core::System::ResultStatus::ErrorSystemMode:
                LOG_CRITICAL(Frontend, "Failed to determine system mode!");
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
                LOG_CRITICAL(Frontend, "The ROM format is not supported.");
                return -1;
            case Core::System::ResultStatus::ErrorVideoCore:
                LOG_CRITICAL(Frontend, "VideoCore error. Ensure that you have the latest graphics "
                                       "drivers for your GPU.");
                return -1;
            case Core::System::ResultStatus::ErrorVideoCore_ErrorGenericDrivers:
                LOG_CRITICAL(
                    Frontend,
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

            RPC::Server rpc_server(system, rpc_server_port);

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

            if (Settings::values.use_disk_shader_cache) {
                std::atomic_bool stop_run;
                system.Renderer().Rasterizer()->LoadDiskResources(
                    stop_run,
                    [&](VideoCore::LoadCallbackStage stage, std::size_t value, std::size_t total) {
                        LOG_DEBUG(Frontend, "Loading stage {} progress {} {}",
                                  static_cast<u32>(stage), value, total);
                        emu_window->DiskShaderCacheProgress(stage, value, total);
                    });
            } else {
                emu_window->DiskShaderCacheProgress(VideoCore::LoadCallbackStage::Complete, 0, 0);
            }

            while (emu_window->IsOpen()) {
                switch (system.RunLoop()) {
                case Core::System::ResultStatus::Success: {
                    break;
                }
                case Core::System::ResultStatus::ShutdownRequested: {
                    emu_window->Close();
                    break;
                }
                case Core::System::ResultStatus::Paused: {
                    while (system.GetStatus() == Core::System::ResultStatus::Paused) {
                        std::this_thread::yield();
                    }
                    break;
                }
                default: { break; }
                }
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
        SDL_Event event;
        SDL_Window* window;

        const auto GetInput = [&](const char* mapping, InputCommon::Polling::DeviceType type) {
            switch (type) {
            case InputCommon::Polling::DeviceType::Button: {
                fmt::print("Current button: {}. After pressing the enter key, press "
                           "a key or button\n",
                           Common::ToUpper(mapping));
                std::cin.get();

                InputCommon::Init();
                SDL_Init(SDL_INIT_VIDEO);

                window = SDL_CreateWindow("", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 0,
                                          0, SDL_WINDOW_BORDERLESS);

                SDL_RaiseWindow(window);

                SCOPE_EXIT({
                    InputCommon::Shutdown();
                    SDL_Quit();
                });

                auto pollers = InputCommon::Polling::GetPollers(type);

                for (auto& poller : pollers) {
                    poller->Start();
                }

                SCOPE_EXIT({
                    for (auto& poller : pollers) {
                        poller->Stop();
                    }
                });

                for (;;) {
                    for (auto& poller : pollers) {
                        const Common::ParamPackage params = poller->GetNextInput();
                        if (params.Has("engine")) {
                            return params;
                        }
                    }

                    while (SDL_PollEvent(&event)) {
                        if (event.type == SDL_KEYUP) {
                            return Common::ParamPackage(
                                InputCommon::GenerateKeyboardParam(event.key.keysym.scancode));
                        }
                    }
                }

                break;
            }
            case InputCommon::Polling::DeviceType::Analog: {
                fmt::print("Current stick: {}. After pressing the enter key,\nFor a keyboard, "
                           "press the keys for up, down, left, right, and modifier.\nFor a "
                           "gamepad, first move "
                           "a stick to the right, and then to the bottom.\n",
                           Common::ToUpper(mapping));
                std::cin.get();

                InputCommon::Init();
                SDL_Init(SDL_INIT_VIDEO);

                window = SDL_CreateWindow("", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 0,
                                          0, SDL_WINDOW_BORDERLESS | SDL_WINDOW_INPUT_FOCUS);

                SDL_RaiseWindow(window);

                SCOPE_EXIT({
                    InputCommon::Shutdown();
                    SDL_Quit();
                });

                auto pollers = InputCommon::Polling::GetPollers(type);

                for (auto& poller : pollers) {
                    poller->Start();
                }

                SCOPE_EXIT({
                    for (auto& poller : pollers) {
                        poller->Stop();
                    }
                });

                std::vector<int> keyboard_scancodes;

                for (;;) {
                    for (auto& poller : pollers) {
                        const Common::ParamPackage params = poller->GetNextInput();
                        if (params.Has("engine")) {
                            return params;
                        }
                    }

                    while (SDL_PollEvent(&event)) {
                        if (event.type == SDL_KEYUP) {
                            pollers.clear();
                            keyboard_scancodes.push_back(event.key.keysym.scancode);
                            if (keyboard_scancodes.size() == 5) {
                                return Common::ParamPackage(
                                    InputCommon::GenerateAnalogParamFromKeys(
                                        keyboard_scancodes[0], keyboard_scancodes[1],
                                        keyboard_scancodes[2], keyboard_scancodes[3],
                                        keyboard_scancodes[4], 0.5f));
                            }
                        }
                    }
                }

                break;
            }
            default: { return Common::ParamPackage(); }
            }
        };

        std::vector<std::string> lines;

        for (const auto& mapping : Settings::NativeButton::mapping) {
            const Common::ParamPackage params =
                GetInput(mapping, InputCommon::Polling::DeviceType::Button);
            lines.push_back(fmt::format("{}={}", mapping, params.Serialize()));
        }

        for (const auto& mapping : Settings::NativeAnalog::mapping) {
            const Common::ParamPackage params =
                GetInput(mapping, InputCommon::Polling::DeviceType::Analog);
            lines.push_back(fmt::format("{}={}", mapping, params.Serialize()));
        }

        fmt::print("Change the [Controls] section in the ini file to "
                   "this:\n\n[Controls]\n");

        for (const std::string& line : lines) {
            fmt::print("{}\n", line);
        }

        break;
    }
    case Command::DumpRomFS: {
        InitializeLogging();

        if (path.empty()) {
            const std::vector<std::string> result =
                pfd::open_file("Open File", ".",
                               {"3DS Executables", "*.cci *.3ds *.cxi *.3dsx *.app"})
                    .result();

            if (result.empty()) {
                return -1;
            } else {
                path = result[0];
            }
        }

        if (dump_romfs_dir.empty()) {
            const std::string result = pfd::select_folder("Dump RomFS", ".").result();

            if (result.empty()) {
                return -1;
            } else {
                dump_romfs_dir = result;
            }
        }

        auto loader = Loader::GetLoader(path);
        if (loader != nullptr &&
            loader->DumpRomFS(dump_romfs_dir) == Loader::ResultStatus::Success) {
            loader->DumpUpdateRomFS(dump_romfs_dir);
            LOG_INFO(Frontend, "Done");
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

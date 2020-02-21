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
#include "core/dumping/backend.h"
#include "core/file_sys/cia_container.h"
#include "core/frontend/framebuffer_layout.h"
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
#include "vvctre/camera/image.h"
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
    std::string movie_record;
    std::string movie_play;
    std::string dump_video;
    bool hidden = false;
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

        cameras.push_back(
            clipp::option(fmt::format("--{}-camera-engine", camera_name_for_option))
                .doc(fmt::format("set {} camera engine.\ndefault: blank\nengine \"blank\" returns "
                                 "a black image.\nengine \"image\" returns a static image.",
                                 camera)) &
            clipp::value("value").set(Settings::values.camera_name[i]));

        cameras.push_back(
            clipp::option(fmt::format("--{}-camera-configuration", camera_name_for_option))
                .doc(fmt::format(
                    "set {} camera configuration\nfor engine \"image\", this is the file path.",
                    camera)) &
            clipp::value("value").set(Settings::values.camera_config[i]));

        cameras.push_back(clipp::option(fmt::format("--{}-camera-flip", camera_name_for_option))
                              .doc(fmt::format("set {} camera flip\n0: None (default)\n1: "
                                               "Horizontal\n2: Vertical\n3: Reverse",
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
          clipp::option("--custom-cpu-ticks").doc("set custom CPU ticks\ndefault: 77") &
              clipp::value("ticks")
                  .set(Settings::values.use_custom_cpu_ticks, true)
                  .set(Settings::values.custom_cpu_ticks),
          clipp::option("--cpu-clock-percentage")
                  .doc("set CPU clock percentage\n"
                       "underclocking can increase the performance of the game at the risk of "
                       "freezing.\noverclocking may fix lag that happens on console, but also "
                       "comes with the risk of freezing.\n"
                       "range is any positive integer (but we suspect 25 - 400 is a good "
                       "idea)\ndefault: 100") &
              clipp::value("percentage").set(Settings::values.cpu_clock_percentage),
          clipp::option("--multiplayer-server-url")
                  .doc("set the multiplayer server URL\ndefault: "
                       "ws://vvctre-multiplayer.glitch.me") &
              clipp::value("url").set(Settings::values.multiplayer_url),
          clipp::option("--log-filter").doc("set the log filter\ndefault: *:Info") &
              clipp::value("filter").set(Settings::values.log_filter),
          clipp::option("--minimum-vertices-per-thread")
                  .doc("set minimum vertices per thread (only used for software shader)\ndefault: "
                       "10") &
              clipp::value("value").set(Settings::values.min_vertices_per_thread),
          clipp::option("--resolution").doc("set resolution\ndefault: 1\n0 means use window size") &
              clipp::value("value").set(Settings::values.resolution_factor),
          clipp::option("--audio-speed")
                  .doc("set audio speed for DSP HLE to a float, must be greater than zero and "
                       "requires audio stretching to be enabled to work properly") &
              clipp::value("value").set(Settings::values.audio_speed),
          clipp::option("--audio-volume").doc("set audio volume\ntype: float\ndefault: 1.0") &
              clipp::value("value").set(Settings::values.volume),
          clipp::option("--audio-engine")
                  .doc("set audio engine\ndefault: auto (uses the highest available "
                       "engine)\nengines:\n- "
                       "(optional) cubeb\n- sdl2") &
              clipp::value("name").set(Settings::values.sink_id),
          clipp::option("--audio-device").doc("set audio device\ndefault: auto") &
              clipp::value("name").set(Settings::values.audio_device_id),
          clipp::option("--background-color-red")
                  .doc("set background color red component\ntype: float\nrange: 0.0-1.0\ndefault: "
                       "0.0") &
              clipp::value("value").set(Settings::values.bg_red),
          clipp::option("--background-color-green")
                  .doc(
                      "set background color green component\ntype: float\nrange: 0.0-1.0\ndefault: "
                      "0.0") &
              clipp::value("value").set(Settings::values.bg_green),
          clipp::option("--background-color-blue")
                  .doc("set background color blue component\ntype: float\nrange: 0.0-1.0\ndefault: "
                       "0.0") &
              clipp::value("value").set(Settings::values.bg_blue),
          clipp::option("--start-time")
                  .doc("set start time to a Unix timestamp")
                  .set(Settings::values.init_clock, Settings::InitClock::FixedTime) &
              clipp::value("value").set(Settings::values.init_time),
          clipp::option("--real-microphone")
                  .doc("use a real microphone")
                  .set(Settings::values.mic_input_type, Settings::MicInputType::Real) &
              clipp::value("device").set(Settings::values.mic_input_device),
          clipp::option("--post-processing-shader")
                  .doc("set the post processing shader name, vvctre includes \"dubois (builtin)\" "
                       "(anaglyph 3D only) and \"horizontal (builtin)\" (interlaced 3D only)") &
              clipp::value("name").set(Settings::values.pp_shader_name),
          clipp::option("--rpc-server-port").doc("set RPC server port (default: 47889)") &
              clipp::value("port").set(rpc_server_port),
          clipp::option("--3d-intensity").doc("set 3D intensity") &
              clipp::value("intensity").call([](const char* value) {
                  Settings::values.factor_3d = std::atoi(value);
              }),
          controls,
          clipp::option("--udp-input-address").doc("set UDP input address\ndefault: 127.0.0.1") &
              clipp::value("address").set(Settings::values.current_input_profile.udp_input_address),
          clipp::option("--udp-input-port").doc("set UDP input port\ndefault: 26760") &
              clipp::value("port").set(Settings::values.current_input_profile.udp_input_port),
          clipp::option("--udp-pad-index").doc("set UDP pad index\ndefault: 0") &
              clipp::value("index").set(Settings::values.current_input_profile.udp_pad_index),
          clipp::option("--motion-device")
                  .doc("set motion device parameters\nuse the emulator window (default): "
                       "engine:motion_emu,update_period:100,sensitivity:0.01,tilt_clamp:90.0\nuse "
                       "controller: engine:cemuhookudp") &
              clipp::value("parameters").set(Settings::values.current_input_profile.motion_device),
          clipp::option("--touch-device")
                  .doc("set touch device parameters\nuse the emulator window (default): "
                       "engine:emu_window\nuse controller: "
                       "engine:cemuhookudp,min_x:__,min_y:__,max_x:__,max_y:__") &
              clipp::value("parameters").set(Settings::values.current_input_profile.touch_device),
          clipp::option("--texture-filter-name")
                  .doc("set texture filter name\ndefault: none\nvalid values:\n- none\n- xBRZ "
                       "freescale\n- Anime4K Ultrafast") &
              clipp::value("name").set(Settings::values.texture_filter_name),
          clipp::option("--texture-filter-factor").doc("set texture filter name\ndefault: 1") &
              clipp::value("factor").set(Settings::values.texture_filter_factor),
          cameras, lle_modules,
          clipp::option("--cpu-interpreter")
              .doc("use CPU interpreter instead of JIT")
              .set(Settings::values.use_cpu_jit, false),
          clipp::option("--dsp-lle")
              .doc("use DSP LLE single-threaded")
              .set(Settings::values.enable_dsp_lle, true)
              .set(Settings::values.enable_dsp_lle_multithread, false),
          clipp::option("--dsp-lle-multi-threaded")
              .doc("use DSP LLE multi-threaded")
              .set(Settings::values.enable_dsp_lle, true)
              .set(Settings::values.enable_dsp_lle_multithread, true),
          clipp::option("--software-renderer")
              .doc("use software renderer instead of hardware renderer")
              .set(Settings::values.use_hw_renderer, false),
          clipp::option("--software-shader")
              .doc("use software shader instead of hardware shader")
              .set(Settings::values.use_hw_shader, false),
          clipp::option("--accurate-multiplication")
              .doc("use accurate multiplication instead of inaccurate multiplication if using "
                   "hardware shader")
              .set(Settings::values.shaders_accurate_mul, true),
          clipp::option("--shader-interpreter")
              .doc("use interpreter instead of JIT for software shader")
              .set(Settings::values.use_shader_jit, false),
          clipp::option("--disable-disk-shader-caching")
              .doc("disable disk shader caching")
              .set(Settings::values.use_disk_shader_cache, false),
          clipp::option("--enable-ignore-format-reinterpretation")
              .doc("enable ignore format reinterpretation")
              .set(Settings::values.ignore_format_reinterpretation, true),
          clipp::option("--dump-textures")
              .doc("dump textures")
              .set(Settings::values.dump_textures, true),
          clipp::option("--custom-textures")
              .doc("use custom textures")
              .set(Settings::values.custom_textures, true),
          clipp::option("--preload-custom-textures")
              .doc("preload custom textures")
              .set(Settings::values.preload_textures, true),
          clipp::option("--custom-layout")
              .doc("use custom layout")
              .set(Settings::values.custom_layout, true),
          clipp::option("--custom-layout-top-left").doc("set custom layout top left\ndefault: 0") &
              clipp::value("value").set(Settings::values.custom_top_left),
          clipp::option("--custom-layout-top-top").doc("set custom layout top top\ndefault: 0") &
              clipp::value("value").set(Settings::values.custom_top_top),
          clipp::option("--custom-layout-top-right")
                  .doc("set custom layout top right\ndefault: 400") &
              clipp::value("value").set(Settings::values.custom_top_right),
          clipp::option("--custom-layout-top-bottom")
                  .doc("set custom layout top bottom\ndefault: 240") &
              clipp::value("value").set(Settings::values.custom_top_bottom),
          clipp::option("--custom-layout-bottom-left")
                  .doc("set custom layout bottom left\ndefault: 40") &
              clipp::value("value").set(Settings::values.custom_bottom_left),
          clipp::option("--custom-layout-bottom-top")
                  .doc("set custom layout bottom top\ndefault: 240") &
              clipp::value("value").set(Settings::values.custom_bottom_top),
          clipp::option("--custom-layout-bottom-right")
                  .doc("set custom layout bottom right\ndefault: 360") &
              clipp::value("value").set(Settings::values.custom_bottom_right),
          clipp::option("--custom-layout-bottom-bottom")
                  .doc("set custom layout bottom bottom\ndefault: 480") &
              clipp::value("value").set(Settings::values.custom_bottom_bottom),
          clipp::option("--single-screen-layout")
              .doc("use single screen layout")
              .set(Settings::values.custom_layout, false)
              .set(Settings::values.layout_option, Settings::LayoutOption::SingleScreen),
          clipp::option("--large-screen-layout")
              .doc("use Large Screen Small Screen layout")
              .set(Settings::values.custom_layout, false)
              .set(Settings::values.layout_option, Settings::LayoutOption::LargeScreen),
          clipp::option("--side-by-side-layout")
              .doc("use side by side layout")
              .set(Settings::values.custom_layout, false)
              .set(Settings::values.layout_option, Settings::LayoutOption::SideScreen),
          clipp::option("--medium-screen-layout")
              .doc("use Large Screen Medium Screen layout")
              .set(Settings::values.custom_layout, false)
              .set(Settings::values.layout_option, Settings::LayoutOption::MediumScreen),
          clipp::option("--swap-screens")
              .doc("swap screens")
              .set(Settings::values.swap_screen, true),
          clipp::option("--upright-orientation")
              .doc("use upright orientation, for book style games")
              .set(Settings::values.upright_screen, true),
          clipp::option("--enable-sharper-distant-objects")
              .doc("enable sharper distant objects")
              .set(Settings::values.sharper_distant_objects, true),
          clipp::option("--3d-side-by-side")
              .doc("use Side by Side 3D")
              .set(Settings::values.render_3d, Settings::StereoRenderOption::SideBySide),
          clipp::option("--3d-anaglyph")
              .doc("use Anaglyph 3D")
              .set(Settings::values.render_3d, Settings::StereoRenderOption::Anaglyph),
          clipp::option("--no-sd-card")
              .doc("don't use a virtual SD card")
              .set(Settings::values.use_virtual_sd, false),
          clipp::option("--new-3ds")
              .doc("makes vvctre emulate a New 3DS")
              .set(Settings::values.is_new_3ds, true),
          clipp::option("--region-japan")
              .doc("force the system region to Japan")
              .set(Settings::values.region_value, 0),
          clipp::option("--region-usa")
              .doc("force the system region to USA")
              .set(Settings::values.region_value, 1),
          clipp::option("--region-europe")
              .doc("force the system region to Europe")
              .set(Settings::values.region_value, 2),
          clipp::option("--region-australia")
              .doc("force the system region to Australia")
              .set(Settings::values.region_value, 3),
          clipp::option("--region-china")
              .doc("force the system region to China")
              .set(Settings::values.region_value, 4),
          clipp::option("--region-korea")
              .doc("force the system region to Korea")
              .set(Settings::values.region_value, 5),
          clipp::option("--region-taiwan")
              .doc("force the system region to Taiwan")
              .set(Settings::values.region_value, 6),
          clipp::option("--nearest-filtering")
              .doc("use nearest filtering instead of linear filtering")
              .set(Settings::values.filter_mode, false),
          clipp::option("--static-microphone")
              .doc("use a microphone that returns static samples")
              .set(Settings::values.mic_input_type, Settings::MicInputType::Static),
          clipp::option("--enable-vsync")
              .doc("enable VSync")
              .set(Settings::values.enable_vsync, true),
          clipp::option("--disable-audio-stretching")
              .doc("disable audio stretching")
              .set(Settings::values.enable_audio_stretching, false),
          clipp::option("--record-frame-times")
              .doc("record frame times")
              .set(Settings::values.record_frame_times, true),
          clipp::option("--hidden").set(hidden).doc("hide the window"),
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
            indicators::ProgressBar bar;
            bar.set_option(indicators::option::PrefixText{"Installing CIA "});

            return Service::AM::InstallCIA(path,
                                           [&](std::size_t written, std::size_t total) {
                                               bar.set_progress(written * 100 / total);
                                           }) == Service::AM::InstallStatus::Success
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
                std::make_unique<EmuWindow_SDL2>(system, hidden, fullscreen);

            // Register frontend applets
            system.RegisterSoftwareKeyboard(std::make_shared<Frontend::SDL2_SoftwareKeyboard>(
                [&emu_window] { emu_window->SoftwareKeyboardStarted(); }));
            system.RegisterMiiSelector(std::make_shared<Frontend::SDL2_MiiSelector>(
                [&emu_window] { emu_window->MiiPickerStarted(); }));

            // Register camera implementations
            Camera::RegisterFactory("image", std::make_unique<Camera::ImageCameraFactory>());

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

            if (Settings::values.use_disk_shader_cache) {
                indicators::ProgressBar bar;
                std::atomic_bool stop_run{false};

                system.Renderer().Rasterizer()->LoadDiskResources(
                    stop_run,
                    [&](VideoCore::LoadCallbackStage stage, std::size_t value, std::size_t total) {
                        switch (stage) {
                        case VideoCore::LoadCallbackStage::Prepare:
                            break;
                        case VideoCore::LoadCallbackStage::Decompile: {
                            bar.set_option(indicators::option::PrefixText{"Decompiling shaders "});
                            bar.set_progress(value * 100 / total);
                            break;
                        }
                        case VideoCore::LoadCallbackStage::Build: {
                            bar.set_option(indicators::option::PrefixText{"Building shaders "});
                            bar.set_progress(value * 100 / total);
                            break;
                        }
                        case VideoCore::LoadCallbackStage::Complete: {
                            break;
                        }
                        }
                    });
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

        std::string options;

        for (const auto& mapping : Settings::NativeButton::mapping) {
            const Common::ParamPackage params =
                GetInput(mapping, InputCommon::Polling::DeviceType::Button);
            options += fmt::format("--{} \"{}\" ", mapping, params.Serialize());
        }

        for (const auto& mapping : Settings::NativeAnalog::mapping) {
            const Common::ParamPackage params =
                GetInput(mapping, InputCommon::Polling::DeviceType::Analog);
            options += fmt::format("--{} \"{}\" ", mapping, params.Serialize());
        }

        fmt::print("Options: {}\n", options);

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

// Copyright 2020 vvctre project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <cstdlib>
#include <string>
#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <fmt/format.h>
#include <glad/glad.h>
#include <imgui.h>
#include <imgui_impl_opengl3.h>
#include <imgui_impl_sdl.h>
#include <imgui_stdlib.h>
#include <portable-file-dialogs.h>
#ifdef HAVE_CUBEB
#include "audio_core/cubeb_input.h"
#endif
#include "audio_core/sink.h"
#include "audio_core/sink_details.h"
#include "common/file_util.h"
#include "common/logging/log.h"
#include "common/param_package.h"
#include "common/string_util.h"
#include "common/version.h"
#include "core/core.h"
#include "core/hle/service/am/am.h"
#include "core/hle/service/cfg/cfg.h"
#include "core/hle/service/ptm/ptm.h"
#include "core/movie.h"
#include "core/settings.h"
#include "input_common/main.h"
#include "video_core/renderer_opengl/post_processing_opengl.h"
#include "video_core/renderer_opengl/texture_filters/texture_filter_manager.h"
#include "vvctre/configuration.h"

Configuration::Configuration() {
    const std::string window_title =
        fmt::format("vvctre {} Configuration", version::vvctre.to_string());

    render_window = SDL_CreateWindow(
        window_title.c_str(),
        SDL_WINDOWPOS_UNDEFINED, // x position
        SDL_WINDOWPOS_UNDEFINED, // y position
        1024, 768, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);

    if (render_window == nullptr) {
        LOG_CRITICAL(Frontend, "Failed to create SDL2 window: {}", SDL_GetError());
        std::exit(1);
    }

    gl_context = SDL_GL_CreateContext(render_window);
    if (gl_context == nullptr) {
        LOG_CRITICAL(Frontend, "Failed to create SDL2 GL context: {}", SDL_GetError());
        std::exit(1);
    }

    if (!gladLoadGLLoader(static_cast<GLADloadproc>(SDL_GL_GetProcAddress))) {
        LOG_CRITICAL(Frontend, "Failed to initialize GL functions: {}", SDL_GetError());
        std::exit(1);
    }

    SDL_GL_SetSwapInterval(1);

    SDL_PumpEvents();

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui::GetIO().IniFilename = nullptr;
    ImGui_ImplSDL2_InitForOpenGL(render_window, gl_context);
    ImGui_ImplOpenGL3_Init("#version 330 core");

    ImGui::GetStyle().WindowRounding = 0.0f;
    ImGui::GetStyle().ChildRounding = 0.0f;
    ImGui::GetStyle().FrameRounding = 0.0f;
    ImGui::GetStyle().GrabRounding = 0.0f;
    ImGui::GetStyle().PopupRounding = 0.0f;
    ImGui::GetStyle().ScrollbarRounding = 0.0f;
}

void Configuration::Run() {
    SDL_Event event;
    auto cfg = std::make_shared<Service::CFG::Module>();

    for (;;) {
        // Poll events
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL2_ProcessEvent(&event);

            if (event.type == SDL_QUIT ||
                (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE)) {
                Settings::values.file_path.clear();
                return;
            }
        }

        // Draw window
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame(render_window);
        ImGui::NewFrame();
        ImGuiIO& io = ImGui::GetIO();
        if (ImGui::Begin("", nullptr,
                         ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoTitleBar |
                             ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse |
                             ImGuiWindowFlags_NoMove)) {
            ImGui::SetWindowPos(ImVec2(), ImGuiCond_Once);
            ImGui::SetWindowSize(io.DisplaySize);
            if (ImGui::BeginTabBar("##tabBar")) {
                if (ImGui::BeginTabItem("Start")) {
                    ImGui::Text("Start settings are not persistent.");
                    ImGui::NewLine();

                    ImGui::Text("File: %s", Settings::values.file_path.empty()
                                                ? "[not set]"
                                                : Settings::values.file_path.c_str());
                    ImGui::SameLine();
                    if (ImGui::Button("Browse...##file")) {
                        const std::vector<std::string> result =
                            pfd::open_file("Browse", ".",
                                           {"All supported files",
                                            "*.cci *.3ds *.cxi *.3dsx *.app *.elf *.axf",
                                            "Cartridges", "*.cci *.3ds", "NCCHs", "*.cxi *.app",
                                            "Homebrew", "*.3dsx *.elf *.axf"})
                                .result();
                        if (!result.empty()) {
                            Settings::values.file_path = result[0];
                        }
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Install CIA")) {
                        const std::vector<std::string> files =
                            pfd::open_file("Install CIA", ".", {"CTR Importable Archive", "*.cia"},
                                           true)
                                .result();

                        for (const auto& file : files) {
                            const Service::AM::InstallStatus status = Service::AM::InstallCIA(file);

                            switch (status) {
                            case Service::AM::InstallStatus::Success:
                                pfd::message("vvctre", fmt::format("{} installed", file),
                                             pfd::choice::ok);
                                break;
                            case Service::AM::InstallStatus::ErrorFailedToOpenFile:
                                pfd::message("vvctre", fmt::format("Failed to open {}", file),
                                             pfd::choice::ok, pfd::icon::error);
                                break;
                            case Service::AM::InstallStatus::ErrorFileNotFound:
                                pfd::message("vvctre", fmt::format("{} not found", file),
                                             pfd::choice::ok, pfd::icon::error);
                                break;
                            case Service::AM::InstallStatus::ErrorAborted:
                                pfd::message("vvctre", fmt::format("{} installation aborted", file),
                                             pfd::choice::ok, pfd::icon::error);
                                break;
                            case Service::AM::InstallStatus::ErrorInvalid:
                                pfd::message("vvctre", fmt::format("{} is invalid", file),
                                             pfd::choice::ok, pfd::icon::error);
                                break;
                            case Service::AM::InstallStatus::ErrorEncrypted:
                                pfd::message("vvctre", fmt::format("{} is encrypted", file),
                                             pfd::choice::ok, pfd::icon::error);
                                break;
                            }
                        }
                    }

                    if (Settings::values.record_movie.empty()) {
                        ImGui::Text("Play Movie: %s", Settings::values.play_movie.empty()
                                                          ? "[not set]"
                                                          : Settings::values.play_movie.c_str());
                        ImGui::SameLine();
                        if (ImGui::Button("Browse...##playmovie")) {
                            const std::vector<std::string> result =
                                pfd::open_file("Play Movie", ".", {"VvCtre Movie", "*.vcm"})
                                    .result();
                            if (!result.empty()) {
                                Settings::values.play_movie = result[0];
                            }
                        }
                    }

                    if (Settings::values.play_movie.empty()) {
                        ImGui::Text("Record Movie: %s",
                                    Settings::values.record_movie.empty()
                                        ? "[not set]"
                                        : Settings::values.record_movie.c_str());
                        ImGui::SameLine();
                        if (ImGui::Button("Browse...##recordmovie")) {
                            const std::string record_movie =
                                pfd::save_file("Record Movie", "movie.vcm",
                                               {"VvCtre Movie", "*.vcm"})
                                    .result();
                            if (!record_movie.empty()) {
                                Settings::values.record_movie = record_movie;
                            }
                        }
                    }

                    ImGui::Text("Log Filter");
                    ImGui::SameLine();
                    ImGui::InputText("##logfilter", &Settings::values.log_filter);

                    ImGui::Text("RPC Server Port");
                    ImGui::SameLine();
                    ImGui::InputInt("##rpcserverport", &Settings::values.rpc_server_port);

                    ImGui::Checkbox("Start in Fullscreen Mode",
                                    &Settings::values.start_in_fullscreen_mode);

                    ImGui::Checkbox("Record Frame Times", &Settings::values.record_frame_times);

                    ImGui::Checkbox("Enable GDB Stub", &Settings::values.use_gdbstub);
                    if (Settings::values.use_gdbstub) {
                        ImGui::SameLine();
                        ImGui::Spacing();
                        ImGui::SameLine();
                        ImGui::Text("GDB Stub Port:");
                        ImGui::SameLine();
                        ImGui::InputScalar("##gdbstubport", ImGuiDataType_U16,
                                           &Settings::values.gdbstub_port);
                    }

                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem("Performance")) {
                    ImGui::Text("Performance settings are not persistent.");
                    ImGui::NewLine();

                    ImGui::Checkbox("Use CPU JIT", &Settings::values.use_cpu_jit);
                    ImGui::Checkbox("Limit Speed", &Settings::values.use_frame_limit);

                    ImGui::Text("Speed Limit");
                    ImGui::SameLine();
                    const u16 min = 1;
                    const u16 max = 500;

                    ImGui::SliderScalar("##speedlimit", ImGuiDataType_U16,
                                        &Settings::values.frame_limit, &min, &max);

                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem("Audio")) {
                    ImGui::Text("Audio settings are not persistent.");
                    ImGui::NewLine();

                    ImGui::Checkbox("Enable DSP LLE", &Settings::values.enable_dsp_lle);

                    if (Settings::values.enable_dsp_lle) {
                        ImGui::Checkbox("Use multiple threads",
                                        &Settings::values.enable_dsp_lle_multithread);
                    }

                    ImGui::Text("Volume:");
                    ImGui::SameLine();
                    ImGui::SliderFloat("##volume", &Settings::values.volume, 0.0f, 1.0f);

                    ImGui::Text("Speed:");
                    ImGui::SameLine();
                    ImGui::SliderFloat("##speed", &Settings::values.audio_speed, 0.001f, 5.0f);

                    ImGui::Text("Sink:");
                    ImGui::SameLine();
                    if (ImGui::BeginCombo("##sink", Settings::values.sink_id.c_str())) {
                        if (ImGui::Selectable("auto")) {
                            Settings::values.sink_id = "auto";
                        }
#ifdef HAVE_CUBEB
                        if (ImGui::Selectable("cubeb")) {
                            Settings::values.sink_id = "cubeb";
                        }
#endif
                        if (ImGui::Selectable("sdl2")) {
                            Settings::values.sink_id = "sdl2";
                        }
                        ImGui::EndCombo();
                    }

                    ImGui::Text("Device:");
                    ImGui::SameLine();
                    if (ImGui::BeginCombo("##device", Settings::values.audio_device_id.c_str())) {
                        if (ImGui::Selectable(AudioCore::auto_device_name)) {
                            Settings::values.audio_device_id = AudioCore::auto_device_name;
                        }

                        for (const auto& device :
                             AudioCore::GetDeviceListForSink(Settings::values.sink_id)) {
                            if (ImGui::Selectable(device.c_str())) {
                                Settings::values.audio_device_id = device;
                            }
                        }

                        ImGui::EndCombo();
                    }

                    ImGui::Text("Microphone Input Type:");
                    ImGui::SameLine();
                    if (ImGui::BeginCombo("##mic_input_type", [] {
                            switch (Settings::values.mic_input_type) {
                            case Settings::MicInputType::None:
                                return "Disabled";
                            case Settings::MicInputType::Real:
                                return "Real Device";
                            case Settings::MicInputType::Static:
                                return "Static Noise";
                            default:
                                break;
                            }

                            return "Invalid";
                        }())) {
                        if (ImGui::Selectable("Disabled")) {
                            Settings::values.mic_input_type = Settings::MicInputType::None;
                        }
                        if (ImGui::Selectable("Real Device")) {
                            Settings::values.mic_input_type = Settings::MicInputType::Real;
                        }
                        if (ImGui::Selectable("Static Noise")) {
                            Settings::values.mic_input_type = Settings::MicInputType::Static;
                        }
                        ImGui::EndCombo();
                    }

                    if (Settings::values.mic_input_type == Settings::MicInputType::Real) {
                        ImGui::Text("Microphone Device");
                        ImGui::SameLine();

                        if (ImGui::BeginCombo("##microphonedevice",
                                              Settings::values.mic_input_device.c_str())) {
#ifdef HAVE_CUBEB
                            for (const auto& device : AudioCore::ListCubebInputDevices()) {
                                if (ImGui::Selectable(device.c_str())) {
                                    Settings::values.mic_input_device = device;
                                }
                            }
#endif

                            ImGui::EndCombo();
                        }
                    }

                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem("Camera")) {
                    ImGui::Text("Camera settings are not persistent.");
                    ImGui::NewLine();

                    ImGui::Text("Inner Camera Engine");
                    ImGui::SameLine();
                    if (ImGui::BeginCombo("##innerengine",
                                          Settings::values
                                              .camera_name[static_cast<std::size_t>(
                                                  Service::CAM::CameraIndex::InnerCamera)]
                                              .c_str())) {
                        if (ImGui::Selectable("blank")) {
                            Settings::values.camera_name[static_cast<std::size_t>(
                                Service::CAM::CameraIndex::InnerCamera)] = "blank";
                        }
                        if (ImGui::Selectable("image")) {
                            Settings::values.camera_name[static_cast<std::size_t>(
                                Service::CAM::CameraIndex::InnerCamera)] = "image";
                        }
                        ImGui::EndCombo();
                    }

                    ImGui::Text("Inner Camera Configuration");
                    ImGui::SameLine();
                    ImGui::InputText("##innerconfiguration",
                                     &Settings::values.camera_config[static_cast<std::size_t>(
                                         Service::CAM::CameraIndex::InnerCamera)]);

                    ImGui::Text("Outer Left Engine");
                    ImGui::SameLine();
                    if (ImGui::BeginCombo("##outerleftengine",
                                          Settings::values
                                              .camera_name[static_cast<std::size_t>(
                                                  Service::CAM::CameraIndex::OuterLeftCamera)]
                                              .c_str())) {
                        if (ImGui::Selectable("blank")) {
                            Settings::values.camera_name[static_cast<std::size_t>(
                                Service::CAM::CameraIndex::OuterLeftCamera)] = "blank";
                        }
                        if (ImGui::Selectable("image")) {
                            Settings::values.camera_name[static_cast<std::size_t>(
                                Service::CAM::CameraIndex::OuterLeftCamera)] = "image";
                        }
                        ImGui::EndCombo();
                    }

                    ImGui::Text("Outer Left Configuration");
                    ImGui::SameLine();
                    ImGui::InputText("##outerleftconfiguration",
                                     &Settings::values.camera_config[static_cast<std::size_t>(
                                         Service::CAM::CameraIndex::OuterLeftCamera)]);

                    ImGui::Text("Outer Right Engine");
                    ImGui::SameLine();
                    if (ImGui::BeginCombo("##outerrightengine",
                                          Settings::values
                                              .camera_name[static_cast<std::size_t>(
                                                  Service::CAM::CameraIndex::OuterRightCamera)]
                                              .c_str())) {
                        if (ImGui::Selectable("blank")) {
                            Settings::values.camera_name[static_cast<std::size_t>(
                                Service::CAM::CameraIndex::OuterRightCamera)] = "blank";
                        }
                        if (ImGui::Selectable("image")) {
                            Settings::values.camera_name[static_cast<std::size_t>(
                                Service::CAM::CameraIndex::OuterRightCamera)] = "image";
                        }
                        ImGui::EndCombo();
                    }

                    ImGui::Text("Outer Right Configuration");
                    ImGui::SameLine();
                    ImGui::InputText("##outerrightconfiguration",
                                     &Settings::values.camera_config[static_cast<std::size_t>(
                                         Service::CAM::CameraIndex::OuterRightCamera)]);

                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem("System")) {
                    ImGui::Text("System settings are persistent.");
                    ImGui::NewLine();

                    ImGui::Text("Username");
                    ImGui::SameLine();

                    std::string username = Common::UTF16ToUTF8(cfg->GetUsername());
                    if (ImGui::InputText("##username", &username)) {
                        cfg->SetUsername(Common::UTF8ToUTF16(username));
                        cfg->UpdateConfigNANDSavegame();
                    }

                    ImGui::Text("Birthday");
                    ImGui::SameLine();

                    auto [month, day] = cfg->GetBirthday();

                    if (ImGui::InputScalar("##birthday", ImGuiDataType_U8, &day)) {
                        cfg->SetBirthday(month, day);
                        update_config_savegame = true;
                    }

                    ImGui::SameLine();
                    ImGui::Text("/");
                    ImGui::SameLine();

                    if (ImGui::InputScalar("##birthmonth", ImGuiDataType_U8, &month)) {
                        cfg->SetBirthday(month, day);
                        update_config_savegame = true;
                    }

                    ImGui::Text("Language");
                    ImGui::SameLine();

                    const Service::CFG::SystemLanguage language = cfg->GetSystemLanguage();

                    if (ImGui::BeginCombo("##language", [&language] {
                            switch (language) {
                            case Service::CFG::SystemLanguage::LANGUAGE_JP:
                                return "Japanese";
                            case Service::CFG::SystemLanguage::LANGUAGE_EN:
                                return "English";
                            case Service::CFG::SystemLanguage::LANGUAGE_FR:
                                return "French";
                            case Service::CFG::SystemLanguage::LANGUAGE_DE:
                                return "German";
                            case Service::CFG::SystemLanguage::LANGUAGE_IT:
                                return "Italian";
                            case Service::CFG::SystemLanguage::LANGUAGE_ES:
                                return "Spanish";
                            case Service::CFG::SystemLanguage::LANGUAGE_ZH:
                                return "Simplified Chinese";
                            case Service::CFG::SystemLanguage::LANGUAGE_KO:
                                return "Korean";
                            case Service::CFG::SystemLanguage::LANGUAGE_NL:
                                return "Dutch";
                            case Service::CFG::SystemLanguage::LANGUAGE_PT:
                                return "Portugese";
                            case Service::CFG::SystemLanguage::LANGUAGE_RU:
                                return "Russian";
                            case Service::CFG::SystemLanguage::LANGUAGE_TW:
                                return "Traditional Chinese";
                            default:
                                break;
                            }

                            return "Invalid language";
                        }())) {
                        if (ImGui::Selectable("Japanese")) {
                            cfg->SetSystemLanguage(Service::CFG::SystemLanguage::LANGUAGE_JP);
                            update_config_savegame = true;
                        }

                        if (ImGui::Selectable("English")) {
                            cfg->SetSystemLanguage(Service::CFG::SystemLanguage::LANGUAGE_EN);
                            update_config_savegame = true;
                        }

                        if (ImGui::Selectable("French")) {
                            cfg->SetSystemLanguage(Service::CFG::SystemLanguage::LANGUAGE_FR);
                            update_config_savegame = true;
                        }

                        if (ImGui::Selectable("German")) {
                            cfg->SetSystemLanguage(Service::CFG::SystemLanguage::LANGUAGE_DE);
                            update_config_savegame = true;
                        }

                        if (ImGui::Selectable("Italian")) {
                            cfg->SetSystemLanguage(Service::CFG::SystemLanguage::LANGUAGE_IT);
                            update_config_savegame = true;
                        }

                        if (ImGui::Selectable("Spanish")) {
                            cfg->SetSystemLanguage(Service::CFG::SystemLanguage::LANGUAGE_ES);
                            update_config_savegame = true;
                        }

                        if (ImGui::Selectable("Simplified Chinese")) {
                            cfg->SetSystemLanguage(Service::CFG::SystemLanguage::LANGUAGE_ZH);
                            update_config_savegame = true;
                        }

                        if (ImGui::Selectable("Korean")) {
                            cfg->SetSystemLanguage(Service::CFG::SystemLanguage::LANGUAGE_KO);
                            update_config_savegame = true;
                        }

                        if (ImGui::Selectable("Dutch")) {
                            cfg->SetSystemLanguage(Service::CFG::SystemLanguage::LANGUAGE_NL);
                            update_config_savegame = true;
                        }

                        if (ImGui::Selectable("Portugese")) {
                            cfg->SetSystemLanguage(Service::CFG::SystemLanguage::LANGUAGE_PT);
                            update_config_savegame = true;
                        }

                        if (ImGui::Selectable("Russian")) {
                            cfg->SetSystemLanguage(Service::CFG::SystemLanguage::LANGUAGE_RU);
                            update_config_savegame = true;
                        }

                        if (ImGui::Selectable("Traditional Chinese")) {
                            cfg->SetSystemLanguage(Service::CFG::SystemLanguage::LANGUAGE_TW);
                            update_config_savegame = true;
                        }

                        ImGui::EndCombo();
                    }

                    ImGui::Text("Play Coins");
                    ImGui::SameLine();

                    const u16 min = 0;
                    const u16 max = 300;

                    u16 play_coins = Service::PTM::Module::GetPlayCoins();
                    if (ImGui::SliderScalar("##playcoins", ImGuiDataType_U16, &play_coins, &min,
                                            &max)) {
                        Service::PTM::Module::SetPlayCoins(play_coins);
                    }

                    if (ImGui::Button("Regenerate Console ID")) {
                        u32 random_number;
                        u64 console_id;
                        cfg->GenerateConsoleUniqueId(random_number, console_id);
                        cfg->SetConsoleUniqueId(random_number, console_id);
                        update_config_savegame = true;
                    }

                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem("LLE Modules")) {
                    ImGui::Text("LLE Modules settings are not persistent.");
                    ImGui::NewLine();

                    for (auto& module : Settings::values.lle_modules) {
                        ImGui::Checkbox(module.first.c_str(), &module.second);
                    }

                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem("Multiplayer")) {
                    ImGui::Text("Multiplayer settings are not persistent.");
                    ImGui::NewLine();

                    ImGui::Text("Server URL");
                    ImGui::SameLine();
                    ImGui::InputText("##serverurl", &Settings::values.multiplayer_url);

                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem("Graphics")) {
                    ImGui::Text("Graphics settings are not persistent.");
                    ImGui::NewLine();

                    ImGui::Checkbox("Use Hardware Renderer", &Settings::values.use_hw_renderer);
                    ImGui::Checkbox("Use Hardware Shader", &Settings::values.use_hw_shader);
                    if (Settings::values.use_hw_shader) {
                        ImGui::Checkbox("Accurate Multiplication",
                                        &Settings::values.shaders_accurate_mul);
                    } else {
                        ImGui::Checkbox("Use Shader JIT", &Settings::values.use_shader_jit);
                    }
                    ImGui::Checkbox("Enable VSync", &Settings::values.enable_vsync);
                    ImGui::Checkbox("Dump Textures", &Settings::values.dump_textures);
                    ImGui::Checkbox("Use Custom Textures", &Settings::values.custom_textures);
                    ImGui::Checkbox("Preload Custom Textures", &Settings::values.preload_textures);
                    ImGui::Checkbox("Enable Linear Filtering", &Settings::values.filter_mode);
                    ImGui::Checkbox("Sharper Distant Objects",
                                    &Settings::values.sharper_distant_objects);

                    ImGui::Checkbox("Ignore Format Reinterpretation",
                                    &Settings::values.ignore_format_reinterpretation);
                    if (ImGui::IsItemHovered()) {
                        ImGui::BeginTooltip();
                        ImGui::Text("Ignore flushing surfaces from CPU memory if the surface was "
                                    "created by the GPU and has a different format.");
                        ImGui::Text("This can speed up many games, potentially break some, but is "
                                    "rightfully just a hack as a placeholder for GPU texture "
                                    "encoding/decoding");
                        ImGui::EndTooltip();
                    }

                    ImGui::Text("Resolution");
                    ImGui::SameLine();
                    const u16 min = 0;
                    const u16 max = 10;
                    ImGui::SliderScalar(
                        "##resolution", ImGuiDataType_U16, &Settings::values.resolution_factor,
                        &min, &max, Settings::values.resolution_factor == 0 ? "Window Size" : "%d");

                    ImGui::Text("Background Color");
                    ImGui::SameLine();
                    ImGui::ColorEdit3("##backgroundcolor", &Settings::values.bg_red,
                                      ImGuiColorEditFlags_NoInputs);

                    ImGui::Text("Post Processing Shader");
                    ImGui::SameLine();
                    if (ImGui::BeginCombo("##postprocessingshader",
                                          Settings::values.pp_shader_name.c_str())) {
                        const auto shaders = OpenGL::GetPostProcessingShaderList(
                            Settings::values.render_3d == Settings::StereoRenderOption::Anaglyph);

                        if (Settings::values.render_3d == Settings::StereoRenderOption::Anaglyph &&
                            ImGui::Selectable("dubois (builtin)")) {
                            Settings::values.pp_shader_name = "dubois (builtin)";
                        } else if (Settings::values.render_3d ==
                                       Settings::StereoRenderOption::Interlaced &&
                                   ImGui::Selectable("horizontal (builtin)")) {
                            Settings::values.pp_shader_name = "horizontal (builtin)";
                        } else if ((Settings::values.render_3d ==
                                        Settings::StereoRenderOption::Off ||
                                    Settings::values.render_3d ==
                                        Settings::StereoRenderOption::SideBySide) &&
                                   ImGui::Selectable("none (builtin)")) {
                            Settings::values.pp_shader_name = "none (builtin)";
                        }

                        for (const auto& shader : shaders) {
                            if (ImGui::Selectable(shader.c_str())) {
                                Settings::values.pp_shader_name = shader;
                            }
                        }

                        ImGui::EndCombo();
                    }

                    ImGui::Text("Texture Filter Name");
                    ImGui::SameLine();
                    if (ImGui::BeginCombo("##texturefiltername",
                                          Settings::values.texture_filter_name.c_str())) {
                        const auto& filters = OpenGL::TextureFilterManager::TextureFilterMap();

                        for (const auto& filter : filters) {
                            if (ImGui::Selectable(std::string(filter.first).c_str())) {
                                Settings::values.texture_filter_name = filter.first;
                            }
                        }

                        ImGui::EndCombo();
                    }

                    ImGui::Text("Texture Filter Factor");
                    ImGui::SameLine();
                    ImGui::InputScalar("##texturefilterfactor", ImGuiDataType_U16,
                                       &Settings::values.texture_filter_factor);

                    ImGui::Text("3D");
                    ImGui::SameLine();

                    if (ImGui::BeginCombo("##render_3d", [] {
                            switch (Settings::values.render_3d) {
                            case Settings::StereoRenderOption::Off:
                                return "Off";
                            case Settings::StereoRenderOption::SideBySide:
                                return "Side by Side";
                            case Settings::StereoRenderOption::Anaglyph:
                                return "Anaglyph";
                            case Settings::StereoRenderOption::Interlaced:
                                return "Interlaced";
                            default:
                                break;
                            }

                            return "Invalid value";
                        }())) {

                        if (ImGui::Selectable("Off", Settings::values.render_3d ==
                                                         Settings::StereoRenderOption::Off)) {
                            Settings::values.render_3d = Settings::StereoRenderOption::Off;
                            Settings::values.pp_shader_name = "none (builtin)";
                        }

                        if (ImGui::Selectable("Side by Side",
                                              Settings::values.render_3d ==
                                                  Settings::StereoRenderOption::SideBySide)) {
                            Settings::values.render_3d = Settings::StereoRenderOption::SideBySide;
                            Settings::values.pp_shader_name = "none (builtin)";
                        }

                        if (ImGui::Selectable("Anaglyph",
                                              Settings::values.render_3d ==
                                                  Settings::StereoRenderOption::Anaglyph)) {
                            Settings::values.render_3d = Settings::StereoRenderOption::Anaglyph;
                            Settings::values.pp_shader_name = "dubois (builtin)";
                        }

                        if (ImGui::Selectable("Interlaced",
                                              Settings::values.render_3d ==
                                                  Settings::StereoRenderOption::Interlaced)) {
                            Settings::values.render_3d = Settings::StereoRenderOption::Interlaced;
                            Settings::values.pp_shader_name = "horizontal (builtin)";
                        }

                        ImGui::EndCombo();
                    }

                    ImGui::SameLine();

                    u8 factor_3d = Settings::values.factor_3d;
                    const u8 factor_3d_min = 0;
                    const u8 factor_3d_max = 100;
                    ImGui::PushItemWidth(100);
                    if (ImGui::SliderScalar("##factor_3d", ImGuiDataType_U8, &factor_3d,
                                            &factor_3d_min, &factor_3d_max, "%d%%")) {
                        Settings::values.factor_3d = factor_3d;
                    }
                    ImGui::PopItemWidth();

                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem("Controls")) {
                    ImGui::Text("Controls settings are not persistent.");
                    ImGui::NewLine();

                    const auto SetMapping = [&](const auto native_id,
                                                InputCommon::Polling::DeviceType device_type) {
                        switch (device_type) {
                        case InputCommon::Polling::DeviceType::Button: {
                            auto pollers = InputCommon::Polling::GetPollers(device_type);

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
                                        Settings::values.buttons[native_id] = params.Serialize();
                                        return;
                                    }
                                }

                                while (SDL_PollEvent(&event)) {
                                    if (event.type == SDL_KEYUP) {
                                        Settings::values.buttons[native_id] =
                                            InputCommon::GenerateKeyboardParam(
                                                event.key.keysym.scancode);
                                        for (auto& poller : pollers) {
                                            poller->Stop();
                                        }
                                        return;
                                    }
                                }
                            }

                            break;
                        }
                        case InputCommon::Polling::DeviceType::Analog: {
                            auto pollers = InputCommon::Polling::GetPollers(device_type);

                            for (auto& poller : pollers) {
                                poller->Start();
                            }

                            std::vector<int> keyboard_scancodes;

                            for (;;) {
                                for (auto& poller : pollers) {
                                    const Common::ParamPackage params = poller->GetNextInput();
                                    if (params.Has("engine")) {
                                        for (auto& poller : pollers) {
                                            poller->Stop();
                                        }
                                        Settings::values.analogs[native_id] = params.Serialize();
                                        return;
                                    }
                                }

                                while (SDL_PollEvent(&event)) {
                                    if (event.type == SDL_KEYUP) {
                                        pollers.clear();
                                        keyboard_scancodes.push_back(event.key.keysym.scancode);
                                        if (keyboard_scancodes.size() == 5) {
                                            Settings::values.analogs[native_id] =
                                                InputCommon::GenerateAnalogParamFromKeys(
                                                    keyboard_scancodes[0], keyboard_scancodes[1],
                                                    keyboard_scancodes[2], keyboard_scancodes[3],
                                                    keyboard_scancodes[4], 0.5f);
                                            for (auto& poller : pollers) {
                                                poller->Stop();
                                            }
                                            return;
                                        }
                                    }
                                }
                            }

                            break;
                        }
                        default: {
                            return;
                        }
                        }
                    };

                    const auto ButtonToText = [](const std::string& params_string) {
                        const Common::ParamPackage params(params_string);

                        if (params.Get("engine", "") == "keyboard") {
                            return std::string(SDL_GetScancodeName(
                                static_cast<SDL_Scancode>(params.Get("code", SDL_SCANCODE_A))));
                        }

                        if (params.Get("engine", "") == "sdl") {
                            if (params.Has("hat")) {
                                return fmt::format("Hat {} {}", params.Get("hat", ""),
                                                   params.Get("direction", ""));
                            }

                            if (params.Has("axis")) {
                                return fmt::format("Axis {}{}", params.Get("axis", ""),
                                                   params.Get("direction", ""));
                            }

                            if (params.Has("button")) {
                                return fmt::format("Button {}", params.Get("button", ""));
                            }
                        }

                        return std::string("[unknown]");
                    };

                    const auto AnalogToText = [&](const std::string& params_string,
                                                  const std::string& dir) {
                        const Common::ParamPackage params(params_string);

                        if (params.Get("engine", "") == "analog_from_button") {
                            return ButtonToText(params.Get(dir, ""));
                        }

                        if (params.Get("engine", "") == "sdl") {
                            if (dir == "modifier") {
                                return std::string("[unused]");
                            }

                            if (dir == "left" || dir == "right") {
                                return fmt::format("Axis {}", params.Get("axis_x", ""));
                            }

                            if (dir == "up" || dir == "down") {
                                return fmt::format("Axis {}", params.Get("axis_y", ""));
                            }
                        }

                        return std::string("[unknown]");
                    };

                    // Buttons
                    ImGui::BeginGroup();
                    ImGui::Text("Buttons");
                    ImGui::NewLine();

                    ImGui::Text("A:");
                    ImGui::SameLine();
                    if (ImGui::Button(
                            (ButtonToText(Settings::values.buttons[Settings::NativeButton::A]) +
                             "##ButtonA")
                                .c_str())) {
                        SetMapping(Settings::NativeButton::A,
                                   InputCommon::Polling::DeviceType::Button);
                    }

                    ImGui::Text("B:");
                    ImGui::SameLine();
                    if (ImGui::Button(
                            (ButtonToText(Settings::values.buttons[Settings::NativeButton::B]) +
                             "##ButtonB")
                                .c_str())) {
                        SetMapping(Settings::NativeButton::B,
                                   InputCommon::Polling::DeviceType::Button);
                    }

                    ImGui::Text("X:");
                    ImGui::SameLine();
                    if (ImGui::Button(
                            (ButtonToText(Settings::values.buttons[Settings::NativeButton::X]) +
                             "##ButtonX")
                                .c_str())) {
                        SetMapping(Settings::NativeButton::X,
                                   InputCommon::Polling::DeviceType::Button);
                    }

                    ImGui::Text("Y:");
                    ImGui::SameLine();
                    if (ImGui::Button(
                            (ButtonToText(Settings::values.buttons[Settings::NativeButton::Y]) +
                             "##ButtonY")
                                .c_str())) {
                        SetMapping(Settings::NativeButton::Y,
                                   InputCommon::Polling::DeviceType::Button);
                    }

                    ImGui::Text("L:");
                    ImGui::SameLine();
                    if (ImGui::Button(
                            (ButtonToText(Settings::values.buttons[Settings::NativeButton::L]) +
                             "##ButtonL")
                                .c_str())) {
                        SetMapping(Settings::NativeButton::L,
                                   InputCommon::Polling::DeviceType::Button);
                    }

                    ImGui::Text("R:");
                    ImGui::SameLine();
                    if (ImGui::Button(
                            (ButtonToText(Settings::values.buttons[Settings::NativeButton::R]) +
                             "##ButtonR")
                                .c_str())) {
                        SetMapping(Settings::NativeButton::R,
                                   InputCommon::Polling::DeviceType::Button);
                    }

                    ImGui::Text("ZL:");
                    ImGui::SameLine();
                    if (ImGui::Button(
                            (ButtonToText(Settings::values.buttons[Settings::NativeButton::ZL]) +
                             "##ButtonZL")
                                .c_str())) {
                        SetMapping(Settings::NativeButton::ZL,
                                   InputCommon::Polling::DeviceType::Button);
                    }

                    ImGui::Text("ZR:");
                    ImGui::SameLine();
                    if (ImGui::Button(
                            (ButtonToText(Settings::values.buttons[Settings::NativeButton::ZR]) +
                             "##ButtonZR")
                                .c_str())) {
                        SetMapping(Settings::NativeButton::ZR,
                                   InputCommon::Polling::DeviceType::Button);
                    }

                    ImGui::Text("Start:");
                    ImGui::SameLine();
                    if (ImGui::Button(
                            (ButtonToText(Settings::values.buttons[Settings::NativeButton::Start]) +
                             "##ButtonStart")
                                .c_str())) {
                        SetMapping(Settings::NativeButton::Start,
                                   InputCommon::Polling::DeviceType::Button);
                    }

                    ImGui::Text("Select:");
                    ImGui::SameLine();
                    if (ImGui::Button(
                            (ButtonToText(
                                 Settings::values.buttons[Settings::NativeButton::Select]) +
                             "##ButtonSelect")
                                .c_str())) {
                        SetMapping(Settings::NativeButton::Select,
                                   InputCommon::Polling::DeviceType::Button);
                    }

                    ImGui::Text("Debug:");
                    ImGui::SameLine();
                    if (ImGui::Button(
                            (ButtonToText(Settings::values.buttons[Settings::NativeButton::Debug]) +
                             "##ButtonDebug")
                                .c_str())) {
                        SetMapping(Settings::NativeButton::Debug,
                                   InputCommon::Polling::DeviceType::Button);
                    }

                    ImGui::Text("GPIO14:");
                    ImGui::SameLine();
                    if (ImGui::Button(
                            (ButtonToText(
                                 Settings::values.buttons[Settings::NativeButton::Gpio14]) +
                             "##ButtonGPIO14")
                                .c_str())) {
                        SetMapping(Settings::NativeButton::Gpio14,
                                   InputCommon::Polling::DeviceType::Button);
                    }
                    ImGui::EndGroup();

                    ImGui::SameLine();

                    // Circle Pad
                    ImGui::BeginGroup();
                    ImGui::Text("Circle Pad");
                    ImGui::NewLine();
                    ImGui::Text(
                        "Up: %s",
                        AnalogToText(Settings::values.analogs[Settings::NativeAnalog::CirclePad],
                                     "up")
                            .c_str());
                    ImGui::Text(
                        "Down: %s",
                        AnalogToText(Settings::values.analogs[Settings::NativeAnalog::CirclePad],
                                     "down")
                            .c_str());
                    ImGui::Text(
                        "Left: %s",
                        AnalogToText(Settings::values.analogs[Settings::NativeAnalog::CirclePad],
                                     "left")
                            .c_str());
                    ImGui::Text(
                        "Right: %s",
                        AnalogToText(Settings::values.analogs[Settings::NativeAnalog::CirclePad],
                                     "right")
                            .c_str());
                    ImGui::Text(
                        "Modifier: %s",
                        AnalogToText(Settings::values.analogs[Settings::NativeAnalog::CirclePad],
                                     "modifier")
                            .c_str());
                    if (ImGui::Button("Set All##circlepad")) {
                        SetMapping(Settings::NativeAnalog::CirclePad,
                                   InputCommon::Polling::DeviceType::Analog);
                    }
                    if (ImGui::IsItemHovered()) {
                        ImGui::SetTooltip("Keyboard: Press the keys to use for Up, Down, "
                                          "Left, Right, and Modifier.\nReal stick: first move "
                                          "the stick to the right, and then to the bottom.");
                    }
                    ImGui::EndGroup();

                    ImGui::SameLine();

                    // C-Stick
                    ImGui::BeginGroup();
                    ImGui::Text("C-Stick");
                    ImGui::NewLine();
                    ImGui::Text(
                        "Up: %s",
                        AnalogToText(Settings::values.analogs[Settings::NativeAnalog::CStick], "up")
                            .c_str());
                    ImGui::Text(
                        "Down: %s",
                        AnalogToText(Settings::values.analogs[Settings::NativeAnalog::CStick],
                                     "down")
                            .c_str());
                    ImGui::Text(
                        "Left: %s",
                        AnalogToText(Settings::values.analogs[Settings::NativeAnalog::CStick],
                                     "left")
                            .c_str());
                    ImGui::Text(
                        "Right: %s",
                        AnalogToText(Settings::values.analogs[Settings::NativeAnalog::CStick],
                                     "right")
                            .c_str());
                    ImGui::Text(
                        "Modifier: %s",
                        AnalogToText(Settings::values.analogs[Settings::NativeAnalog::CStick],
                                     "modifier")
                            .c_str());
                    if (ImGui::Button("Set All##cstick")) {
                        SetMapping(Settings::NativeAnalog::CStick,
                                   InputCommon::Polling::DeviceType::Analog);
                    }
                    if (ImGui::IsItemHovered()) {
                        ImGui::SetTooltip("Keyboard: Press the keys to use for Up, Down, "
                                          "Left, Right, and Modifier.\nReal stick: first move "
                                          "the stick to the right, and then to the bottom.");
                    }
                    ImGui::EndGroup();

                    ImGui::SameLine();

                    // D-Pad
                    ImGui::BeginGroup();
                    ImGui::Text("D-Pad");
                    ImGui::NewLine();
                    ImGui::Text("Up:");
                    ImGui::SameLine();
                    if (ImGui::Button(
                            (ButtonToText(Settings::values.buttons[Settings::NativeButton::Up]) +
                             "##DPadUp")
                                .c_str())) {
                        SetMapping(Settings::NativeButton::Up,
                                   InputCommon::Polling::DeviceType::Button);
                    }
                    ImGui::Text("Down:");
                    ImGui::SameLine();
                    if (ImGui::Button(
                            (ButtonToText(Settings::values.buttons[Settings::NativeButton::Down]) +
                             "##DPadDown")
                                .c_str())) {
                        SetMapping(Settings::NativeButton::Down,
                                   InputCommon::Polling::DeviceType::Button);
                    }
                    ImGui::Text("Left:");
                    ImGui::SameLine();
                    if (ImGui::Button(
                            (ButtonToText(Settings::values.buttons[Settings::NativeButton::Left]) +
                             "##DPadLeft")
                                .c_str())) {
                        SetMapping(Settings::NativeButton::Left,
                                   InputCommon::Polling::DeviceType::Button);
                    }
                    ImGui::Text("Right:");
                    ImGui::SameLine();
                    if (ImGui::Button(
                            (ButtonToText(Settings::values.buttons[Settings::NativeButton::Right]) +
                             "##DPadRight")
                                .c_str())) {
                        SetMapping(Settings::NativeButton::Right,
                                   InputCommon::Polling::DeviceType::Button);
                    }
                    ImGui::EndGroup();

                    ImGui::SameLine();

                    // Hotkeys
                    ImGui::BeginGroup();
                    ImGui::Text("Hotkeys");
                    ImGui::NewLine();
                    ImGui::Text("Copy Screenshot: CTRL + C");
                    ImGui::Text("Load File: CTRL + L");
                    ImGui::Text("Restart Emulation: CTRL + R");
                    ImGui::Text("Toggle Limit Speed: CTRL + Z");
                    ImGui::Text("Toggle Dump Textures: CTRL + D");
                    ImGui::Text("Speed Limit - 5: -");
                    ImGui::Text("Speed Limit + 5: +");
                    ImGui::Text("Load Amiibo: F1");
                    ImGui::Text("Remove Amiibo: F2");
                    ImGui::Text("Toggle Fullscreen: F11");
                    ImGui::Text("Window Size Resolution: CTRL + A");
                    ImGui::Text("1x Resolution: CTRL + 1");
                    ImGui::Text("2x Resolution: CTRL + 2");
                    ImGui::Text("3x Resolution: CTRL + 3");
                    ImGui::Text("4x Resolution: CTRL + 4");
                    ImGui::Text("5x Resolution: CTRL + 5");
                    ImGui::Text("6x Resolution: CTRL + 6");
                    ImGui::Text("7x Resolution: CTRL + 7");
                    ImGui::Text("8x Resolution: CTRL + 8");
                    ImGui::Text("9x Resolution: CTRL + 9");
                    ImGui::Text("10x Resolution: CTRL + 0");
                    ImGui::EndGroup();

                    ImGui::NewLine();

                    ImGui::Text("Motion:");
                    ImGui::SameLine();
                    if (ImGui::BeginCombo("##motion_device", [] {
                            const std::string engine =
                                Common::ParamPackage(Settings::values.motion_device)
                                    .Get("engine", "");

                            if (engine == "motion_emu") {
                                return "Right Click";
                            } else if (engine == "cemuhookudp") {
                                return "UDP";
                            }

                            return "Invalid";
                        }())) {
                        if (ImGui::Selectable("Right Click")) {
                            Settings::values.motion_device = "engine:motion_emu";
                        }

                        if (ImGui::Selectable("UDP")) {
                            Settings::values.motion_device = "engine:cemuhookudp";
                        }

                        ImGui::EndCombo();
                    }

                    Common::ParamPackage motion_device(Settings::values.motion_device);

                    if (motion_device.Get("engine", "") == "motion_emu") {
                        int update_period = motion_device.Get("update_period", 100);
                        float sensitivity = motion_device.Get("sensitivity", 0.01f);
                        float tilt_clamp = motion_device.Get("tilt_clamp", 90.0f);

                        ImGui::Text("Motion Update Period:");
                        ImGui::SameLine();
                        if (ImGui::InputInt("##update_period", &update_period)) {
                            motion_device.Set("update_period", update_period);
                            Settings::values.motion_device = motion_device.Serialize();
                        }

                        ImGui::Text("Motion Sensitivity:");
                        ImGui::SameLine();
                        if (ImGui::InputFloat("##sensitivity", &sensitivity)) {
                            motion_device.Set("sensitivity", sensitivity);
                            Settings::values.motion_device = motion_device.Serialize();
                        }

                        ImGui::Text("Motion Tilt Clamp:");
                        ImGui::SameLine();
                        if (ImGui::InputFloat("##tilt_clamp", &tilt_clamp)) {
                            motion_device.Set("tilt_clamp", update_period);
                            Settings::values.motion_device = motion_device.Serialize();
                        }
                    }

                    ImGui::Text("Touch:");
                    ImGui::SameLine();
                    if (ImGui::BeginCombo("##touch_device", [] {
                            const std::string engine =
                                Common::ParamPackage(Settings::values.touch_device)
                                    .Get("engine", "");

                            if (engine == "emu_window") {
                                return "Mouse";
                            } else if (engine == "cemuhookudp") {
                                return "UDP";
                            }

                            return "Invalid";
                        }())) {
                        if (ImGui::Selectable("Mouse")) {
                            Settings::values.touch_device = "engine:emu_window";
                        }

                        if (ImGui::Selectable("UDP")) {
                            Settings::values.touch_device = "engine:cemuhookudp";
                        }

                        ImGui::EndCombo();
                    }

                    Common::ParamPackage touch_device(Settings::values.touch_device);

                    if (touch_device.Get("engine", "") == "cemuhookudp") {
                        int min_x = touch_device.Get("min_x", 100);
                        int min_y = touch_device.Get("min_y", 50);
                        int max_x = touch_device.Get("max_x", 1800);
                        int max_y = touch_device.Get("max_y", 850);

                        ImGui::Text("Touch Minimum X:");
                        ImGui::SameLine();
                        if (ImGui::InputInt("##min_x", &min_x)) {
                            touch_device.Set("min_x", min_x);
                            Settings::values.touch_device = touch_device.Serialize();
                        }

                        ImGui::Text("Touch Minimum Y:");
                        ImGui::SameLine();
                        if (ImGui::InputInt("##min_y", &min_y)) {
                            touch_device.Set("min_y", min_y);
                            Settings::values.touch_device = touch_device.Serialize();
                        }

                        ImGui::Text("Touch Maximum X:");
                        ImGui::SameLine();
                        if (ImGui::InputInt("##max_x", &max_x)) {
                            touch_device.Set("max_x", max_x);
                            Settings::values.touch_device = touch_device.Serialize();
                        }

                        ImGui::Text("Touch Maximum Y:");
                        ImGui::SameLine();
                        if (ImGui::InputInt("##max_y", &max_y)) {
                            touch_device.Set("max_y", max_y);
                            Settings::values.touch_device = touch_device.Serialize();
                        }
                    }

                    ImGui::Text("UDP Input Address:");
                    ImGui::SameLine();
                    ImGui::InputText("##udp_input_address", &Settings::values.udp_input_address);

                    ImGui::Text("UDP Input Port:");
                    ImGui::SameLine();
                    ImGui::InputScalar("##udp_input_port", ImGuiDataType_U16,
                                       &Settings::values.udp_input_port);

                    ImGui::Text("UDP Pad Index:");
                    ImGui::SameLine();
                    ImGui::InputScalar("##udp_pad_index", ImGuiDataType_U8,
                                       &Settings::values.udp_pad_index);

                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem("Layout")) {
                    if (!Settings::values.custom_layout) {
                        ImGui::Text("Layout:");
                        ImGui::SameLine();
                        if (ImGui::BeginCombo("##layout", [] {
                                switch (Settings::values.layout_option) {
                                case Settings::LayoutOption::Default:
                                    return "Default";
                                case Settings::LayoutOption::SingleScreen:
                                    return "Single Screen";
                                case Settings::LayoutOption::LargeScreen:
                                    return "Large Screen";
                                case Settings::LayoutOption::SideScreen:
                                    return "Side by Side";
                                case Settings::LayoutOption::MediumScreen:
                                    return "Medium Screen";
                                default:
                                    break;
                                }

                                return "Invalid";
                            }())) {
                            if (ImGui::Selectable("Default")) {
                                Settings::values.layout_option = Settings::LayoutOption::Default;
                            }
                            if (ImGui::Selectable("Single Screen")) {
                                Settings::values.layout_option =
                                    Settings::LayoutOption::SingleScreen;
                            }
                            if (ImGui::Selectable("Large Screen")) {
                                Settings::values.layout_option =
                                    Settings::LayoutOption::LargeScreen;
                            }
                            if (ImGui::Selectable("Side by Side")) {
                                Settings::values.layout_option = Settings::LayoutOption::SideScreen;
                            }
                            if (ImGui::Selectable("Medium Screen")) {
                                Settings::values.layout_option =
                                    Settings::LayoutOption::MediumScreen;
                            }
                            ImGui::EndCombo();
                        }
                    }

                    ImGui::Checkbox("Use Custom Layout", &Settings::values.custom_layout);
                    ImGui::Checkbox("Swap Screens", &Settings::values.swap_screen);
                    ImGui::Checkbox("Upright Orientation", &Settings::values.upright_screen);

                    if (Settings::values.custom_layout) {
                        ImGui::Text("Top Left:");
                        ImGui::SameLine();
                        ImGui::InputScalar("##topleft", ImGuiDataType_U16,
                                           &Settings::values.custom_top_left);
                        ImGui::Text("Top Top:");
                        ImGui::SameLine();
                        ImGui::InputScalar("##toptop", ImGuiDataType_U16,
                                           &Settings::values.custom_top_top);
                        ImGui::Text("Top Right:");
                        ImGui::SameLine();
                        ImGui::InputScalar("##topright", ImGuiDataType_U16,
                                           &Settings::values.custom_top_right);
                        ImGui::Text("Top Bottom:");
                        ImGui::SameLine();
                        ImGui::InputScalar("##topbottom", ImGuiDataType_U16,
                                           &Settings::values.custom_top_bottom);
                        ImGui::Text("Bottom Left:");
                        ImGui::SameLine();
                        ImGui::InputScalar("##bottomleft", ImGuiDataType_U16,
                                           &Settings::values.custom_bottom_left);
                        ImGui::Text("Bottom Top:");
                        ImGui::SameLine();
                        ImGui::InputScalar("##bottomtop", ImGuiDataType_U16,
                                           &Settings::values.custom_bottom_top);
                        ImGui::Text("Bottom Right:");
                        ImGui::SameLine();
                        ImGui::InputScalar("##bottomright", ImGuiDataType_U16,
                                           &Settings::values.custom_bottom_right);
                        ImGui::Text("Bottom Bottom:");
                        ImGui::SameLine();
                        ImGui::InputScalar("##bottombottom", ImGuiDataType_U16,
                                           &Settings::values.custom_bottom_bottom);
                    }

                    ImGui::EndTabItem();
                }

                ImGui::EndTabBar();
            }

            // OK
            if (!Settings::values.file_path.empty()) {
                ImGui::NewLine();
                if (ImGui::Button("OK")) {
                    Settings::Apply();
                    if (update_config_savegame) {
                        cfg->UpdateConfigNANDSavegame();
                    }
                    return;
                }
            }
        }

        ImGui::End();
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        SDL_GL_SwapWindow(render_window);
    }
}

Configuration::~Configuration() {
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
    SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(render_window);
}

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
#include <json.hpp>
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
#include "video_core/renderer_opengl/texture_filters/texture_filterer.h"
#include "vvctre/configuration.h"

Configuration::Configuration() {
    const std::string window_title =
        fmt::format("vvctre {} Configuration", version::vvctre.to_string());

    render_window = SDL_CreateWindow(
        window_title.c_str(),
        SDL_WINDOWPOS_UNDEFINED, // x position
        SDL_WINDOWPOS_UNDEFINED, // y position
        640, 480, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);

    if (render_window == nullptr) {
        LOG_CRITICAL(Frontend, "Failed to create SDL2 window: {}", SDL_GetError());
        std::exit(1);
    }

    SDL_SetWindowMinimumSize(render_window, 640, 480);

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
    std::shared_ptr<Service::CFG::Module> cfg = std::make_shared<Service::CFG::Module>();

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

                    ImGui::Text("Region:");
                    ImGui::SameLine();
                    if (ImGui::BeginCombo("##region", [&] {
                            switch (Settings::values.region_value) {
                            case -1:
                                return "Auto-select";
                            case 0:
                                return "Japan";
                            case 1:
                                return "USA";
                            case 2:
                                return "Europe";
                            case 3:
                                return "Australia";
                            case 4:
                                return "China";
                            case 5:
                                return "Korea";
                            case 6:
                                return "Taiwan";
                            }

                            return "Invalid";
                        }())) {
                        if (ImGui::Selectable("Auto-select")) {
                            Settings::values.region_value = Settings::REGION_VALUE_AUTO_SELECT;
                        }
                        if (ImGui::Selectable("Japan")) {
                            Settings::values.region_value = 0;
                        }
                        if (ImGui::Selectable("USA")) {
                            Settings::values.region_value = 1;
                        }
                        if (ImGui::Selectable("Europe")) {
                            Settings::values.region_value = 2;
                        }
                        if (ImGui::Selectable("Australia")) {
                            Settings::values.region_value = 3;
                        }
                        if (ImGui::Selectable("China")) {
                            Settings::values.region_value = 4;
                        }
                        if (ImGui::Selectable("Korea")) {
                            Settings::values.region_value = 5;
                        }
                        if (ImGui::Selectable("Taiwan")) {
                            Settings::values.region_value = 6;
                        }
                        ImGui::EndCombo();
                    }

                    ImGui::Text("Log Filter:");
                    ImGui::SameLine();
                    ImGui::InputText("##logfilter", &Settings::values.log_filter);

                    ImGui::Text("RPC Server Port:");
                    ImGui::SameLine();
                    ImGui::InputInt("##rpcserverport", &Settings::values.rpc_server_port);

                    ImGui::Text("Multiplayer Server URL:");
                    ImGui::SameLine();
                    ImGui::InputText("##multiplayerserverurl", &Settings::values.multiplayer_url);

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
                        if (ImGui::Selectable("image (configuration: file path or URL)")) {
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
                        if (ImGui::Selectable("image (configuration: file path or URL)")) {
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
                        if (ImGui::Selectable("image (configuration: file path or URL)")) {
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

                    ImGui::Text("Username:");
                    ImGui::SameLine();

                    std::string username = Common::UTF16ToUTF8(cfg->GetUsername());
                    if (ImGui::InputText("##username", &username)) {
                        cfg->SetUsername(Common::UTF8ToUTF16(username));
                        cfg->UpdateConfigNANDSavegame();
                    }

                    ImGui::Text("Birthday:");
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

                    ImGui::Text("Language:");
                    ImGui::SameLine();

                    if (ImGui::BeginCombo("##language", [&] {
                            switch (cfg->GetSystemLanguage()) {
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

                    ImGui::Text("Sound output mode:");
                    ImGui::SameLine();
                    if (ImGui::BeginCombo("##soundoutputmode", [&] {
                            switch (cfg->GetSoundOutputMode()) {
                            case Service::CFG::SoundOutputMode::SOUND_MONO:
                                return "Mono";
                            case Service::CFG::SoundOutputMode::SOUND_STEREO:
                                return "Stereo";
                            case Service::CFG::SoundOutputMode::SOUND_SURROUND:
                                return "Surround";
                            default:
                                break;
                            }

                            return "Invalid";
                        }())) {
                        if (ImGui::Selectable("Mono")) {
                            cfg->SetSoundOutputMode(Service::CFG::SoundOutputMode::SOUND_MONO);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Stereo")) {
                            cfg->SetSoundOutputMode(Service::CFG::SoundOutputMode::SOUND_STEREO);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Surround")) {
                            cfg->SetSoundOutputMode(Service::CFG::SoundOutputMode::SOUND_SURROUND);
                            update_config_savegame = true;
                        }
                        ImGui::EndCombo();
                    }

                    ImGui::Text("Country:");
                    ImGui::SameLine();
                    if (ImGui::BeginCombo("##country", [&] {
                            switch (cfg->GetCountryCode()) {
                            case 1:
                                return "Japan";
                            case 8:
                                return "Anguilla";
                            case 9:
                                return "Antigua and Barbuda";
                            case 10:
                                return "Argentina";
                            case 11:
                                return "Aruba";
                            case 12:
                                return "Bahamas";
                            case 13:
                                return "Barbados";
                            case 14:
                                return "Belize";
                            case 15:
                                return "Bolivia";
                            case 16:
                                return "Brazil";
                            case 17:
                                return "British Virgin Islands";
                            case 18:
                                return "Canada";
                            case 19:
                                return "Cayman Islands";
                            case 20:
                                return "Chile";
                            case 21:
                                return "Colombia";
                            case 22:
                                return "Costa Rica";
                            case 23:
                                return "Dominica";
                            case 24:
                                return "Dominican Republic";
                            case 25:
                                return "Ecuador";
                            case 26:
                                return "El Salvador";
                            case 27:
                                return "French Guiana";
                            case 28:
                                return "Grenada";
                            case 29:
                                return "Guadeloupe";
                            case 30:
                                return "Guatemala";
                            case 31:
                                return "Guyana";
                            case 32:
                                return "Haiti";
                            case 33:
                                return "Honduras";
                            case 34:
                                return "Jamaica";
                            case 35:
                                return "Martinique";
                            case 36:
                                return "Mexico";
                            case 37:
                                return "Montserrat";
                            case 38:
                                return "Netherlands Antilles";
                            case 39:
                                return "Nicaragua";
                            case 40:
                                return "Panama";
                            case 41:
                                return "Paraguay";
                            case 42:
                                return "Peru";
                            case 43:
                                return "Saint Kitts and Nevis";
                            case 44:
                                return "Saint Lucia";
                            case 45:
                                return "Saint Vincent and the Grenadines";
                            case 46:
                                return "Suriname";
                            case 47:
                                return "Trinidad and Tobago";
                            case 48:
                                return "Turks and Caicos Islands";
                            case 49:
                                return "United States";
                            case 50:
                                return "Uruguay";
                            case 51:
                                return "US Virgin Islands";
                            case 52:
                                return "Venezuela";
                            case 64:
                                return "Albania";
                            case 65:
                                return "Australia";
                            case 66:
                                return "Austria";
                            case 67:
                                return "Belgium";
                            case 68:
                                return "Bosnia and Herzegovina";
                            case 69:
                                return "Botswana";
                            case 70:
                                return "Bulgaria";
                            case 71:
                                return "Croatia";
                            case 72:
                                return "Cyprus";
                            case 73:
                                return "Czech Republic";
                            case 74:
                                return "Denmark";
                            case 75:
                                return "Estonia";
                            case 76:
                                return "Finland";
                            case 77:
                                return "France";
                            case 78:
                                return "Germany";
                            case 79:
                                return "Greece";
                            case 80:
                                return "Hungary";
                            case 81:
                                return "Iceland";
                            case 82:
                                return "Ireland";
                            case 83:
                                return "Italy";
                            case 84:
                                return "Latvia";
                            case 85:
                                return "Lesotho";
                            case 86:
                                return "Liechtenstein";
                            case 87:
                                return "Lithuania";
                            case 88:
                                return "Luxembourg";
                            case 89:
                                return "Macedonia";
                            case 90:
                                return "Malta";
                            case 91:
                                return "Montenegro";
                            case 92:
                                return "Mozambique";
                            case 93:
                                return "Namibia";
                            case 94:
                                return "Netherlands";
                            case 95:
                                return "New Zealand";
                            case 96:
                                return "Norway";
                            case 97:
                                return "Poland";
                            case 98:
                                return "Portugal";
                            case 99:
                                return "Romania";
                            case 100:
                                return "Russia";
                            case 101:
                                return "Serbia";
                            case 102:
                                return "Slovakia";
                            case 103:
                                return "Slovenia";
                            case 104:
                                return "South Africa";
                            case 105:
                                return "Spain";
                            case 106:
                                return "Swaziland";
                            case 107:
                                return "Sweden";
                            case 108:
                                return "Switzerland";
                            case 109:
                                return "Turkey";
                            case 110:
                                return "United Kingdom";
                            case 111:
                                return "Zambia";
                            case 112:
                                return "Zimbabwe";
                            case 113:
                                return "Azerbaijan";
                            case 114:
                                return "Mauritania";
                            case 115:
                                return "Mali";
                            case 116:
                                return "Niger";
                            case 117:
                                return "Chad";
                            case 118:
                                return "Sudan";
                            case 119:
                                return "Eritrea";
                            case 120:
                                return "Djibouti";
                            case 121:
                                return "Somalia";
                            case 122:
                                return "Andorra";
                            case 123:
                                return "Gibraltar";
                            case 124:
                                return "Guernsey";
                            case 125:
                                return "Isle of Man";
                            case 126:
                                return "Jersey";
                            case 127:
                                return "Monaco";
                            case 128:
                                return "Taiwan";
                            case 136:
                                return "South Korea";
                            case 144:
                                return "Hong Kong";
                            case 145:
                                return "Macau";
                            case 152:
                                return "Indonesia";
                            case 153:
                                return "Singapore";
                            case 154:
                                return "Thailand";
                            case 155:
                                return "Philippines";
                            case 156:
                                return "Malaysia";
                            case 160:
                                return "China";
                            case 168:
                                return "United Arab Emirates";
                            case 169:
                                return "India";
                            case 170:
                                return "Egypt";
                            case 171:
                                return "Oman";
                            case 172:
                                return "Qatar";
                            case 173:
                                return "Kuwait";
                            case 174:
                                return "Saudi Arabia";
                            case 175:
                                return "Syria";
                            case 176:
                                return "Bahrain";
                            case 177:
                                return "Jordan";
                            case 184:
                                return "San Marino";
                            case 185:
                                return "Vatican City";
                            case 186:
                                return "Bermuda";
                            default:
                                break;
                            }

                            return "Invalid";
                        }())) {
                        if (ImGui::Selectable("Japan")) {
                            cfg->SetCountryCode(1);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Anguilla")) {
                            cfg->SetCountryCode(8);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Antigua and Barbuda")) {
                            cfg->SetCountryCode(9);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Argentina")) {
                            cfg->SetCountryCode(10);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Aruba")) {
                            cfg->SetCountryCode(11);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Bahamas")) {
                            cfg->SetCountryCode(12);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Barbados")) {
                            cfg->SetCountryCode(13);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Belize")) {
                            cfg->SetCountryCode(14);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Bolivia")) {
                            cfg->SetCountryCode(15);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Brazil")) {
                            cfg->SetCountryCode(16);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("British Virgin Islands")) {
                            cfg->SetCountryCode(17);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Canada")) {
                            cfg->SetCountryCode(18);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Cayman Islands")) {
                            cfg->SetCountryCode(19);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Chile")) {
                            cfg->SetCountryCode(20);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Colombia")) {
                            cfg->SetCountryCode(21);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Costa Rica")) {
                            cfg->SetCountryCode(22);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Dominica")) {
                            cfg->SetCountryCode(23);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Dominican Republic")) {
                            cfg->SetCountryCode(24);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Ecuador")) {
                            cfg->SetCountryCode(25);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("El Salvador")) {
                            cfg->SetCountryCode(26);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("French Guiana")) {
                            cfg->SetCountryCode(27);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Grenada")) {
                            cfg->SetCountryCode(28);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Guadeloupe")) {
                            cfg->SetCountryCode(29);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Guatemala")) {
                            cfg->SetCountryCode(30);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Guyana")) {
                            cfg->SetCountryCode(31);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Haiti")) {
                            cfg->SetCountryCode(32);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Honduras")) {
                            cfg->SetCountryCode(33);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Jamaica")) {
                            cfg->SetCountryCode(34);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Martinique")) {
                            cfg->SetCountryCode(35);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Mexico")) {
                            cfg->SetCountryCode(36);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Montserrat")) {
                            cfg->SetCountryCode(37);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Netherlands Antilles")) {
                            cfg->SetCountryCode(38);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Nicaragua")) {
                            cfg->SetCountryCode(39);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Panama")) {
                            cfg->SetCountryCode(40);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Paraguay")) {
                            cfg->SetCountryCode(41);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Peru")) {
                            cfg->SetCountryCode(42);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Saint Kitts and Nevis")) {
                            cfg->SetCountryCode(43);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Saint Lucia")) {
                            cfg->SetCountryCode(44);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Saint Vincent and the Grenadines")) {
                            cfg->SetCountryCode(45);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Suriname")) {
                            cfg->SetCountryCode(46);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Trinidad and Tobago")) {
                            cfg->SetCountryCode(47);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Turks and Caicos Islands")) {
                            cfg->SetCountryCode(48);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("United States")) {
                            cfg->SetCountryCode(49);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Uruguay")) {
                            cfg->SetCountryCode(50);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("US Virgin Islands")) {
                            cfg->SetCountryCode(51);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Venezuela")) {
                            cfg->SetCountryCode(52);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Albania")) {
                            cfg->SetCountryCode(64);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Australia")) {
                            cfg->SetCountryCode(65);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Austria")) {
                            cfg->SetCountryCode(66);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Belgium")) {
                            cfg->SetCountryCode(67);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Bosnia and Herzegovina")) {
                            cfg->SetCountryCode(68);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Botswana")) {
                            cfg->SetCountryCode(69);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Bulgaria")) {
                            cfg->SetCountryCode(70);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Croatia")) {
                            cfg->SetCountryCode(71);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Cyprus")) {
                            cfg->SetCountryCode(72);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Czech Republic")) {
                            cfg->SetCountryCode(73);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Denmark")) {
                            cfg->SetCountryCode(74);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Estonia")) {
                            cfg->SetCountryCode(75);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Finland")) {
                            cfg->SetCountryCode(76);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("France")) {
                            cfg->SetCountryCode(77);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Germany")) {
                            cfg->SetCountryCode(78);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Greece")) {
                            cfg->SetCountryCode(79);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Hungary")) {
                            cfg->SetCountryCode(80);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Iceland")) {
                            cfg->SetCountryCode(81);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Ireland")) {
                            cfg->SetCountryCode(82);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Italy")) {
                            cfg->SetCountryCode(83);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Latvia")) {
                            cfg->SetCountryCode(84);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Lesotho")) {
                            cfg->SetCountryCode(85);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Liechtenstein")) {
                            cfg->SetCountryCode(86);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Lithuania")) {
                            cfg->SetCountryCode(87);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Luxembourg")) {
                            cfg->SetCountryCode(88);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Macedonia")) {
                            cfg->SetCountryCode(89);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Malta")) {
                            cfg->SetCountryCode(90);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Montenegro")) {
                            cfg->SetCountryCode(91);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Mozambique")) {
                            cfg->SetCountryCode(92);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Namibia")) {
                            cfg->SetCountryCode(93);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Netherlands")) {
                            cfg->SetCountryCode(94);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("New Zealand")) {
                            cfg->SetCountryCode(95);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Norway")) {
                            cfg->SetCountryCode(96);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Poland")) {
                            cfg->SetCountryCode(97);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Portugal")) {
                            cfg->SetCountryCode(98);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Romania")) {
                            cfg->SetCountryCode(99);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Russia")) {
                            cfg->SetCountryCode(100);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Serbia")) {
                            cfg->SetCountryCode(101);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Slovakia")) {
                            cfg->SetCountryCode(102);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Slovenia")) {
                            cfg->SetCountryCode(103);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("South Africa")) {
                            cfg->SetCountryCode(104);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Spain")) {
                            cfg->SetCountryCode(105);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Swaziland")) {
                            cfg->SetCountryCode(106);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Sweden")) {
                            cfg->SetCountryCode(107);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Switzerland")) {
                            cfg->SetCountryCode(108);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Turkey")) {
                            cfg->SetCountryCode(109);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("United Kingdom")) {
                            cfg->SetCountryCode(110);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Zambia")) {
                            cfg->SetCountryCode(111);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Zimbabwe")) {
                            cfg->SetCountryCode(112);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Azerbaijan")) {
                            cfg->SetCountryCode(113);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Mauritania")) {
                            cfg->SetCountryCode(114);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Mali")) {
                            cfg->SetCountryCode(115);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Niger")) {
                            cfg->SetCountryCode(116);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Chad")) {
                            cfg->SetCountryCode(117);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Sudan")) {
                            cfg->SetCountryCode(118);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Eritrea")) {
                            cfg->SetCountryCode(119);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Djibouti")) {
                            cfg->SetCountryCode(120);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Somalia")) {
                            cfg->SetCountryCode(121);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Andorra")) {
                            cfg->SetCountryCode(122);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Gibraltar")) {
                            cfg->SetCountryCode(123);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Guernsey")) {
                            cfg->SetCountryCode(124);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Isle of Man")) {
                            cfg->SetCountryCode(125);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Jersey")) {
                            cfg->SetCountryCode(126);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Monaco")) {
                            cfg->SetCountryCode(127);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Taiwan")) {
                            cfg->SetCountryCode(128);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("South Korea")) {
                            cfg->SetCountryCode(136);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Hong Kong")) {
                            cfg->SetCountryCode(144);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Macau")) {
                            cfg->SetCountryCode(145);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Indonesia")) {
                            cfg->SetCountryCode(152);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Singapore")) {
                            cfg->SetCountryCode(153);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Thailand")) {
                            cfg->SetCountryCode(154);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Philippines")) {
                            cfg->SetCountryCode(155);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Malaysia")) {
                            cfg->SetCountryCode(156);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("China")) {
                            cfg->SetCountryCode(160);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("United Arab Emirates")) {
                            cfg->SetCountryCode(168);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("India")) {
                            cfg->SetCountryCode(169);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Egypt")) {
                            cfg->SetCountryCode(170);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Oman")) {
                            cfg->SetCountryCode(171);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Qatar")) {
                            cfg->SetCountryCode(172);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Kuwait")) {
                            cfg->SetCountryCode(173);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Saudi Arabia")) {
                            cfg->SetCountryCode(174);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Syria")) {
                            cfg->SetCountryCode(175);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Bahrain")) {
                            cfg->SetCountryCode(176);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Jordan")) {
                            cfg->SetCountryCode(177);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("San Marino")) {
                            cfg->SetCountryCode(184);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Vatican City")) {
                            cfg->SetCountryCode(185);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Bermuda")) {
                            cfg->SetCountryCode(186);
                            update_config_savegame = true;
                        }
                        ImGui::EndCombo();
                    }

                    ImGui::Text("Play Coins:");
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

                if (ImGui::BeginTabItem("Graphics")) {
                    ImGui::Text("Graphics settings are not persistent.");
                    ImGui::NewLine();

                    ImGui::Checkbox("Use Hardware Renderer", &Settings::values.use_hw_renderer);
                    ImGui::Checkbox("Use Hardware Shader", &Settings::values.use_hw_shader);
                    if (Settings::values.use_hw_shader) {
                        ImGui::Checkbox("Accurate Multiplication",
                                        &Settings::values.shaders_accurate_mul);
                    }
                    ImGui::Checkbox("Use Shader JIT", &Settings::values.use_shader_jit);
                    ImGui::Checkbox("Use Disk Shader Cache",
                                    &Settings::values.use_disk_shader_cache);
                    ImGui::Checkbox("Enable VSync", &Settings::values.enable_vsync);
                    ImGui::Checkbox("Dump Textures", &Settings::values.dump_textures);
                    ImGui::Checkbox("Use Custom Textures", &Settings::values.custom_textures);
                    ImGui::Checkbox("Preload Custom Textures", &Settings::values.preload_textures);
                    ImGui::Checkbox("Enable Linear Filtering", &Settings::values.filter_mode);
                    if (ImGui::IsItemHovered()) {
                        ImGui::SetTooltip("This is required for some shaders to work correctly");
                    }
                    ImGui::Checkbox("Sharper Distant Objects",
                                    &Settings::values.sharper_distant_objects);

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

                    ImGui::Text("Texture Filter");
                    ImGui::SameLine();
                    if (ImGui::BeginCombo("##texturefilter",
                                          Settings::values.texture_filter_name.c_str())) {
                        const auto& filters = OpenGL::TextureFilterer::GetFilterNames();

                        for (const auto& filter : filters) {
                            if (ImGui::Selectable(std::string(filter).c_str())) {
                                Settings::values.texture_filter_name = filter;
                            }
                        }

                        ImGui::EndCombo();
                    }

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
                    if (ImGui::Button("Load File")) {
                        const std::vector<std::string> path =
                            pfd::open_file("Load File", ".", {"JSON Files", "*.json"}).result();
                        if (!path.empty()) {
                            std::string contents;
                            FileUtil::ReadFileToString(true, path[0], contents);

                            try {
                                const nlohmann::json json = nlohmann::json::parse(contents);
                                Settings::values.buttons =
                                    json["buttons"]
                                        .get<std::array<std::string,
                                                        Settings::NativeButton::NumButtons>>();
                                Settings::values.analogs =
                                    json["analogs"]
                                        .get<std::array<std::string,
                                                        Settings::NativeAnalog::NumAnalogs>>();
                                Settings::values.motion_device =
                                    json["motion_device"].get<std::string>();
                                Settings::values.touch_device =
                                    json["touch_device"].get<std::string>();
                                Settings::values.udp_input_address =
                                    json["udp_input_address"].get<std::string>();
                                Settings::values.udp_input_port = json["udp_input_port"].get<u16>();
                                Settings::values.udp_pad_index = json["udp_pad_index"].get<u8>();
                            } catch (nlohmann::json::exception& exception) {
                                pfd::message("JSON Exception", exception.what());
                            }
                        }
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Save File")) {
                        const std::string path =
                            pfd::save_file("Save File", "controls.json", {"JSON Files", "*.json"})
                                .result();

                        if (!path.empty()) {
                            const std::string json =
                                nlohmann::json{
                                    {"buttons", Settings::values.buttons},
                                    {"analogs", Settings::values.analogs},
                                    {"motion_device", Settings::values.motion_device},
                                    {"touch_device", Settings::values.touch_device},
                                    {"udp_input_address", Settings::values.udp_input_address},
                                    {"udp_input_port", Settings::values.udp_input_port},
                                    {"udp_pad_index", Settings::values.udp_pad_index},
                                }
                                    .dump();

                            FileUtil::WriteStringToFile(true, path, json);
                        }
                    }
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
                    if (ImGui::IsItemHovered()) {
                        ImGui::SetTooltip(
                            "If you're using a XInput controller, make sure it says Axis 2+.");
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
                    if (ImGui::IsItemHovered()) {
                        ImGui::SetTooltip(
                            "If you're using a XInput controller, make sure it says Axis 5+.");
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
                    {
                        Common::ParamPackage params(
                            Settings::values.analogs[Settings::NativeAnalog::CirclePad]);
                        if (params.Get("engine", "") == "sdl") {
                            float deadzone = params.Get("deadzone", 0.0f);
                            ImGui::Text("Deadzone:");
                            ImGui::SameLine();
                            ImGui::PushItemWidth(100);
                            if (ImGui::SliderFloat("##deadzoneCirclePad", &deadzone, 0.0f, 1.0f)) {
                                params.Set("deadzone", deadzone);
                                Settings::values.analogs[Settings::NativeAnalog::CirclePad] =
                                    params.Serialize();
                            }
                            ImGui::PopItemWidth();
                        }
                    }
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
                    {
                        Common::ParamPackage params(
                            Settings::values.analogs[Settings::NativeAnalog::CStick]);
                        if (params.Get("engine", "") == "sdl") {
                            float deadzone = params.Get("deadzone", 0.0f);
                            ImGui::Text("Deadzone:");
                            ImGui::SameLine();
                            ImGui::PushItemWidth(100);
                            if (ImGui::SliderFloat("##deadzoneCStick", &deadzone, 0.0f, 1.0f)) {
                                params.Set("deadzone", deadzone);
                                Settings::values.analogs[Settings::NativeAnalog::CStick] =
                                    params.Serialize();
                            }
                            ImGui::PopItemWidth();
                        }
                    }
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

                    ImGui::NewLine();

                    ImGui::Text("Motion:");
                    ImGui::SameLine();

                    ImGui::PushItemWidth(100);
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
                    ImGui::PopItemWidth();

                    Common::ParamPackage motion_device(Settings::values.motion_device);

                    if (motion_device.Get("engine", "") == "motion_emu") {
                        int update_period = motion_device.Get("update_period", 100);
                        float sensitivity = motion_device.Get("sensitivity", 0.01f);
                        float clamp = motion_device.Get("tilt_clamp", 90.0f);

                        ImGui::SameLine();
                        ImGui::Text("Update Period:");
                        ImGui::SameLine();
                        ImGui::PushItemWidth(30);
                        if (ImGui::InputInt("##update_period", &update_period, 0)) {
                            motion_device.Set("update_period", update_period);
                            Settings::values.motion_device = motion_device.Serialize();
                        }
                        ImGui::PopItemWidth();

                        ImGui::SameLine();
                        ImGui::Text("Sensitivity:");
                        ImGui::SameLine();
                        ImGui::PushItemWidth(40);
                        if (ImGui::InputFloat("##sensitivity", &sensitivity)) {
                            motion_device.Set("sensitivity", sensitivity);
                            Settings::values.motion_device = motion_device.Serialize();
                        }
                        ImGui::PopItemWidth();

                        ImGui::SameLine();
                        ImGui::Text("Clamp:");
                        ImGui::SameLine();
                        ImGui::PushItemWidth(50);
                        if (ImGui::InputFloat("##clamp", &clamp)) {
                            motion_device.Set("tilt_clamp", clamp);
                            Settings::values.motion_device = motion_device.Serialize();
                        }
                        ImGui::PopItemWidth();
                    }

                    ImGui::Text("Touch:");
                    ImGui::SameLine();
                    ImGui::PushItemWidth(60);
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
                    ImGui::PopItemWidth();

                    Common::ParamPackage touch_device(Settings::values.touch_device);

                    if (touch_device.Get("engine", "") == "cemuhookudp") {
                        int min_x = touch_device.Get("min_x", 100);
                        int min_y = touch_device.Get("min_y", 50);
                        int max_x = touch_device.Get("max_x", 1800);
                        int max_y = touch_device.Get("max_y", 850);

                        ImGui::SameLine();
                        ImGui::Text("Min X:");
                        ImGui::SameLine();
                        ImGui::PushItemWidth(45);
                        if (ImGui::InputInt("##min_x", &min_x, 0)) {
                            touch_device.Set("min_x", min_x);
                            Settings::values.touch_device = touch_device.Serialize();
                        }
                        ImGui::PopItemWidth();

                        ImGui::SameLine();
                        ImGui::Text("Min Y:");
                        ImGui::SameLine();
                        ImGui::PushItemWidth(45);
                        if (ImGui::InputInt("##min_y", &min_y, 0)) {
                            touch_device.Set("min_y", min_y);
                            Settings::values.touch_device = touch_device.Serialize();
                        }
                        ImGui::PopItemWidth();

                        ImGui::SameLine();
                        ImGui::Text("Max X:");
                        ImGui::SameLine();
                        ImGui::PushItemWidth(45);
                        if (ImGui::InputInt("##max_x", &max_x, 0)) {
                            touch_device.Set("max_x", max_x);
                            Settings::values.touch_device = touch_device.Serialize();
                        }
                        ImGui::PopItemWidth();

                        ImGui::SameLine();
                        ImGui::Text("Max Y:");
                        ImGui::SameLine();
                        ImGui::PushItemWidth(45);
                        if (ImGui::InputInt("##max_y", &max_y, 0)) {
                            touch_device.Set("max_y", max_y);
                            Settings::values.touch_device = touch_device.Serialize();
                        }
                        ImGui::PopItemWidth();
                    }

                    if (motion_device.Get("engine", "") == "cemuhookudp" ||
                        touch_device.Get("engine", "") == "cemuhookudp") {
                        ImGui::Text("UDP:");

                        ImGui::SameLine();
                        ImGui::PushItemWidth(110);
                        ImGui::InputText("##udp_input_address",
                                         &Settings::values.udp_input_address);
                        ImGui::PopItemWidth();

                        ImGui::SameLine();
                        ImGui::Text(":");
                        ImGui::SameLine();
                        ImGui::PushItemWidth(40);
                        ImGui::InputScalar("##udp_input_port", ImGuiDataType_U16,
                                           &Settings::values.udp_input_port);
                        ImGui::PopItemWidth();

                        ImGui::SameLine();
                        ImGui::Text("Pad");
                        ImGui::SameLine();
                        ImGui::PushItemWidth(15);
                        ImGui::InputScalar("##udp_pad_index", ImGuiDataType_U8,
                                           &Settings::values.udp_pad_index);
                        ImGui::PopItemWidth();
                    }

                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem("Layout")) {
                    ImGui::Text("Layout settings are not persistent.");
                    ImGui::NewLine();

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

                if (ImGui::BeginTabItem("LLE Modules")) {
                    ImGui::Text("LLE Modules settings are not persistent.");
                    ImGui::NewLine();

                    for (auto& module : Settings::values.lle_modules) {
                        ImGui::Checkbox(module.first.c_str(), &module.second);
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

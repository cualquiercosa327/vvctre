// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <cstdlib>
#include <string>
#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <clip.h>
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
#include "common/stb_image_write.h"
#include "common/string_util.h"
#include "common/version.h"
#include "core/3ds.h"
#include "core/cheats/cheat_base.h"
#include "core/cheats/cheats.h"
#include "core/core.h"
#include "core/hle/service/am/am.h"
#include "core/hle/service/cfg/cfg.h"
#include "core/hle/service/nfc/nfc.h"
#include "core/hle/service/ptm/ptm.h"
#include "core/movie.h"
#include "core/settings.h"
#include "input_common/keyboard.h"
#include "input_common/main.h"
#include "input_common/motion_emu.h"
#include "input_common/sdl/sdl.h"
#include "video_core/renderer_base.h"
#include "video_core/renderer_opengl/post_processing_opengl.h"
#include "video_core/renderer_opengl/texture_filters/texture_filterer.h"
#include "video_core/video_core.h"
#include "vvctre/emu_window/emu_window_sdl2.h"

static std::string IPC_Recorder_GetStatusString(IPCDebugger::RequestStatus status) {
    switch (status) {
    case IPCDebugger::RequestStatus::Sent:
        return "Sent";
    case IPCDebugger::RequestStatus::Handling:
        return "Handling";
    case IPCDebugger::RequestStatus::Handled:
        return "Handled";
    case IPCDebugger::RequestStatus::HLEUnimplemented:
        return "HLEUnimplemented";
    default:
        break;
    }

    return "Invalid";
}

void EmuWindow_SDL2::OnMouseMotion(s32 x, s32 y) {
    TouchMoved((unsigned)std::max(x, 0), (unsigned)std::max(y, 0));
    InputCommon::GetMotionEmu()->Tilt(x, y);
}

void EmuWindow_SDL2::OnMouseButton(u32 button, u8 state, s32 x, s32 y) {
    if (button == SDL_BUTTON_LEFT) {
        if (state == SDL_PRESSED) {
            TouchPressed((unsigned)std::max(x, 0), (unsigned)std::max(y, 0));
        } else {
            TouchReleased();
        }
    } else if (button == SDL_BUTTON_RIGHT) {
        if (state == SDL_PRESSED) {
            InputCommon::GetMotionEmu()->BeginTilt(x, y);
        } else {
            InputCommon::GetMotionEmu()->EndTilt();
        }
    }
}

std::pair<unsigned, unsigned> EmuWindow_SDL2::TouchToPixelPos(float touch_x, float touch_y) const {
    int w, h;
    SDL_GetWindowSize(render_window, &w, &h);

    touch_x *= w;
    touch_y *= h;

    return {static_cast<unsigned>(std::max(std::round(touch_x), 0.0f)),
            static_cast<unsigned>(std::max(std::round(touch_y), 0.0f))};
}

void EmuWindow_SDL2::OnFingerDown(float x, float y) {
    // TODO(NeatNit): keep track of multitouch using the fingerID and a dictionary of some kind
    // This isn't critical because the best we can do when we have that is to average them, like the
    // 3DS does

    const auto [px, py] = TouchToPixelPos(x, y);
    TouchPressed(px, py);
}

void EmuWindow_SDL2::OnFingerMotion(float x, float y) {
    const auto [px, py] = TouchToPixelPos(x, y);
    TouchMoved(px, py);
}

void EmuWindow_SDL2::OnFingerUp() {
    TouchReleased();
}

void EmuWindow_SDL2::OnKeyEvent(int key, u8 state) {
    if (state == SDL_PRESSED) {
        InputCommon::GetKeyboard()->PressKey(key);
    } else if (state == SDL_RELEASED) {
        InputCommon::GetKeyboard()->ReleaseKey(key);
    }
}

bool EmuWindow_SDL2::IsOpen() const {
    return is_open;
}

void EmuWindow_SDL2::Close() {
    is_open = false;
}

void EmuWindow_SDL2::DiskShaderCacheProgress(const float value) {
    disk_shader_cache_loading_progress = value;
}

void EmuWindow_SDL2::OnResize() {
    int width, height;
    SDL_GetWindowSize(render_window, &width, &height);
    UpdateCurrentFramebufferLayout(width, height);
}

void EmuWindow_SDL2::ToggleFullscreen() {
    if (SDL_GetWindowFlags(render_window) & SDL_WINDOW_FULLSCREEN_DESKTOP) {
        SDL_SetWindowFullscreen(render_window, 0);
    } else {
        SDL_SetWindowFullscreen(render_window, SDL_WINDOW_FULLSCREEN_DESKTOP);
    }
}

EmuWindow_SDL2::EmuWindow_SDL2(Core::System& system) : system(system) {
    const std::string window_title = fmt::format("vvctre {}", version::vvctre.to_string());

    render_window =
        SDL_CreateWindow(window_title.c_str(),
                         SDL_WINDOWPOS_UNDEFINED, // x position
                         SDL_WINDOWPOS_UNDEFINED, // y position
                         Core::kScreenTopWidth, Core::kScreenTopHeight + Core::kScreenBottomHeight,
                         SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);

    if (render_window == nullptr) {
        LOG_CRITICAL(Frontend, "Failed to create SDL2 window: {}", SDL_GetError());
        std::exit(1);
    }

    if (Settings::values.start_in_fullscreen_mode) {
        ToggleFullscreen();
    } else {
        SDL_SetWindowMinimumSize(render_window, Core::kScreenTopWidth,
                                 Core::kScreenTopHeight + Core::kScreenBottomHeight);
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

    SDL_GL_SetSwapInterval(Settings::values.enable_vsync ? 1 : 0);

    OnResize();
    SDL_PumpEvents();
    LOG_INFO(Frontend, "Version: {}", version::vvctre.to_string());
    LOG_INFO(Frontend, "Movie version: {}", version::movie);
    LOG_INFO(Frontend, "Shader cache version: {}", version::shader_cache);
    Settings::LogSettings();

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui::GetIO().IniFilename = nullptr;
    ImGui_ImplSDL2_InitForOpenGL(render_window, gl_context);
    ImGui_ImplOpenGL3_Init("#version 330 core");

    DoneCurrent();
}

EmuWindow_SDL2::~EmuWindow_SDL2() {
    InputCommon::Shutdown();
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
    SDL_GL_DeleteContext(gl_context);
    SDL_Quit();
}

void EmuWindow_SDL2::SwapBuffers() {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL2_NewFrame(render_window);
    ImGui::NewFrame();
    ImGuiIO& io = ImGui::GetIO();

    if (ImGui::Begin("FPS and Menu", nullptr,
                     ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings |
                         ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_AlwaysAutoResize)) {
        if (ImGui::IsWindowAppearing()) {
            ImGui::SetWindowFocus(nullptr);
        }
        ImGui::SetWindowPos(ImVec2(), ImGuiCond_Once);
        ImGui::TextColored(fps_color, "%d FPS", static_cast<int>(ImGui::GetIO().Framerate));
        if (ImGui::BeginPopupContextItem(nullptr, ImGuiMouseButton_Right)) {
            system.frontend_paused = true;

            if (ImGui::BeginMenu("File")) {
                if (ImGui::MenuItem("Load File")) {
                    const std::vector<std::string> result =
                        pfd::open_file(
                            "Load File", ".",
                            {"3DS Executables", "*.cci *.3ds *.cxi *.3dsx *.app *.elf *.axf"})
                            .result();

                    if (!result.empty()) {
                        system.SetResetFilePath(result[0]);
                        system.RequestReset();
                    }
                }

                if (ImGui::MenuItem("Install CIA (blocking)")) {
                    const std::vector<std::string> files =
                        pfd::open_file("Install CIA", ".", {"CTR Importable Archive", "*.cia"},
                                       true)
                            .result();

                    if (!files.empty()) {
                        const std::vector<std::string> files =
                            pfd::open_file("Install CIA", ".", {"CTR Importable Archive", "*.cia"},
                                           true)
                                .result();

                        auto am = Service::AM::GetModule(system);

                        for (const auto& file : files) {
                            const Service::AM::InstallStatus status = Service::AM::InstallCIA(file);

                            switch (status) {
                            case Service::AM::InstallStatus::Success:
                                if (am != nullptr) {
                                    am->ScanForAllTitles();
                                }
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
                }

                if (ImGui::BeginMenu("Amiibo")) {
                    if (ImGui::MenuItem("Load")) {
                        const auto result =
                            pfd::open_file("Load Amiibo", ".",
                                           {"Amiibo Files", "*.bin", "Anything", "*"})
                                .result();

                        if (!result.empty()) {
                            FileUtil::IOFile file(result[0], "rb");
                            Service::NFC::AmiiboData data;
                            if (file.ReadArray(&data, 1) == 1) {
                                std::shared_ptr<Service::NFC::Module::Interface> nfc =
                                    system.ServiceManager()
                                        .GetService<Service::NFC::Module::Interface>("nfc:u");
                                if (nfc != nullptr) {
                                    nfc->LoadAmiibo(data);
                                }
                            } else {
                                pfd::message("vvctre", "Failed to load the amiibo file",
                                             pfd::choice::ok, pfd::icon::error);
                            }
                        }
                    }

                    if (ImGui::MenuItem("Remove")) {
                        std::shared_ptr<Service::NFC::Module::Interface> nfc =
                            system.ServiceManager().GetService<Service::NFC::Module::Interface>(
                                "nfc:u");
                        if (nfc != nullptr) {
                            nfc->RemoveAmiibo();
                        }
                    }

                    ImGui::EndMenu();
                }

                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Settings")) {
                if (ImGui::BeginMenu("General")) {
                    if (ImGui::Checkbox("Limit Speed", &Settings::values.use_frame_limit)) {
                        Settings::LogSettings();
                    }

                    if (Settings::values.use_frame_limit) {
                        ImGui::SameLine();
                        ImGui::Spacing();
                        ImGui::SameLine();
                        ImGui::Text("Limit:");
                        ImGui::SameLine();
                        ImGui::PushItemWidth(45);
                        if (ImGui::InputScalar("##speedlimit", ImGuiDataType_U16,
                                               &Settings::values.frame_limit)) {
                            Settings::LogSettings();
                        }
                        ImGui::PopItemWidth();
                        ImGui::SameLine();
                        ImGui::Text("%");
                    }

                    ImGui::EndMenu();
                }

                if (ImGui::BeginMenu("Audio")) {
                    ImGui::Text("Volume:");
                    ImGui::SameLine();
                    if (ImGui::SliderFloat("##volume", &Settings::values.volume, 0.0f, 1.0f)) {
                        Settings::LogSettings();
                    }

                    ImGui::Text("Speed:");
                    ImGui::SameLine();
                    if (ImGui::SliderFloat("##speed", &Settings::values.audio_speed, 0.001f,
                                           5.0f)) {
                        Settings::LogSettings();
                    }

                    ImGui::Text("Sink:");
                    ImGui::SameLine();
                    if (ImGui::BeginCombo("##sink", Settings::values.sink_id.c_str())) {
                        if (ImGui::Selectable("auto")) {
                            Settings::values.sink_id = "auto";
                            Settings::Apply();
                            Settings::LogSettings();
                        }
#ifdef HAVE_CUBEB
                        if (ImGui::Selectable("cubeb")) {
                            Settings::values.sink_id = "cubeb";
                            Settings::Apply();
                            Settings::LogSettings();
                        }
#endif
                        if (ImGui::Selectable("sdl2")) {
                            Settings::values.sink_id = "sdl2";
                            Settings::Apply();
                            Settings::LogSettings();
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
                                Settings::Apply();
                                Settings::LogSettings();
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
                            Settings::Apply();
                            Settings::LogSettings();
                        }
                        if (ImGui::Selectable("Real Device")) {
                            Settings::values.mic_input_type = Settings::MicInputType::Real;
                            Settings::Apply();
                            Settings::LogSettings();
                        }
                        if (ImGui::Selectable("Static Noise")) {
                            Settings::values.mic_input_type = Settings::MicInputType::Static;
                            Settings::Apply();
                            Settings::LogSettings();
                        }
                        ImGui::EndCombo();
                    }

                    if (Settings::values.mic_input_type == Settings::MicInputType::Real) {
                        ImGui::Text("Microphone Device:");
                        ImGui::SameLine();

                        if (ImGui::BeginCombo("##microphonedevice",
                                              Settings::values.mic_input_device.c_str())) {
#ifdef HAVE_CUBEB
                            for (const auto& device : AudioCore::ListCubebInputDevices()) {
                                if (ImGui::Selectable(device.c_str())) {
                                    Settings::values.mic_input_device = device;
                                    Settings::Apply();
                                    Settings::LogSettings();
                                }
                            }
#endif

                            ImGui::EndCombo();
                        }
                    }

                    ImGui::EndMenu();
                }

                if (ImGui::BeginMenu("Graphics")) {
                    if (ImGui::Checkbox("Use Hardware Renderer",
                                        &Settings::values.use_hw_renderer)) {
                        Settings::Apply();
                        Settings::LogSettings();
                    }
                    ImGui::Indent();

                    if (ImGui::Checkbox("Use Hardware Shader", &Settings::values.use_hw_shader)) {
                        Settings::Apply();
                        Settings::LogSettings();
                    }
                    ImGui::Indent();

                    if (ImGui::Checkbox("Use Accurate Multiplication",
                                        &Settings::values.shaders_accurate_mul)) {
                        Settings::Apply();
                        Settings::LogSettings();
                    }

                    ImGui::Unindent();
                    ImGui::Unindent();

                    if (ImGui::Checkbox("Use Shader JIT", &Settings::values.use_shader_jit)) {
                        Settings::LogSettings();
                    }

                    if (ImGui::Checkbox("Enable VSync", &Settings::values.enable_vsync)) {
                        Settings::LogSettings();
                    }

                    ImGui::Text("Resolution:");
                    ImGui::SameLine();
                    const u16 min = 0;
                    const u16 max = 10;
                    if (ImGui::SliderScalar("##resolution", ImGuiDataType_U16,
                                            &Settings::values.resolution_factor, &min, &max,
                                            Settings::values.resolution_factor == 0 ? "Window Size"
                                                                                    : "%d")) {
                        Settings::LogSettings();
                    }

                    ImGui::Text("Background Color:");
                    ImGui::SameLine();
                    if (ImGui::ColorEdit3("##backgroundcolor", &Settings::values.bg_red,
                                          ImGuiColorEditFlags_NoInputs)) {
                        VideoCore::g_renderer_bg_color_update_requested = true;
                        Settings::LogSettings();
                    }

                    ImGui::Text("Post Processing Shader:");
                    ImGui::SameLine();
                    if (ImGui::BeginCombo("##postprocessingshader",
                                          Settings::values.pp_shader_name.c_str())) {
                        const auto shaders = OpenGL::GetPostProcessingShaderList(
                            Settings::values.render_3d == Settings::StereoRenderOption::Anaglyph);

                        if (Settings::values.render_3d == Settings::StereoRenderOption::Anaglyph &&
                            ImGui::Selectable("dubois (builtin)")) {
                            Settings::values.pp_shader_name = "dubois (builtin)";
                            Settings::Apply();
                            Settings::LogSettings();
                        } else if (Settings::values.render_3d ==
                                       Settings::StereoRenderOption::Interlaced &&
                                   ImGui::Selectable("horizontal (builtin)")) {
                            Settings::values.pp_shader_name = "horizontal (builtin)";
                            Settings::Apply();
                            Settings::LogSettings();
                        } else if ((Settings::values.render_3d ==
                                        Settings::StereoRenderOption::Off ||
                                    Settings::values.render_3d ==
                                        Settings::StereoRenderOption::SideBySide) &&
                                   ImGui::Selectable("none (builtin)")) {
                            Settings::values.pp_shader_name = "none (builtin)";
                            Settings::Apply();
                            Settings::LogSettings();
                        }

                        for (const auto& shader : shaders) {
                            if (ImGui::Selectable(shader.c_str())) {
                                Settings::values.pp_shader_name = shader;
                                Settings::Apply();
                                Settings::LogSettings();
                            }
                        }

                        ImGui::EndCombo();
                    }

                    ImGui::Text("Texture Filter:");
                    ImGui::SameLine();
                    if (ImGui::BeginCombo("##texturefilter",
                                          Settings::values.texture_filter_name.c_str())) {
                        const auto& filters = OpenGL::TextureFilterer::GetFilterNames();

                        for (const auto& filter : filters) {
                            if (ImGui::Selectable(std::string(filter).c_str())) {
                                Settings::values.texture_filter_name = filter;
                                Settings::Apply();
                                Settings::LogSettings();
                            }
                        }

                        ImGui::EndCombo();
                    }

                    if (ImGui::Checkbox("Dump Textures", &Settings::values.dump_textures)) {
                        Settings::LogSettings();
                    }

                    if (ImGui::Checkbox("Use Custom Textures", &Settings::values.custom_textures)) {
                        Settings::LogSettings();
                    }

                    if (ImGui::Checkbox("Preload Custom Textures",
                                        &Settings::values.preload_textures)) {
                        Settings::LogSettings();
                    }

                    ImGui::Text("3D:");
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
                            Settings::Apply();
                            Settings::LogSettings();
                        }

                        if (ImGui::Selectable("Side by Side",
                                              Settings::values.render_3d ==
                                                  Settings::StereoRenderOption::SideBySide)) {
                            Settings::values.render_3d = Settings::StereoRenderOption::SideBySide;
                            Settings::Apply();
                            Settings::LogSettings();
                        }

                        if (ImGui::Selectable("Anaglyph",
                                              Settings::values.render_3d ==
                                                  Settings::StereoRenderOption::Anaglyph)) {
                            Settings::values.render_3d = Settings::StereoRenderOption::Anaglyph;
                            Settings::Apply();
                            Settings::LogSettings();
                        }

                        if (ImGui::Selectable("Interlaced",
                                              Settings::values.render_3d ==
                                                  Settings::StereoRenderOption::Interlaced)) {
                            Settings::values.render_3d = Settings::StereoRenderOption::Interlaced;
                            Settings::Apply();
                            Settings::LogSettings();
                        }

                        ImGui::EndCombo();
                    }

                    ImGui::SameLine();

                    u8 factor_3d = Settings::values.factor_3d;
                    const u8 factor_3d_min = 0;
                    const u8 factor_3d_max = 100;
                    if (ImGui::SliderScalar("##factor_3d", ImGuiDataType_U8, &factor_3d,
                                            &factor_3d_min, &factor_3d_max, "%d%%")) {
                        Settings::values.factor_3d = factor_3d;
                        Settings::LogSettings();
                    }

                    ImGui::EndMenu();
                }

                if (ImGui::BeginMenu("Camera")) {
                    ImGui::Text("Inner Camera Engine:");
                    ImGui::SameLine();
                    if (ImGui::BeginCombo("##innerengine",
                                          Settings::values
                                              .camera_name[static_cast<std::size_t>(
                                                  Service::CAM::CameraIndex::InnerCamera)]
                                              .c_str())) {
                        if (ImGui::Selectable("blank")) {
                            Settings::values.camera_name[static_cast<std::size_t>(
                                Service::CAM::CameraIndex::InnerCamera)] = "blank";
                            auto cam = Service::CAM::GetModule(system);
                            if (cam != nullptr) {
                                cam->ReloadCameraDevices();
                            }
                            Settings::LogSettings();
                        }
                        if (ImGui::Selectable("image (configuration: file path or URL)")) {
                            Settings::values.camera_name[static_cast<std::size_t>(
                                Service::CAM::CameraIndex::InnerCamera)] = "image";
                            auto cam = Service::CAM::GetModule(system);
                            if (cam != nullptr) {
                                cam->ReloadCameraDevices();
                            }
                            Settings::LogSettings();
                        }
                        ImGui::EndCombo();
                    }

                    ImGui::Text("Inner Camera Configuration:");
                    ImGui::SameLine();
                    if (ImGui::InputText("##innerconfiguration",
                                         &Settings::values.camera_config[static_cast<std::size_t>(
                                             Service::CAM::CameraIndex::InnerCamera)])) {
                        auto cam = Service::CAM::GetModule(system);
                        if (cam != nullptr) {
                            cam->ReloadCameraDevices();
                        }
                        Settings::LogSettings();
                    }

                    ImGui::Text("Outer Left Engine:");
                    ImGui::SameLine();
                    if (ImGui::BeginCombo("##outerleftengine",
                                          Settings::values
                                              .camera_name[static_cast<std::size_t>(
                                                  Service::CAM::CameraIndex::OuterLeftCamera)]
                                              .c_str())) {
                        if (ImGui::Selectable("blank")) {
                            Settings::values.camera_name[static_cast<std::size_t>(
                                Service::CAM::CameraIndex::OuterLeftCamera)] = "blank";
                            auto cam = Service::CAM::GetModule(system);
                            if (cam != nullptr) {
                                cam->ReloadCameraDevices();
                            }
                            Settings::LogSettings();
                        }
                        if (ImGui::Selectable("image (configuration: file path or URL)")) {
                            Settings::values.camera_name[static_cast<std::size_t>(
                                Service::CAM::CameraIndex::OuterLeftCamera)] = "image";
                            auto cam = Service::CAM::GetModule(system);
                            if (cam != nullptr) {
                                cam->ReloadCameraDevices();
                            }
                            Settings::LogSettings();
                        }
                        ImGui::EndCombo();
                    }

                    ImGui::Text("Outer Left Configuration:");
                    ImGui::SameLine();
                    if (ImGui::InputText("##outerleftconfiguration",
                                         &Settings::values.camera_config[static_cast<std::size_t>(
                                             Service::CAM::CameraIndex::OuterLeftCamera)])) {
                        auto cam = Service::CAM::GetModule(system);
                        if (cam != nullptr) {
                            cam->ReloadCameraDevices();
                        }
                        Settings::LogSettings();
                    }

                    ImGui::Text("Outer Right Engine:");
                    ImGui::SameLine();
                    if (ImGui::BeginCombo("##outerrightengine",
                                          Settings::values
                                              .camera_name[static_cast<std::size_t>(
                                                  Service::CAM::CameraIndex::OuterRightCamera)]
                                              .c_str())) {
                        if (ImGui::Selectable("blank")) {
                            Settings::values.camera_name[static_cast<std::size_t>(
                                Service::CAM::CameraIndex::OuterRightCamera)] = "blank";
                            auto cam = Service::CAM::GetModule(system);
                            if (cam != nullptr) {
                                cam->ReloadCameraDevices();
                            }
                            Settings::LogSettings();
                        }
                        if (ImGui::Selectable("image (configuration: file path or URL)")) {
                            Settings::values.camera_name[static_cast<std::size_t>(
                                Service::CAM::CameraIndex::OuterRightCamera)] = "image";
                            auto cam = Service::CAM::GetModule(system);
                            if (cam != nullptr) {
                                cam->ReloadCameraDevices();
                            }
                            Settings::LogSettings();
                        }
                        ImGui::EndCombo();
                    }

                    ImGui::Text("Outer Right Configuration:");
                    ImGui::SameLine();
                    if (ImGui::InputText("##outerrightconfiguration",
                                         &Settings::values.camera_config[static_cast<std::size_t>(
                                             Service::CAM::CameraIndex::OuterRightCamera)])) {
                        auto cam = Service::CAM::GetModule(system);
                        if (cam != nullptr) {
                            cam->ReloadCameraDevices();
                        }
                        Settings::LogSettings();
                    }

                    ImGui::EndMenu();
                }

                if (ImGui::BeginMenu("System")) {
                    auto cfg = Service::CFG::GetModule(system);

                    if (cfg != nullptr) {
                        ImGui::Text("Username (changing will restart emulation):");
                        ImGui::SameLine();

                        std::string username = Common::UTF16ToUTF8(cfg->GetUsername());
                        if (ImGui::InputText("##username", &username)) {
                            cfg->SetUsername(Common::UTF8ToUTF16(username));
                            cfg->UpdateConfigNANDSavegame();
                            system.RequestReset();
                        }

                        ImGui::Text("Birthday (changing will restart emulation):");
                        ImGui::SameLine();

                        auto [month, day] = cfg->GetBirthday();

                        if (ImGui::InputScalar("##birthday", ImGuiDataType_U8, &day)) {
                            cfg->SetBirthday(month, day);
                            cfg->UpdateConfigNANDSavegame();
                            system.RequestReset();
                        }

                        ImGui::SameLine();
                        ImGui::Text("/");
                        ImGui::SameLine();

                        if (ImGui::InputScalar("##birthmonth", ImGuiDataType_U8, &month)) {
                            cfg->SetBirthday(month, day);
                            cfg->UpdateConfigNANDSavegame();
                            system.RequestReset();
                        }

                        ImGui::Text("Language (changing will restart emulation):");
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
                                cfg->UpdateConfigNANDSavegame();
                                system.RequestReset();
                            }

                            if (ImGui::Selectable("English")) {
                                cfg->SetSystemLanguage(Service::CFG::SystemLanguage::LANGUAGE_EN);
                                cfg->UpdateConfigNANDSavegame();
                                system.RequestReset();
                            }

                            if (ImGui::Selectable("French")) {
                                cfg->SetSystemLanguage(Service::CFG::SystemLanguage::LANGUAGE_FR);
                                cfg->UpdateConfigNANDSavegame();
                                system.RequestReset();
                            }

                            if (ImGui::Selectable("German")) {
                                cfg->SetSystemLanguage(Service::CFG::SystemLanguage::LANGUAGE_DE);
                                cfg->UpdateConfigNANDSavegame();
                                system.RequestReset();
                            }

                            if (ImGui::Selectable("Italian")) {
                                cfg->SetSystemLanguage(Service::CFG::SystemLanguage::LANGUAGE_IT);
                                cfg->UpdateConfigNANDSavegame();
                                system.RequestReset();
                            }

                            if (ImGui::Selectable("Spanish")) {
                                cfg->SetSystemLanguage(Service::CFG::SystemLanguage::LANGUAGE_ES);
                                cfg->UpdateConfigNANDSavegame();
                                system.RequestReset();
                            }

                            if (ImGui::Selectable("Simplified Chinese")) {
                                cfg->SetSystemLanguage(Service::CFG::SystemLanguage::LANGUAGE_ZH);
                                cfg->UpdateConfigNANDSavegame();
                                system.RequestReset();
                            }

                            if (ImGui::Selectable("Korean")) {
                                cfg->SetSystemLanguage(Service::CFG::SystemLanguage::LANGUAGE_KO);
                                cfg->UpdateConfigNANDSavegame();
                                system.RequestReset();
                            }

                            if (ImGui::Selectable("Dutch")) {
                                cfg->SetSystemLanguage(Service::CFG::SystemLanguage::LANGUAGE_NL);
                                cfg->UpdateConfigNANDSavegame();
                                system.RequestReset();
                            }

                            if (ImGui::Selectable("Portugese")) {
                                cfg->SetSystemLanguage(Service::CFG::SystemLanguage::LANGUAGE_PT);
                                cfg->UpdateConfigNANDSavegame();
                                system.RequestReset();
                            }

                            if (ImGui::Selectable("Russian")) {
                                cfg->SetSystemLanguage(Service::CFG::SystemLanguage::LANGUAGE_RU);
                                cfg->UpdateConfigNANDSavegame();
                                system.RequestReset();
                            }

                            if (ImGui::Selectable("Traditional Chinese")) {
                                cfg->SetSystemLanguage(Service::CFG::SystemLanguage::LANGUAGE_TW);
                                cfg->UpdateConfigNANDSavegame();
                                system.RequestReset();
                            }

                            ImGui::EndCombo();
                        }

                        ImGui::Text("Sound output mode (changing will restart emulation):");
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
                                cfg->UpdateConfigNANDSavegame();
                                system.RequestReset();
                            }
                            if (ImGui::Selectable("Stereo")) {
                                cfg->SetSoundOutputMode(
                                    Service::CFG::SoundOutputMode::SOUND_STEREO);
                                cfg->UpdateConfigNANDSavegame();
                                system.RequestReset();
                            }
                            if (ImGui::Selectable("Surround")) {
                                cfg->SetSoundOutputMode(
                                    Service::CFG::SoundOutputMode::SOUND_SURROUND);
                                cfg->UpdateConfigNANDSavegame();
                                system.RequestReset();
                            }
                            ImGui::EndCombo();
                        }

                        ImGui::Text("Country (changing will restart emulation):");
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
                                cfg->UpdateConfigNANDSavegame();
                                system.RequestReset();
                            }
                            if (ImGui::Selectable("Anguilla")) {
                                cfg->SetCountryCode(8);
                                cfg->UpdateConfigNANDSavegame();
                                system.RequestReset();
                            }
                            if (ImGui::Selectable("Antigua and Barbuda")) {
                                cfg->SetCountryCode(9);
                                cfg->UpdateConfigNANDSavegame();
                                system.RequestReset();
                            }
                            if (ImGui::Selectable("Argentina")) {
                                cfg->SetCountryCode(10);
                                cfg->UpdateConfigNANDSavegame();
                                system.RequestReset();
                            }
                            if (ImGui::Selectable("Aruba")) {
                                cfg->SetCountryCode(11);
                                cfg->UpdateConfigNANDSavegame();
                                system.RequestReset();
                            }
                            if (ImGui::Selectable("Bahamas")) {
                                cfg->SetCountryCode(12);
                                cfg->UpdateConfigNANDSavegame();
                                system.RequestReset();
                            }
                            if (ImGui::Selectable("Barbados")) {
                                cfg->SetCountryCode(13);
                                cfg->UpdateConfigNANDSavegame();
                                system.RequestReset();
                            }
                            if (ImGui::Selectable("Belize")) {
                                cfg->SetCountryCode(14);
                                cfg->UpdateConfigNANDSavegame();
                                system.RequestReset();
                            }
                            if (ImGui::Selectable("Bolivia")) {
                                cfg->SetCountryCode(15);
                                cfg->UpdateConfigNANDSavegame();
                                system.RequestReset();
                            }
                            if (ImGui::Selectable("Brazil")) {
                                cfg->SetCountryCode(16);
                                cfg->UpdateConfigNANDSavegame();
                                system.RequestReset();
                            }
                            if (ImGui::Selectable("British Virgin Islands")) {
                                cfg->SetCountryCode(17);
                                cfg->UpdateConfigNANDSavegame();
                                system.RequestReset();
                            }
                            if (ImGui::Selectable("Canada")) {
                                cfg->SetCountryCode(18);
                                cfg->UpdateConfigNANDSavegame();
                                system.RequestReset();
                            }
                            if (ImGui::Selectable("Cayman Islands")) {
                                cfg->SetCountryCode(19);
                                cfg->UpdateConfigNANDSavegame();
                                system.RequestReset();
                            }
                            if (ImGui::Selectable("Chile")) {
                                cfg->SetCountryCode(20);
                                cfg->UpdateConfigNANDSavegame();
                                system.RequestReset();
                            }
                            if (ImGui::Selectable("Colombia")) {
                                cfg->SetCountryCode(21);
                                cfg->UpdateConfigNANDSavegame();
                                system.RequestReset();
                            }
                            if (ImGui::Selectable("Costa Rica")) {
                                cfg->SetCountryCode(22);
                                cfg->UpdateConfigNANDSavegame();
                                system.RequestReset();
                            }
                            if (ImGui::Selectable("Dominica")) {
                                cfg->SetCountryCode(23);
                                cfg->UpdateConfigNANDSavegame();
                                system.RequestReset();
                            }
                            if (ImGui::Selectable("Dominican Republic")) {
                                cfg->SetCountryCode(24);
                                cfg->UpdateConfigNANDSavegame();
                                system.RequestReset();
                            }
                            if (ImGui::Selectable("Ecuador")) {
                                cfg->SetCountryCode(25);
                                cfg->UpdateConfigNANDSavegame();
                                system.RequestReset();
                            }
                            if (ImGui::Selectable("El Salvador")) {
                                cfg->SetCountryCode(26);
                                cfg->UpdateConfigNANDSavegame();
                                system.RequestReset();
                            }
                            if (ImGui::Selectable("French Guiana")) {
                                cfg->SetCountryCode(27);
                                cfg->UpdateConfigNANDSavegame();
                                system.RequestReset();
                            }
                            if (ImGui::Selectable("Grenada")) {
                                cfg->SetCountryCode(28);
                                cfg->UpdateConfigNANDSavegame();
                                system.RequestReset();
                            }
                            if (ImGui::Selectable("Guadeloupe")) {
                                cfg->SetCountryCode(29);
                                cfg->UpdateConfigNANDSavegame();
                                system.RequestReset();
                            }
                            if (ImGui::Selectable("Guatemala")) {
                                cfg->SetCountryCode(30);
                                cfg->UpdateConfigNANDSavegame();
                                system.RequestReset();
                            }
                            if (ImGui::Selectable("Guyana")) {
                                cfg->SetCountryCode(31);
                                cfg->UpdateConfigNANDSavegame();
                                system.RequestReset();
                            }
                            if (ImGui::Selectable("Haiti")) {
                                cfg->SetCountryCode(32);
                                cfg->UpdateConfigNANDSavegame();
                                system.RequestReset();
                            }
                            if (ImGui::Selectable("Honduras")) {
                                cfg->SetCountryCode(33);
                                cfg->UpdateConfigNANDSavegame();
                                system.RequestReset();
                            }
                            if (ImGui::Selectable("Jamaica")) {
                                cfg->SetCountryCode(34);
                                cfg->UpdateConfigNANDSavegame();
                                system.RequestReset();
                            }
                            if (ImGui::Selectable("Martinique")) {
                                cfg->SetCountryCode(35);
                                cfg->UpdateConfigNANDSavegame();
                                system.RequestReset();
                            }
                            if (ImGui::Selectable("Mexico")) {
                                cfg->SetCountryCode(36);
                                cfg->UpdateConfigNANDSavegame();
                                system.RequestReset();
                            }
                            if (ImGui::Selectable("Montserrat")) {
                                cfg->SetCountryCode(37);
                                cfg->UpdateConfigNANDSavegame();
                                system.RequestReset();
                            }
                            if (ImGui::Selectable("Netherlands Antilles")) {
                                cfg->SetCountryCode(38);
                                cfg->UpdateConfigNANDSavegame();
                                system.RequestReset();
                            }
                            if (ImGui::Selectable("Nicaragua")) {
                                cfg->SetCountryCode(39);
                                cfg->UpdateConfigNANDSavegame();
                                system.RequestReset();
                            }
                            if (ImGui::Selectable("Panama")) {
                                cfg->SetCountryCode(40);
                                cfg->UpdateConfigNANDSavegame();
                                system.RequestReset();
                            }
                            if (ImGui::Selectable("Paraguay")) {
                                cfg->SetCountryCode(41);
                                cfg->UpdateConfigNANDSavegame();
                                system.RequestReset();
                            }
                            if (ImGui::Selectable("Peru")) {
                                cfg->SetCountryCode(42);
                                cfg->UpdateConfigNANDSavegame();
                                system.RequestReset();
                            }
                            if (ImGui::Selectable("Saint Kitts and Nevis")) {
                                cfg->SetCountryCode(43);
                                cfg->UpdateConfigNANDSavegame();
                                system.RequestReset();
                            }
                            if (ImGui::Selectable("Saint Lucia")) {
                                cfg->SetCountryCode(44);
                                cfg->UpdateConfigNANDSavegame();
                                system.RequestReset();
                            }
                            if (ImGui::Selectable("Saint Vincent and the Grenadines")) {
                                cfg->SetCountryCode(45);
                                cfg->UpdateConfigNANDSavegame();
                                system.RequestReset();
                            }
                            if (ImGui::Selectable("Suriname")) {
                                cfg->SetCountryCode(46);
                                cfg->UpdateConfigNANDSavegame();
                                system.RequestReset();
                            }
                            if (ImGui::Selectable("Trinidad and Tobago")) {
                                cfg->SetCountryCode(47);
                                cfg->UpdateConfigNANDSavegame();
                                system.RequestReset();
                            }
                            if (ImGui::Selectable("Turks and Caicos Islands")) {
                                cfg->SetCountryCode(48);
                                cfg->UpdateConfigNANDSavegame();
                                system.RequestReset();
                            }
                            if (ImGui::Selectable("United States")) {
                                cfg->SetCountryCode(49);
                                cfg->UpdateConfigNANDSavegame();
                                system.RequestReset();
                            }
                            if (ImGui::Selectable("Uruguay")) {
                                cfg->SetCountryCode(50);
                                cfg->UpdateConfigNANDSavegame();
                                system.RequestReset();
                            }
                            if (ImGui::Selectable("US Virgin Islands")) {
                                cfg->SetCountryCode(51);
                                cfg->UpdateConfigNANDSavegame();
                                system.RequestReset();
                            }
                            if (ImGui::Selectable("Venezuela")) {
                                cfg->SetCountryCode(52);
                                cfg->UpdateConfigNANDSavegame();
                                system.RequestReset();
                            }
                            if (ImGui::Selectable("Albania")) {
                                cfg->SetCountryCode(64);
                                cfg->UpdateConfigNANDSavegame();
                                system.RequestReset();
                            }
                            if (ImGui::Selectable("Australia")) {
                                cfg->SetCountryCode(65);
                                cfg->UpdateConfigNANDSavegame();
                                system.RequestReset();
                            }
                            if (ImGui::Selectable("Austria")) {
                                cfg->SetCountryCode(66);
                                cfg->UpdateConfigNANDSavegame();
                                system.RequestReset();
                            }
                            if (ImGui::Selectable("Belgium")) {
                                cfg->SetCountryCode(67);
                                cfg->UpdateConfigNANDSavegame();
                                system.RequestReset();
                            }
                            if (ImGui::Selectable("Bosnia and Herzegovina")) {
                                cfg->SetCountryCode(68);
                                cfg->UpdateConfigNANDSavegame();
                                system.RequestReset();
                            }
                            if (ImGui::Selectable("Botswana")) {
                                cfg->SetCountryCode(69);
                                cfg->UpdateConfigNANDSavegame();
                                system.RequestReset();
                            }
                            if (ImGui::Selectable("Bulgaria")) {
                                cfg->SetCountryCode(70);
                                cfg->UpdateConfigNANDSavegame();
                                system.RequestReset();
                            }
                            if (ImGui::Selectable("Croatia")) {
                                cfg->SetCountryCode(71);
                                cfg->UpdateConfigNANDSavegame();
                                system.RequestReset();
                            }
                            if (ImGui::Selectable("Cyprus")) {
                                cfg->SetCountryCode(72);
                                cfg->UpdateConfigNANDSavegame();
                                system.RequestReset();
                            }
                            if (ImGui::Selectable("Czech Republic")) {
                                cfg->SetCountryCode(73);
                                cfg->UpdateConfigNANDSavegame();
                                system.RequestReset();
                            }
                            if (ImGui::Selectable("Denmark")) {
                                cfg->SetCountryCode(74);
                                cfg->UpdateConfigNANDSavegame();
                                system.RequestReset();
                            }
                            if (ImGui::Selectable("Estonia")) {
                                cfg->SetCountryCode(75);
                                cfg->UpdateConfigNANDSavegame();
                                system.RequestReset();
                            }
                            if (ImGui::Selectable("Finland")) {
                                cfg->SetCountryCode(76);
                                cfg->UpdateConfigNANDSavegame();
                                system.RequestReset();
                            }
                            if (ImGui::Selectable("France")) {
                                cfg->SetCountryCode(77);
                                cfg->UpdateConfigNANDSavegame();
                                system.RequestReset();
                            }
                            if (ImGui::Selectable("Germany")) {
                                cfg->SetCountryCode(78);
                                cfg->UpdateConfigNANDSavegame();
                                system.RequestReset();
                            }
                            if (ImGui::Selectable("Greece")) {
                                cfg->SetCountryCode(79);
                                cfg->UpdateConfigNANDSavegame();
                                system.RequestReset();
                            }
                            if (ImGui::Selectable("Hungary")) {
                                cfg->SetCountryCode(80);
                                cfg->UpdateConfigNANDSavegame();
                                system.RequestReset();
                            }
                            if (ImGui::Selectable("Iceland")) {
                                cfg->SetCountryCode(81);
                                cfg->UpdateConfigNANDSavegame();
                                system.RequestReset();
                            }
                            if (ImGui::Selectable("Ireland")) {
                                cfg->SetCountryCode(82);
                                cfg->UpdateConfigNANDSavegame();
                                system.RequestReset();
                            }
                            if (ImGui::Selectable("Italy")) {
                                cfg->SetCountryCode(83);
                                cfg->UpdateConfigNANDSavegame();
                                system.RequestReset();
                            }
                            if (ImGui::Selectable("Latvia")) {
                                cfg->SetCountryCode(84);
                                cfg->UpdateConfigNANDSavegame();
                                system.RequestReset();
                            }
                            if (ImGui::Selectable("Lesotho")) {
                                cfg->SetCountryCode(85);
                                cfg->UpdateConfigNANDSavegame();
                                system.RequestReset();
                            }
                            if (ImGui::Selectable("Liechtenstein")) {
                                cfg->SetCountryCode(86);
                                cfg->UpdateConfigNANDSavegame();
                                system.RequestReset();
                            }
                            if (ImGui::Selectable("Lithuania")) {
                                cfg->SetCountryCode(87);
                                cfg->UpdateConfigNANDSavegame();
                                system.RequestReset();
                            }
                            if (ImGui::Selectable("Luxembourg")) {
                                cfg->SetCountryCode(88);
                                cfg->UpdateConfigNANDSavegame();
                                system.RequestReset();
                            }
                            if (ImGui::Selectable("Macedonia")) {
                                cfg->SetCountryCode(89);
                                cfg->UpdateConfigNANDSavegame();
                                system.RequestReset();
                            }
                            if (ImGui::Selectable("Malta")) {
                                cfg->SetCountryCode(90);
                                cfg->UpdateConfigNANDSavegame();
                                system.RequestReset();
                            }
                            if (ImGui::Selectable("Montenegro")) {
                                cfg->SetCountryCode(91);
                                cfg->UpdateConfigNANDSavegame();
                                system.RequestReset();
                            }
                            if (ImGui::Selectable("Mozambique")) {
                                cfg->SetCountryCode(92);
                                cfg->UpdateConfigNANDSavegame();
                                system.RequestReset();
                            }
                            if (ImGui::Selectable("Namibia")) {
                                cfg->SetCountryCode(93);
                                cfg->UpdateConfigNANDSavegame();
                                system.RequestReset();
                            }
                            if (ImGui::Selectable("Netherlands")) {
                                cfg->SetCountryCode(94);
                                cfg->UpdateConfigNANDSavegame();
                                system.RequestReset();
                            }
                            if (ImGui::Selectable("New Zealand")) {
                                cfg->SetCountryCode(95);
                                cfg->UpdateConfigNANDSavegame();
                                system.RequestReset();
                            }
                            if (ImGui::Selectable("Norway")) {
                                cfg->SetCountryCode(96);
                                cfg->UpdateConfigNANDSavegame();
                                system.RequestReset();
                            }
                            if (ImGui::Selectable("Poland")) {
                                cfg->SetCountryCode(97);
                                cfg->UpdateConfigNANDSavegame();
                                system.RequestReset();
                            }
                            if (ImGui::Selectable("Portugal")) {
                                cfg->SetCountryCode(98);
                                cfg->UpdateConfigNANDSavegame();
                                system.RequestReset();
                            }
                            if (ImGui::Selectable("Romania")) {
                                cfg->SetCountryCode(99);
                                cfg->UpdateConfigNANDSavegame();
                                system.RequestReset();
                            }
                            if (ImGui::Selectable("Russia")) {
                                cfg->SetCountryCode(100);
                                cfg->UpdateConfigNANDSavegame();
                                system.RequestReset();
                            }
                            if (ImGui::Selectable("Serbia")) {
                                cfg->SetCountryCode(101);
                                cfg->UpdateConfigNANDSavegame();
                                system.RequestReset();
                            }
                            if (ImGui::Selectable("Slovakia")) {
                                cfg->SetCountryCode(102);
                                cfg->UpdateConfigNANDSavegame();
                                system.RequestReset();
                            }
                            if (ImGui::Selectable("Slovenia")) {
                                cfg->SetCountryCode(103);
                                cfg->UpdateConfigNANDSavegame();
                                system.RequestReset();
                            }
                            if (ImGui::Selectable("South Africa")) {
                                cfg->SetCountryCode(104);
                                cfg->UpdateConfigNANDSavegame();
                                system.RequestReset();
                            }
                            if (ImGui::Selectable("Spain")) {
                                cfg->SetCountryCode(105);
                                cfg->UpdateConfigNANDSavegame();
                                system.RequestReset();
                            }
                            if (ImGui::Selectable("Swaziland")) {
                                cfg->SetCountryCode(106);
                                cfg->UpdateConfigNANDSavegame();
                                system.RequestReset();
                            }
                            if (ImGui::Selectable("Sweden")) {
                                cfg->SetCountryCode(107);
                                cfg->UpdateConfigNANDSavegame();
                                system.RequestReset();
                            }
                            if (ImGui::Selectable("Switzerland")) {
                                cfg->SetCountryCode(108);
                                cfg->UpdateConfigNANDSavegame();
                                system.RequestReset();
                            }
                            if (ImGui::Selectable("Turkey")) {
                                cfg->SetCountryCode(109);
                                cfg->UpdateConfigNANDSavegame();
                                system.RequestReset();
                            }
                            if (ImGui::Selectable("United Kingdom")) {
                                cfg->SetCountryCode(110);
                                cfg->UpdateConfigNANDSavegame();
                                system.RequestReset();
                            }
                            if (ImGui::Selectable("Zambia")) {
                                cfg->SetCountryCode(111);
                                cfg->UpdateConfigNANDSavegame();
                                system.RequestReset();
                            }
                            if (ImGui::Selectable("Zimbabwe")) {
                                cfg->SetCountryCode(112);
                                cfg->UpdateConfigNANDSavegame();
                                system.RequestReset();
                            }
                            if (ImGui::Selectable("Azerbaijan")) {
                                cfg->SetCountryCode(113);
                                cfg->UpdateConfigNANDSavegame();
                                system.RequestReset();
                            }
                            if (ImGui::Selectable("Mauritania")) {
                                cfg->SetCountryCode(114);
                                cfg->UpdateConfigNANDSavegame();
                                system.RequestReset();
                            }
                            if (ImGui::Selectable("Mali")) {
                                cfg->SetCountryCode(115);
                                cfg->UpdateConfigNANDSavegame();
                                system.RequestReset();
                            }
                            if (ImGui::Selectable("Niger")) {
                                cfg->SetCountryCode(116);
                                cfg->UpdateConfigNANDSavegame();
                                system.RequestReset();
                            }
                            if (ImGui::Selectable("Chad")) {
                                cfg->SetCountryCode(117);
                                cfg->UpdateConfigNANDSavegame();
                                system.RequestReset();
                            }
                            if (ImGui::Selectable("Sudan")) {
                                cfg->SetCountryCode(118);
                                cfg->UpdateConfigNANDSavegame();
                                system.RequestReset();
                            }
                            if (ImGui::Selectable("Eritrea")) {
                                cfg->SetCountryCode(119);
                                cfg->UpdateConfigNANDSavegame();
                                system.RequestReset();
                            }
                            if (ImGui::Selectable("Djibouti")) {
                                cfg->SetCountryCode(120);
                                cfg->UpdateConfigNANDSavegame();
                                system.RequestReset();
                            }
                            if (ImGui::Selectable("Somalia")) {
                                cfg->SetCountryCode(121);
                                cfg->UpdateConfigNANDSavegame();
                                system.RequestReset();
                            }
                            if (ImGui::Selectable("Andorra")) {
                                cfg->SetCountryCode(122);
                                cfg->UpdateConfigNANDSavegame();
                                system.RequestReset();
                            }
                            if (ImGui::Selectable("Gibraltar")) {
                                cfg->SetCountryCode(123);
                                cfg->UpdateConfigNANDSavegame();
                                system.RequestReset();
                            }
                            if (ImGui::Selectable("Guernsey")) {
                                cfg->SetCountryCode(124);
                                cfg->UpdateConfigNANDSavegame();
                                system.RequestReset();
                            }
                            if (ImGui::Selectable("Isle of Man")) {
                                cfg->SetCountryCode(125);
                                cfg->UpdateConfigNANDSavegame();
                                system.RequestReset();
                            }
                            if (ImGui::Selectable("Jersey")) {
                                cfg->SetCountryCode(126);
                                cfg->UpdateConfigNANDSavegame();
                                system.RequestReset();
                            }
                            if (ImGui::Selectable("Monaco")) {
                                cfg->SetCountryCode(127);
                                cfg->UpdateConfigNANDSavegame();
                                system.RequestReset();
                            }
                            if (ImGui::Selectable("Taiwan")) {
                                cfg->SetCountryCode(128);
                                cfg->UpdateConfigNANDSavegame();
                                system.RequestReset();
                            }
                            if (ImGui::Selectable("South Korea")) {
                                cfg->SetCountryCode(136);
                                cfg->UpdateConfigNANDSavegame();
                                system.RequestReset();
                            }
                            if (ImGui::Selectable("Hong Kong")) {
                                cfg->SetCountryCode(144);
                                cfg->UpdateConfigNANDSavegame();
                                system.RequestReset();
                            }
                            if (ImGui::Selectable("Macau")) {
                                cfg->SetCountryCode(145);
                                cfg->UpdateConfigNANDSavegame();
                                system.RequestReset();
                            }
                            if (ImGui::Selectable("Indonesia")) {
                                cfg->SetCountryCode(152);
                                cfg->UpdateConfigNANDSavegame();
                                system.RequestReset();
                            }
                            if (ImGui::Selectable("Singapore")) {
                                cfg->SetCountryCode(153);
                                cfg->UpdateConfigNANDSavegame();
                                system.RequestReset();
                            }
                            if (ImGui::Selectable("Thailand")) {
                                cfg->SetCountryCode(154);
                                cfg->UpdateConfigNANDSavegame();
                                system.RequestReset();
                            }
                            if (ImGui::Selectable("Philippines")) {
                                cfg->SetCountryCode(155);
                                cfg->UpdateConfigNANDSavegame();
                                system.RequestReset();
                            }
                            if (ImGui::Selectable("Malaysia")) {
                                cfg->SetCountryCode(156);
                                cfg->UpdateConfigNANDSavegame();
                                system.RequestReset();
                            }
                            if (ImGui::Selectable("China")) {
                                cfg->SetCountryCode(160);
                                cfg->UpdateConfigNANDSavegame();
                                system.RequestReset();
                            }
                            if (ImGui::Selectable("United Arab Emirates")) {
                                cfg->SetCountryCode(168);
                                cfg->UpdateConfigNANDSavegame();
                                system.RequestReset();
                            }
                            if (ImGui::Selectable("India")) {
                                cfg->SetCountryCode(169);
                                cfg->UpdateConfigNANDSavegame();
                                system.RequestReset();
                            }
                            if (ImGui::Selectable("Egypt")) {
                                cfg->SetCountryCode(170);
                                cfg->UpdateConfigNANDSavegame();
                                system.RequestReset();
                            }
                            if (ImGui::Selectable("Oman")) {
                                cfg->SetCountryCode(171);
                                cfg->UpdateConfigNANDSavegame();
                                system.RequestReset();
                            }
                            if (ImGui::Selectable("Qatar")) {
                                cfg->SetCountryCode(172);
                                cfg->UpdateConfigNANDSavegame();
                                system.RequestReset();
                            }
                            if (ImGui::Selectable("Kuwait")) {
                                cfg->SetCountryCode(173);
                                cfg->UpdateConfigNANDSavegame();
                                system.RequestReset();
                            }
                            if (ImGui::Selectable("Saudi Arabia")) {
                                cfg->SetCountryCode(174);
                                cfg->UpdateConfigNANDSavegame();
                                system.RequestReset();
                            }
                            if (ImGui::Selectable("Syria")) {
                                cfg->SetCountryCode(175);
                                cfg->UpdateConfigNANDSavegame();
                                system.RequestReset();
                            }
                            if (ImGui::Selectable("Bahrain")) {
                                cfg->SetCountryCode(176);
                                cfg->UpdateConfigNANDSavegame();
                                system.RequestReset();
                            }
                            if (ImGui::Selectable("Jordan")) {
                                cfg->SetCountryCode(177);
                                cfg->UpdateConfigNANDSavegame();
                                system.RequestReset();
                            }
                            if (ImGui::Selectable("San Marino")) {
                                cfg->SetCountryCode(184);
                                cfg->UpdateConfigNANDSavegame();
                                system.RequestReset();
                            }
                            if (ImGui::Selectable("Vatican City")) {
                                cfg->SetCountryCode(185);
                                cfg->UpdateConfigNANDSavegame();
                                system.RequestReset();
                            }
                            if (ImGui::Selectable("Bermuda")) {
                                cfg->SetCountryCode(186);
                                cfg->UpdateConfigNANDSavegame();
                                system.RequestReset();
                            }
                            ImGui::EndCombo();
                        }
                    }

                    ImGui::Text("Play Coins (may need to restart emulation):");
                    ImGui::SameLine();

                    const u16 min = 0;
                    const u16 max = 300;

                    u16 play_coins = Service::PTM::Module::GetPlayCoins();
                    if (ImGui::SliderScalar("##playcoins", ImGuiDataType_U16, &play_coins, &min,
                                            &max)) {
                        Service::PTM::Module::SetPlayCoins(play_coins);
                    }

                    ImGui::EndMenu();
                }

                if (ImGui::BeginMenu("GUI")) {
                    ImGui::Text("FPS Color:");
                    ImGui::SameLine();
                    ImGui::ColorPicker4("##fps_color", (float*)&fps_color);

                    ImGui::EndMenu();
                }

                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("View")) {
                ImGui::Checkbox("Cheats", &show_cheats_window);

                ImGui::Separator();

                if (ImGui::Checkbox("Fullscreen", SDL_GetWindowFlags(render_window) &
                                                      SDL_WINDOW_FULLSCREEN_DESKTOP)) {
                    ToggleFullscreen();
                }

                ImGui::Separator();
                if (ImGui::BeginMenu("Layout")) {
                    if (!Settings::values.custom_layout) {
                        if (ImGui::MenuItem("Default")) {
                            Settings::values.layout_option = Settings::LayoutOption::Default;
                            Settings::Apply();
                            Settings::LogSettings();
                        }

                        if (ImGui::MenuItem("Single Screen")) {
                            Settings::values.layout_option = Settings::LayoutOption::SingleScreen;
                            Settings::Apply();
                            Settings::LogSettings();
                        }

                        if (ImGui::MenuItem("Large Screen")) {
                            Settings::values.layout_option = Settings::LayoutOption::LargeScreen;
                            Settings::Apply();
                            Settings::LogSettings();
                        }

                        if (ImGui::MenuItem("Side by Side")) {
                            Settings::values.layout_option = Settings::LayoutOption::SideScreen;
                            Settings::Apply();
                            Settings::LogSettings();
                        }

                        if (ImGui::MenuItem("Medium Screen")) {
                            Settings::values.layout_option = Settings::LayoutOption::MediumScreen;
                            Settings::Apply();
                            Settings::LogSettings();
                        }
                    } else {
                        ImGui::Text("Top Left");
                        ImGui::SameLine();
                        if (ImGui::InputScalar("##topleft", ImGuiDataType_U16,
                                               &Settings::values.custom_top_left)) {
                            Settings::Apply();
                            Settings::LogSettings();
                        }
                        ImGui::Text("Top Top");
                        ImGui::SameLine();
                        if (ImGui::InputScalar("##toptop", ImGuiDataType_U16,
                                               &Settings::values.custom_top_top)) {
                            Settings::Apply();
                            Settings::LogSettings();
                        }
                        ImGui::Text("Top Right");
                        ImGui::SameLine();
                        if (ImGui::InputScalar("##topright", ImGuiDataType_U16,
                                               &Settings::values.custom_top_right)) {
                            Settings::Apply();
                            Settings::LogSettings();
                        }
                        ImGui::Text("Top Bottom");
                        ImGui::SameLine();
                        if (ImGui::InputScalar("##topbottom", ImGuiDataType_U16,
                                               &Settings::values.custom_top_bottom)) {
                            Settings::Apply();
                            Settings::LogSettings();
                        }
                        ImGui::Text("Bottom Left");
                        ImGui::SameLine();
                        if (ImGui::InputScalar("##bottomleft", ImGuiDataType_U16,
                                               &Settings::values.custom_bottom_left)) {
                            Settings::Apply();
                            Settings::LogSettings();
                        }
                        ImGui::Text("Bottom Top");
                        ImGui::SameLine();
                        if (ImGui::InputScalar("##bottomtop", ImGuiDataType_U16,
                                               &Settings::values.custom_bottom_top)) {
                            Settings::Apply();
                            Settings::LogSettings();
                        }
                        ImGui::Text("Bottom Right");
                        ImGui::SameLine();
                        if (ImGui::InputScalar("##bottomright", ImGuiDataType_U16,
                                               &Settings::values.custom_bottom_right)) {
                            Settings::Apply();
                            Settings::LogSettings();
                        }
                        ImGui::Text("Bottom Bottom");
                        ImGui::SameLine();
                        if (ImGui::InputScalar("##bottombottom", ImGuiDataType_U16,
                                               &Settings::values.custom_bottom_bottom)) {
                            Settings::Apply();
                            Settings::LogSettings();
                        }
                    }

                    ImGui::Separator();

                    if (ImGui::Checkbox("Use Custom Layout", &Settings::values.custom_layout)) {
                        Settings::Apply();
                        Settings::LogSettings();
                    }

                    if (ImGui::Checkbox("Swap Screens", &Settings::values.swap_screen)) {
                        Settings::Apply();
                        Settings::LogSettings();
                    }

                    if (ImGui::Checkbox("Upright Orientation", &Settings::values.upright_screen)) {
                        Settings::Apply();
                        Settings::LogSettings();
                    }

                    ImGui::EndMenu();
                }

                if (ImGui::BeginMenu("Debugging")) {
                    if (ImGui::Checkbox("IPC Recorder", &show_ipc_recorder_window)) {
                        ipc_records.clear();

                        if (!show_ipc_recorder_window) {
                            ipc_recorder_enabled = false;

                            IPCDebugger::Recorder& r =
                                Core::System::GetInstance().Kernel().GetIPCRecorder();

                            r.SetEnabled(ipc_recorder_enabled);
                            r.UnbindCallback(ipc_recorder_callback);
                            ipc_recorder_callback = nullptr;
                        }
                    }

                    ImGui::EndMenu();
                }

                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Emulation")) {
                if (ImGui::MenuItem("Restart")) {
                    system.RequestReset();
                }

                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Tools")) {
                if (ImGui::MenuItem("Save Screenshot")) {
                    const auto& layout = GetFramebufferLayout();
                    u8* data = new u8[layout.width * layout.height * 4];
                    if (VideoCore::RequestScreenshot(
                            data,
                            [=] {
                                const auto filename =
                                    pfd::save_file("Save Screenshot", "screenshot.png",
                                                   {"Portable Network Graphics", "*.png"})
                                        .result();
                                if (!filename.empty()) {
                                    std::vector<u8> v(layout.width * layout.height * 4);
                                    std::memcpy(v.data(), data, v.size());
                                    delete[] data;

                                    const auto rotate =
                                        [](const std::vector<u8>& input,
                                           const Layout::FramebufferLayout& layout) {
                                            std::vector<u8> output(input.size());

                                            for (std::size_t i = 0; i < layout.height; i++) {
                                                for (std::size_t j = 0; j < layout.width; j++) {
                                                    for (std::size_t k = 0; k < 4; k++) {
                                                        output[i * (layout.width * 4) + j * 4 + k] =
                                                            input[(layout.height - i - 1) *
                                                                      (layout.width * 4) +
                                                                  j * 4 + k];
                                                    }
                                                }
                                            }

                                            return output;
                                        };

                                    const auto convert_bgra_to_rgba =
                                        [](const std::vector<u8>& input,
                                           const Layout::FramebufferLayout& layout) {
                                            int offset = 0;
                                            std::vector<u8> output(input.size());

                                            for (u32 y = 0; y < layout.height; ++y) {
                                                for (u32 x = 0; x < layout.width; ++x) {
                                                    output[offset] = input[offset + 2];
                                                    output[offset + 1] = input[offset + 1];
                                                    output[offset + 2] = input[offset];
                                                    output[offset + 3] = input[offset + 3];

                                                    offset += 4;
                                                }
                                            }

                                            return output;
                                        };

                                    v = convert_bgra_to_rgba(rotate(v, layout), layout);

                                    stbi_write_png(filename.c_str(), layout.width, layout.height, 4,
                                                   v.data(), layout.width * 4);
                                }
                            },
                            layout)) {
                        delete[] data;
                    }
                }

                if (ImGui::MenuItem("Copy Screenshot")) {
                    CopyScreenshot();
                }

                if (ImGui::BeginMenu("Movie")) {
                    auto& movie = Core::Movie::GetInstance();

                    if (ImGui::MenuItem("Play", nullptr, nullptr,
                                        !movie.IsPlayingInput() && !movie.IsRecordingInput())) {
                        const auto filename =
                            pfd::open_file("Play Movie", ".", {"VVCTRE Movie", "*.vcm"}).result();
                        if (!filename.empty()) {
                            const auto movie_result = movie.ValidateMovie(filename[0]);
                            switch (movie_result) {
                            case Core::Movie::ValidationResult::OK:
                                movie.StartPlayback(filename[0], [&] {
                                    pfd::message("vvctre", "Playback finished", pfd::choice::ok);
                                });
                                break;
                            case Core::Movie::ValidationResult::GameDismatch:
                                pfd::message(
                                    "vvctre",
                                    "Movie was recorded using a ROM with a different program ID",
                                    pfd::choice::ok, pfd::icon::warning);
                                movie.StartPlayback(filename[0], [&] {
                                    pfd::message("vvctre", "Playback finished", pfd::choice::ok,
                                                 pfd::icon::info);
                                });
                                break;
                            case Core::Movie::ValidationResult::Invalid:
                                pfd::message("vvctre", "Movie file doesn't have a valid header",
                                             pfd::choice::ok, pfd::icon::info);
                                break;
                            }
                        }
                    }

                    if (ImGui::MenuItem("Record", nullptr, nullptr,
                                        !movie.IsPlayingInput() && !movie.IsRecordingInput())) {
                        const std::string filename =
                            pfd::save_file("Play Movie", "movie.vcm", {"VVCTRE Movie", "*.vcm"})
                                .result();
                        if (!filename.empty()) {
                            movie.StartRecording(filename);
                        }
                    }

                    if (ImGui::MenuItem("Stop Playback/Recording", nullptr, nullptr,
                                        movie.IsPlayingInput() || movie.IsRecordingInput())) {
                        movie.Shutdown();
                        pfd::message("vvctre", "Movie saved");
                    }

                    ImGui::EndMenu();
                }

                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Help")) {
                if (ImGui::MenuItem("Discord Server")) {
#ifdef _WIN32
                    const int code = std::system("start https://discord.gg/fUrNqwA");
#else
                    const int code = std::system("xdg-open https://discord.gg/fUrNqwA");
#endif
                    LOG_INFO(Frontend, "Opened Discord invite, exit code: {}", code);
                }

                ImGui::EndMenu();
            }

            ImGui::EndPopup();
        } else {
            system.frontend_paused = false;
        }
    }
    ImGui::End();

    if (swkbd_config != nullptr && swkbd_code != nullptr && swkbd_text != nullptr) {
        ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f),
                                ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        if (ImGui::Begin("Keyboard", nullptr,
                         ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::InputTextWithHint("", swkbd_config->hint_text.c_str(), swkbd_text,
                                     swkbd_config->multiline_mode ? ImGuiInputTextFlags_Multiline
                                                                  : 0);

            if (Frontend::SoftwareKeyboard::ValidateInput(*swkbd_text, *swkbd_config) ==
                Frontend::ValidationError::None) {
                switch (swkbd_config->button_config) {
                case Frontend::ButtonConfig::None:
                case Frontend::ButtonConfig::Single: {
                    if (ImGui::Button((!swkbd_config->has_custom_button_text ||
                                               swkbd_config->button_text[0].empty()
                                           ? Frontend::SWKBD_BUTTON_OKAY
                                           : swkbd_config->button_text[0])
                                          .c_str())) {
                        swkbd_config = nullptr;
                        swkbd_code = nullptr;
                        swkbd_text = nullptr;
                    }
                    break;
                }

                case Frontend::ButtonConfig::Dual: {
                    const std::string cancel = (swkbd_config->button_text.size() < 1 ||
                                                swkbd_config->button_text[0].empty())
                                                   ? Frontend::SWKBD_BUTTON_CANCEL
                                                   : swkbd_config->button_text[0];
                    const std::string ok = (swkbd_config->button_text.size() < 2 ||
                                            swkbd_config->button_text[1].empty())
                                               ? Frontend::SWKBD_BUTTON_OKAY
                                               : swkbd_config->button_text[1];
                    if (ImGui::Button(cancel.c_str())) {
                        swkbd_config = nullptr;
                        swkbd_code = nullptr;
                        swkbd_text = nullptr;
                        break;
                    }
                    ImGui::SameLine();
                    if (ImGui::Button(ok.c_str())) {
                        *swkbd_code = 1;
                        swkbd_config = nullptr;
                        swkbd_code = nullptr;
                        swkbd_text = nullptr;
                    }
                    break;
                }

                case Frontend::ButtonConfig::Triple: {
                    const std::string cancel = (swkbd_config->button_text.size() < 1 ||
                                                swkbd_config->button_text[0].empty())
                                                   ? Frontend::SWKBD_BUTTON_CANCEL
                                                   : swkbd_config->button_text[0];
                    const std::string forgot = (swkbd_config->button_text.size() < 2 ||
                                                swkbd_config->button_text[1].empty())
                                                   ? Frontend::SWKBD_BUTTON_FORGOT
                                                   : swkbd_config->button_text[1];
                    const std::string ok = (swkbd_config->button_text.size() < 3 ||
                                            swkbd_config->button_text[2].empty())
                                               ? Frontend::SWKBD_BUTTON_OKAY
                                               : swkbd_config->button_text[2];
                    if (ImGui::Button(cancel.c_str())) {
                        swkbd_config = nullptr;
                        swkbd_code = nullptr;
                        swkbd_text = nullptr;
                        break;
                    }
                    ImGui::SameLine();
                    if (ImGui::Button(forgot.c_str())) {
                        *swkbd_code = 1;
                        swkbd_config = nullptr;
                        swkbd_code = nullptr;
                        swkbd_text = nullptr;
                        break;
                    }
                    ImGui::SameLine();
                    if (ImGui::Button(ok.c_str())) {
                        *swkbd_code = 2;
                        swkbd_config = nullptr;
                        swkbd_code = nullptr;
                        swkbd_text = nullptr;
                    }
                    break;
                }
                }
            }
        }
        ImGui::End();
    }

    if (mii_selector_config != nullptr && mii_selector_miis != nullptr &&
        mii_selector_code != nullptr && mii_selector_selected_mii != nullptr) {
        ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f),
                                ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        if (ImGui::Begin(
                (mii_selector_config->title.empty() ? "Mii Selector" : mii_selector_config->title)
                    .c_str(),
                nullptr, ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize)) {
            if (ImGui::ListBoxHeader("##miis")) {
                for (std::size_t index = 0; index < mii_selector_miis->size(); ++index) {
                    if (ImGui::Selectable(
                            Common::UTF16BufferToUTF8(mii_selector_miis->at(index).mii_name)
                                .c_str())) {
                        *mii_selector_code = 0;
                        *mii_selector_selected_mii = mii_selector_miis->at(index);
                        mii_selector_config = nullptr;
                        mii_selector_miis = nullptr;
                        mii_selector_code = nullptr;
                        mii_selector_selected_mii = nullptr;
                        break;
                    }
                }
                ImGui::ListBoxFooter();
            }
            if (mii_selector_config && mii_selector_config->enable_cancel_button &&
                ImGui::Button("Cancel")) {
                mii_selector_config = nullptr;
                mii_selector_miis = nullptr;
                mii_selector_code = nullptr;
                mii_selector_selected_mii = nullptr;
            }
        }
        ImGui::End();
    }

    if (show_ipc_recorder_window) {
        if (ImGui::Begin("IPC Recorder", &show_ipc_recorder_window,
                         ImGuiWindowFlags_NoSavedSettings)) {
            if (ImGui::Checkbox("Enabled", &ipc_recorder_enabled)) {
                IPCDebugger::Recorder& r = Core::System::GetInstance().Kernel().GetIPCRecorder();

                r.SetEnabled(ipc_recorder_enabled);

                if (ipc_recorder_enabled) {
                    ipc_recorder_callback =
                        r.BindCallback([&](const IPCDebugger::RequestRecord& record) {
                            ipc_records[record.id] = record;
                        });
                } else {
                    r.UnbindCallback(ipc_recorder_callback);
                    ipc_recorder_callback = nullptr;
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Clear")) {
                ipc_records.clear();
            }
            ImGui::SameLine();
            static std::string filter;
            ImGui::InputTextWithHint("##filter", "Filter", &filter);
            if (ImGui::ListBoxHeader("##records", ImVec2(-1.0f, -1.0f))) {
                for (const auto& record : ipc_records) {
                    std::string service_name;
                    std::string function_name = "Unknown";
                    if (record.second.client_port.id != -1) {
                        service_name = system.ServiceManager().GetServiceNameByPortId(
                            static_cast<u32>(record.second.client_port.id));
                    }
                    if (service_name.empty()) {
                        service_name = record.second.server_session.name;
                        service_name = Common::ReplaceAll(service_name, "_Server", "");
                        service_name = Common::ReplaceAll(service_name, "_Client", "");
                    }
                    const std::string label = fmt::format(
                        "#{} - {} - {} (0x{:08X}) - {} - {}", record.first, service_name,
                        record.second.function_name.empty() ? "Unknown"
                                                            : record.second.function_name,
                        record.second.untranslated_request_cmdbuf.empty()
                            ? 0xFFFFFFFF
                            : record.second.untranslated_request_cmdbuf[0],
                        record.second.is_hle ? "HLE" : "LLE",
                        IPC_Recorder_GetStatusString(record.second.status));
                    if (label.find(filter) != std::string::npos) {
                        ImGui::Selectable(label.c_str());
                        if (ImGui::IsItemHovered()) {
                            ImGui::SetTooltip(
                                "id: %d\n"
                                "status: %d\n"
                                "client_process.type: %s\n"
                                "client_process.name: %s\n"
                                "client_process.id: %d\n"
                                "client_thread.type: %s\n"
                                "client_thread.name: %s\n"
                                "client_thread.id: %d\n"
                                "client_session.type: %s\n"
                                "client_session.name: %s\n"
                                "client_session.id: %d\n"
                                "client_port.type: %s\n"
                                "client_port.name: %s\n"
                                "client_port.id: %d\n"
                                "server_process.type: %s\n"
                                "server_process.name: %s\n"
                                "server_process.id: %d\n"
                                "server_thread.type: %s\n"
                                "server_thread.name: %s\n"
                                "server_thread.id: %d\n"
                                "server_session.type: %s\n"
                                "server_session.name: %s\n"
                                "server_session.id: %d\n"
                                "function_name: %s\n"
                                "is_hle: %s\n"
                                "untranslated_request_cmdbuf: %s\n"
                                "translated_request_cmdbuf: %s\n"
                                "untranslated_reply_cmdbuf: %s\n"
                                "translated_reply_cmdbuf: %s",
                                record.first, static_cast<int>(record.second.status),
                                record.second.client_process.type.c_str(),
                                record.second.client_process.name.c_str(),
                                record.second.client_process.id,
                                record.second.client_thread.type.c_str(),
                                record.second.client_thread.name.c_str(),
                                record.second.client_thread.id,
                                record.second.client_session.type.c_str(),
                                record.second.client_session.name.c_str(),
                                record.second.client_session.id,
                                record.second.client_port.type.c_str(),
                                record.second.client_port.name.c_str(),
                                record.second.client_port.id,
                                record.second.server_process.type.c_str(),
                                record.second.server_process.name.c_str(),
                                record.second.server_process.id,
                                record.second.server_thread.type.c_str(),
                                record.second.server_thread.name.c_str(),
                                record.second.server_thread.id,
                                record.second.server_session.type.c_str(),
                                record.second.server_session.name.c_str(),
                                record.second.server_session.id,
                                record.second.function_name.c_str(),
                                record.second.is_hle ? "true" : "false",
                                fmt::format(
                                    "0x{:08X}",
                                    fmt::join(record.second.untranslated_request_cmdbuf, ", 0x"))
                                    .c_str(),
                                fmt::format(
                                    "0x{:08X}",
                                    fmt::join(record.second.translated_request_cmdbuf, ", 0x"))
                                    .c_str(),
                                fmt::format(
                                    "0x{:08X}",
                                    fmt::join(record.second.untranslated_reply_cmdbuf, ", 0x"))
                                    .c_str(),
                                fmt::format(
                                    "0x{:08X}",
                                    fmt::join(record.second.translated_reply_cmdbuf, ", 0x"))
                                    .c_str());
                        }
                    }
                }
                ImGui::ListBoxFooter();
            }
        }
        if (!show_ipc_recorder_window) {
            ipc_recorder_enabled = false;

            IPCDebugger::Recorder& r = Core::System::GetInstance().Kernel().GetIPCRecorder();

            r.SetEnabled(ipc_recorder_enabled);
            r.UnbindCallback(ipc_recorder_callback);
            ipc_recorder_callback = nullptr;
        }
        ImGui::End();
    }

    if (show_cheats_window) {
        if (ImGui::Begin("Cheats", &show_cheats_window, ImGuiWindowFlags_NoSavedSettings)) {
            if (ImGui::Button("Edit File")) {
                const std::string filepath = fmt::format(
                    "{}{:016X}.txt", FileUtil::GetUserPath(FileUtil::UserPath::CheatsDir),
                    system.Kernel().GetCurrentProcess()->codeset->program_id);

                FileUtil::CreateFullPath(filepath);

                if (!FileUtil::Exists(filepath)) {
                    FileUtil::CreateEmptyFile(filepath);
                }

#ifdef _WIN32
                const int code = std::system(fmt::format("start {}", filepath).c_str());
#else
                const int code = std::system(fmt::format("xdg-open {}", filepath).c_str());
#endif
                LOG_INFO(Frontend, "Opened cheats file in text editor, exit code: {}", code);
            }

            ImGui::SameLine();

            if (ImGui::Button("Reload File")) {
                system.CheatEngine().LoadCheatFile();
            }

            if (ImGui::ListBoxHeader("##cheats", ImVec2(-1.0f, -1.0f))) {
                for (const auto& cheat : system.CheatEngine().GetCheats()) {
                    bool enabled = cheat->IsEnabled();
                    if (ImGui::Checkbox(cheat->GetName().c_str(), &enabled)) {
                        cheat->SetEnabled(enabled);
                    }
                }
                ImGui::ListBoxFooter();
            }
        }

        ImGui::End();
    }

    if (disk_shader_cache_loading_progress != -1.0f) {
        if (ImGui::Begin("Loading Shaders", nullptr,
                         ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::ProgressBar(disk_shader_cache_loading_progress, ImVec2(0.0f, 0.0f));
        }
        ImGui::End();
    }

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    SDL_GL_SwapWindow(render_window);
}

void EmuWindow_SDL2::PollEvents() {
    SDL_Event event;

    // SDL_PollEvent returns 0 when there are no more events in the event queue
    while (SDL_PollEvent(&event)) {
        ImGui_ImplSDL2_ProcessEvent(&event);

        switch (event.type) {
        case SDL_WINDOWEVENT:
            switch (event.window.event) {
            case SDL_WINDOWEVENT_SIZE_CHANGED:
            case SDL_WINDOWEVENT_RESIZED:
            case SDL_WINDOWEVENT_MAXIMIZED:
            case SDL_WINDOWEVENT_RESTORED:
            case SDL_WINDOWEVENT_MINIMIZED:
                OnResize();
                break;
            case SDL_WINDOWEVENT_CLOSE:
                is_open = false;
                break;
            }
            break;
        case SDL_KEYDOWN:
        case SDL_KEYUP:
            // Ignore it if a Dear ImGui window is focused
            if (ImGui::IsWindowFocused(ImGuiFocusedFlags_AnyWindow)) {
                return;
            }

            OnKeyEvent(static_cast<int>(event.key.keysym.scancode), event.key.state);
            break;
        case SDL_MOUSEMOTION:
            // Ignore it if a Dear ImGui window is focused
            if (ImGui::IsWindowFocused(ImGuiFocusedFlags_AnyWindow)) {
                return;
            }

            // Ignore if it came from touch
            if (event.button.which != SDL_TOUCH_MOUSEID) {
                OnMouseMotion(event.motion.x, event.motion.y);
            }

            break;
        case SDL_MOUSEBUTTONDOWN:
        case SDL_MOUSEBUTTONUP:
            // Ignore it if a Dear ImGui window is focused
            if (ImGui::IsWindowFocused(ImGuiFocusedFlags_AnyWindow)) {
                return;
            }

            // Ignore if it came from touch
            if (event.button.which != SDL_TOUCH_MOUSEID) {
                OnMouseButton(event.button.button, event.button.state, event.button.x,
                              event.button.y);
            }

            break;
        case SDL_FINGERDOWN:
            // Ignore it if a Dear ImGui window is focused
            if (ImGui::IsWindowFocused(ImGuiFocusedFlags_AnyWindow)) {
                return;
            }

            OnFingerDown(event.tfinger.x, event.tfinger.y);
            break;
        case SDL_FINGERMOTION:
            // Ignore it if a Dear ImGui window is focused
            if (ImGui::IsWindowFocused(ImGuiFocusedFlags_AnyWindow)) {
                return;
            }

            OnFingerMotion(event.tfinger.x, event.tfinger.y);
            break;
        case SDL_FINGERUP:
            // Ignore it if a Dear ImGui window is focused
            if (ImGui::IsWindowFocused(ImGuiFocusedFlags_AnyWindow)) {
                return;
            }

            OnFingerUp();
            break;
        case SDL_QUIT:
            is_open = false;
            break;
        default:
            break;
        }
    }
}

void EmuWindow_SDL2::MakeCurrent() {
    SDL_GL_MakeCurrent(render_window, gl_context);
}

void EmuWindow_SDL2::DoneCurrent() {
    SDL_GL_MakeCurrent(render_window, nullptr);
}

void EmuWindow_SDL2::CopyScreenshot() {
    const auto& layout = GetFramebufferLayout();
    u8* data = new u8[layout.width * layout.height * 4];

    if (VideoCore::RequestScreenshot(
            data,
            [=] {
                std::vector<u8> v(layout.width * layout.height * 4);
                std::memcpy(v.data(), data, v.size());
                delete[] data;

                const auto rotate = [](const std::vector<u8>& input,
                                       const Layout::FramebufferLayout& layout) {
                    std::vector<u8> output(input.size());

                    for (std::size_t i = 0; i < layout.height; i++) {
                        for (std::size_t j = 0; j < layout.width; j++) {
                            for (std::size_t k = 0; k < 4; k++) {
                                output[i * (layout.width * 4) + j * 4 + k] =
                                    input[(layout.height - i - 1) * (layout.width * 4) + j * 4 + k];
                            }
                        }
                    }

                    return output;
                };

                const auto convert_bgra_to_rgba = [](const std::vector<u8>& input,
                                                     const Layout::FramebufferLayout& layout) {
                    int offset = 0;
                    std::vector<u8> output(input.size());

                    for (u32 y = 0; y < layout.height; ++y) {
                        for (u32 x = 0; x < layout.width; ++x) {
                            output[offset] = input[offset + 2];
                            output[offset + 1] = input[offset + 1];
                            output[offset + 2] = input[offset];
                            output[offset + 3] = input[offset + 3];

                            offset += 4;
                        }
                    }

                    return output;
                };

                v = convert_bgra_to_rgba(rotate(v, layout), layout);

                clip::image_spec spec;
                spec.width = layout.width;
                spec.height = layout.height;
                spec.bits_per_pixel = 32;
                spec.bytes_per_row = spec.width * 4;
                spec.red_mask = 0xff;
                spec.green_mask = 0xff00;
                spec.blue_mask = 0xff0000;
                spec.alpha_mask = 0xff000000;
                spec.red_shift = 0;
                spec.green_shift = 8;
                spec.blue_shift = 16;
                spec.alpha_shift = 24;

                clip::set_image(clip::image(v.data(), spec));
            },
            layout)) {
        delete[] data;
    }
}

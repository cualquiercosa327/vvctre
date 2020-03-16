// Copyright 2016 Citra Emulator Project
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
#include "video_core/renderer_opengl/texture_filters/texture_filter_manager.h"
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

void EmuWindow_SDL2::Fullscreen() {
    if (SDL_SetWindowFullscreen(render_window, SDL_WINDOW_FULLSCREEN_DESKTOP) == 0) {
        return;
    }

    LOG_ERROR(Frontend, "Fullscreening failed: {}", SDL_GetError());

    // Fallback algorithm: Maximise window.
    // Works on all systems (unless something is seriously wrong)
    LOG_INFO(Frontend, "Falling back on a maximised window...");
    SDL_MaximizeWindow(render_window);
}

EmuWindow_SDL2::EmuWindow_SDL2(Core::System& system, const bool fullscreen, const char* arg0)
    : system(system), arg0(arg0) {
    // Initialize the window
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK) < 0) {
        LOG_CRITICAL(Frontend, "Failed to initialize SDL2! Exiting...");
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

    if (fullscreen) {
        Fullscreen();
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

    if (!ImGui::IsWindowFocused(ImGuiFocusedFlags_AnyWindow)) {
        if (io.KeyCtrl && ImGui::IsKeyReleased(SDL_SCANCODE_L)) {
            const std::vector<std::string> result =
                pfd::open_file("Load File", ".",
                               {"3DS Executables", "*.cci *.3ds *.cxi *.3dsx *.app *.elf *.axf"})
                    .result();

            if (!result.empty()) {
                system.SetResetFilePath(result[0]);
                system.RequestReset();
            }
        }

        if (io.KeyCtrl && ImGui::IsKeyReleased(SDL_SCANCODE_R)) {
            system.RequestReset();
        }

        if (io.KeyCtrl && ImGui::IsKeyReleased(SDL_SCANCODE_Z)) {
            Settings::values.use_frame_limit = !Settings::values.use_frame_limit;
            Settings::LogSettings();
        }

        if (io.KeyCtrl && ImGui::IsKeyReleased(SDL_SCANCODE_D)) {
            Settings::values.dump_textures = !Settings::values.dump_textures;
            messages.push_back(fmt::format(
                "Dump Textures {}", Settings::values.dump_textures ? "enabled" : "disabled"));
        }

        if (ImGui::IsKeyPressed(SDL_SCANCODE_KP_MINUS)) {
            Settings::values.frame_limit = std::clamp(Settings::values.frame_limit - 5, 1, 65535);
            Settings::LogSettings();
        }

        if (ImGui::IsKeyPressed(SDL_SCANCODE_KP_PLUS)) {
            Settings::values.frame_limit = std::clamp(Settings::values.frame_limit + 5, 1, 65535);
            Settings::LogSettings();
        }

        if (ImGui::IsKeyReleased(SDL_SCANCODE_F1)) {
            const auto result =
                pfd::open_file("Load Amiibo", ".", {"Amiibo Files", "*.bin", "Anything", "*"})
                    .result();

            if (!result.empty()) {
                FileUtil::IOFile file(result[0], "rb");
                Service::NFC::AmiiboData data;
                if (file.ReadArray(&data, 1) == 1) {
                    std::shared_ptr<Service::NFC::Module::Interface> nfc =
                        system.ServiceManager().GetService<Service::NFC::Module::Interface>(
                            "nfc:u");
                    if (nfc != nullptr) {
                        nfc->LoadAmiibo(data);
                    }
                } else {
                    messages.push_back("Failed to load the amiibo file");
                }
            }
        }

        if (ImGui::IsKeyReleased(SDL_SCANCODE_F2)) {
            std::shared_ptr<Service::NFC::Module::Interface> nfc =
                system.ServiceManager().GetService<Service::NFC::Module::Interface>("nfc:u");
            if (nfc != nullptr) {
                nfc->RemoveAmiibo();
            }
        }

        if (io.KeyCtrl && ImGui::IsKeyReleased(SDL_SCANCODE_A)) {
            Settings::values.resolution_factor = 0;
            Settings::LogSettings();
        }

        if (io.KeyCtrl && ImGui::IsKeyReleased(SDL_SCANCODE_1)) {
            Settings::values.resolution_factor = 1;
            Settings::LogSettings();
        }

        if (io.KeyCtrl && ImGui::IsKeyReleased(SDL_SCANCODE_2)) {
            Settings::values.resolution_factor = 2;
            Settings::LogSettings();
        }

        if (io.KeyCtrl && ImGui::IsKeyReleased(SDL_SCANCODE_3)) {
            Settings::values.resolution_factor = 3;
            Settings::LogSettings();
        }

        if (io.KeyCtrl && ImGui::IsKeyReleased(SDL_SCANCODE_4)) {
            Settings::values.resolution_factor = 4;
            Settings::LogSettings();
        }

        if (io.KeyCtrl && ImGui::IsKeyReleased(SDL_SCANCODE_5)) {
            Settings::values.resolution_factor = 5;
            Settings::LogSettings();
        }

        if (io.KeyCtrl && ImGui::IsKeyReleased(SDL_SCANCODE_6)) {
            Settings::values.resolution_factor = 6;
            Settings::LogSettings();
        }

        if (io.KeyCtrl && ImGui::IsKeyReleased(SDL_SCANCODE_7)) {
            Settings::values.resolution_factor = 7;
            Settings::LogSettings();
        }

        if (io.KeyCtrl && ImGui::IsKeyReleased(SDL_SCANCODE_8)) {
            Settings::values.resolution_factor = 8;
            Settings::LogSettings();
        }

        if (io.KeyCtrl && ImGui::IsKeyReleased(SDL_SCANCODE_9)) {
            Settings::values.resolution_factor = 9;
            Settings::LogSettings();
        }

        if (io.KeyCtrl && ImGui::IsKeyReleased(SDL_SCANCODE_0)) {
            Settings::values.resolution_factor = 10;
            Settings::LogSettings();
        }
    }

    if (ImGui::Begin("FPS", nullptr,
                     ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings |
                         ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::SetWindowPos(ImVec2(), ImGuiCond_Once);
        ImGui::TextColored(fps_color, "%d FPS", static_cast<int>(ImGui::GetIO().Framerate));
        if (ImGui::IsWindowFocused() && ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
            ImGui::ColorPicker4("##picker", (float*)&fps_color);
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

    if (!messages.empty()) {
        ImGui::OpenPopup("Messages");
    }

    ImGui::SetNextWindowSize(ImVec2(250.0f, 100.0f), ImGuiCond_Appearing);
    if (ImGui::BeginPopupModal("Messages")) {
        for (const auto& message : messages) {
            ImGui::TextWrapped("%s\n\n", message.c_str());
        }
        if (ImGui::Button("OK")) {
            messages.clear();
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    if (ipc_recorder_enabled) {
        if (ImGui::Begin("IPC Recorder", nullptr, ImGuiWindowFlags_NoSavedSettings)) {
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
        ImGui::End();
    }

    if (show_cheats_window) {
        if (ImGui::Begin("Cheats", nullptr, ImGuiWindowFlags_NoSavedSettings)) {
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
    } else {
        if (ImGui::BeginPopupContextVoid(nullptr, ImGuiMouseButton_Middle)) {
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
                        for (const auto& file : files) {
                            Service::AM::InstallCIA(file);
                        }

                        auto am = Service::AM::GetModule(system);
                        if (am != nullptr) {
                            am->ScanForAllTitles();
                        }

                        messages.push_back("CIA installation finished");
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
                                messages.push_back("Failed to load the amiibo file");
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
                    if (ImGui::MenuItem("Limit Speed", nullptr,
                                        &Settings::values.use_frame_limit)) {
                        Settings::LogSettings();
                    }

                    ImGui::Text("Speed Limit");
                    ImGui::SameLine();
                    const u16 min = 1;
                    const u16 max = 500;
                    if (ImGui::SliderScalar("##speedlimit", ImGuiDataType_U16,
                                            &Settings::values.frame_limit, &min, &max)) {
                        Settings::LogSettings();
                    }

                    ImGui::EndMenu();
                }

                if (ImGui::BeginMenu("Audio")) {
                    ImGui::Text("Volume");
                    ImGui::SameLine();
                    if (ImGui::SliderFloat("##volume", &Settings::values.volume, 0.0f, 1.0f)) {
                        Settings::LogSettings();
                    }

                    ImGui::Text("Speed");
                    ImGui::SameLine();
                    if (ImGui::SliderFloat("##speed", &Settings::values.audio_speed, 0.001f,
                                           5.0f)) {
                        Settings::LogSettings();
                    }

                    ImGui::EndMenu();
                }

                if (ImGui::BeginMenu("Graphics")) {
                    if (ImGui::MenuItem("Use Hardware Renderer", nullptr,
                                        &Settings::values.use_hw_renderer)) {
                        Settings::Apply();
                        Settings::LogSettings();
                    }
                    ImGui::Indent();

                    if (ImGui::MenuItem("Use Hardware Shader", nullptr,
                                        &Settings::values.use_hw_shader)) {
                        Settings::Apply();
                        Settings::LogSettings();
                    }
                    ImGui::Indent();

                    if (ImGui::MenuItem("Use Accurate Multiplication", nullptr,
                                        &Settings::values.shaders_accurate_mul)) {
                        Settings::Apply();
                        Settings::LogSettings();
                    }

                    ImGui::Unindent();
                    ImGui::Unindent();

                    if (ImGui::MenuItem("Use Shader JIT", nullptr,
                                        &Settings::values.use_shader_jit)) {
                        Settings::LogSettings();
                    }

                    if (ImGui::MenuItem("Enable VSync", nullptr, &Settings::values.enable_vsync)) {
                        Settings::LogSettings();
                    }

                    ImGui::Text("Resolution");
                    ImGui::SameLine();
                    const u16 min = 0;
                    const u16 max = 10;
                    if (ImGui::SliderScalar("##resolution", ImGuiDataType_U16,
                                            &Settings::values.resolution_factor, &min, &max,
                                            Settings::values.resolution_factor == 0 ? "Window Size"
                                                                                    : "%d")) {
                        Settings::LogSettings();
                    }

                    ImGui::Text("Background Color");
                    ImGui::SameLine();
                    if (ImGui::ColorEdit3("##backgroundcolor", &Settings::values.bg_red,
                                          ImGuiColorEditFlags_NoInputs)) {
                        VideoCore::g_renderer_bg_color_update_requested = true;
                        Settings::LogSettings();
                    }

                    ImGui::Text("Texture Filter Name");
                    ImGui::SameLine();
                    if (ImGui::BeginCombo("##texturefiltername",
                                          Settings::values.texture_filter_name.c_str())) {
                        const auto& filters = OpenGL::TextureFilterManager::TextureFilterMap();

                        for (const auto& filter : filters) {
                            if (ImGui::Selectable(std::string(filter.first).c_str())) {
                                Settings::values.texture_filter_name = filter.first;
                                Settings::Apply();
                                Settings::LogSettings();
                            }
                        }

                        ImGui::EndCombo();
                    }

                    ImGui::Text("Texture Filter Factor");
                    ImGui::SameLine();
                    if (ImGui::InputScalar("##texturefilterfactor", ImGuiDataType_U16,
                                           &Settings::values.texture_filter_factor)) {
                        Settings::Apply();
                        Settings::LogSettings();
                    }

                    if (ImGui::Checkbox("Dump Textures", &Settings::values.dump_textures)) {
                        Settings::LogSettings();
                    }

                    ImGui::EndMenu();
                }

                if (ImGui::BeginMenu("Camera")) {
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
                            auto cam = Service::CAM::GetModule(system);
                            if (cam != nullptr) {
                                cam->ReloadCameraDevices();
                            }
                        }
                        if (ImGui::Selectable("image")) {
                            Settings::values.camera_name[static_cast<std::size_t>(
                                Service::CAM::CameraIndex::InnerCamera)] = "image";
                            auto cam = Service::CAM::GetModule(system);
                            if (cam != nullptr) {
                                cam->ReloadCameraDevices();
                            }
                        }
                        ImGui::EndCombo();
                    }

                    ImGui::Text("Inner Camera Configuration");
                    ImGui::SameLine();
                    if (ImGui::InputText("##innerconfiguration",
                                         &Settings::values.camera_config[static_cast<std::size_t>(
                                             Service::CAM::CameraIndex::InnerCamera)])) {
                        auto cam = Service::CAM::GetModule(system);
                        if (cam != nullptr) {
                            cam->ReloadCameraDevices();
                        }
                    }

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
                            auto cam = Service::CAM::GetModule(system);
                            if (cam != nullptr) {
                                cam->ReloadCameraDevices();
                            }
                        }
                        if (ImGui::Selectable("image")) {
                            Settings::values.camera_name[static_cast<std::size_t>(
                                Service::CAM::CameraIndex::OuterLeftCamera)] = "image";
                            auto cam = Service::CAM::GetModule(system);
                            if (cam != nullptr) {
                                cam->ReloadCameraDevices();
                            }
                        }
                        ImGui::EndCombo();
                    }

                    ImGui::Text("Outer Left Configuration");
                    ImGui::SameLine();
                    if (ImGui::InputText("##outerleftconfiguration",
                                         &Settings::values.camera_config[static_cast<std::size_t>(
                                             Service::CAM::CameraIndex::OuterLeftCamera)])) {
                        auto cam = Service::CAM::GetModule(system);
                        if (cam != nullptr) {
                            cam->ReloadCameraDevices();
                        }
                    }

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
                            auto cam = Service::CAM::GetModule(system);
                            if (cam != nullptr) {
                                cam->ReloadCameraDevices();
                            }
                        }
                        if (ImGui::Selectable("image")) {
                            Settings::values.camera_name[static_cast<std::size_t>(
                                Service::CAM::CameraIndex::OuterRightCamera)] = "image";
                            auto cam = Service::CAM::GetModule(system);
                            if (cam != nullptr) {
                                cam->ReloadCameraDevices();
                            }
                        }
                        ImGui::EndCombo();
                    }

                    ImGui::Text("Outer Right Configuration");
                    ImGui::SameLine();
                    if (ImGui::InputText("##outerrightconfiguration",
                                         &Settings::values.camera_config[static_cast<std::size_t>(
                                             Service::CAM::CameraIndex::OuterRightCamera)])) {
                        auto cam = Service::CAM::GetModule(system);
                        if (cam != nullptr) {
                            cam->ReloadCameraDevices();
                        }
                    }

                    ImGui::EndMenu();
                }

                if (ImGui::BeginMenu("Hacks")) {
                    if (ImGui::Checkbox("Ignore Format Reinterpretation",
                                        &Settings::values.ignore_format_reinterpretation)) {
                        Settings::LogSettings();
                    }
                    if (ImGui::IsItemHovered()) {
                        ImGui::BeginTooltip();
                        ImGui::Text("Ignore flushing surfaces from CPU memory if the surface was "
                                    "created by the GPU and has a different format.");
                        ImGui::Text("This can speed up many games, potentially break some, but is "
                                    "rightfully just a hack as a placeholder for GPU texture "
                                    "encoding/decoding");
                        ImGui::EndTooltip();
                    }
                    ImGui::EndMenu();
                }

                if (ImGui::BeginMenu("System (persistent)")) {
                    auto cfg = Service::CFG::GetModule(system);

                    if (cfg != nullptr) {
                        ImGui::Text("Username (changing will restart emulation)");
                        ImGui::SameLine();

                        std::string username = Common::UTF16ToUTF8(cfg->GetUsername());
                        if (ImGui::InputText("##username", &username)) {
                            cfg->SetUsername(Common::UTF8ToUTF16(username));
                            cfg->UpdateConfigNANDSavegame();
                            system.RequestReset();
                        }

                        ImGui::Text("Birthday (changing will restart emulation)");
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

                        if (ImGui::BeginMenu("Language (changing will restart emulation)")) {
                            const Service::CFG::SystemLanguage language = cfg->GetSystemLanguage();

                            if (ImGui::RadioButton("Japanese",
                                                   language ==
                                                       Service::CFG::SystemLanguage::LANGUAGE_JP)) {
                                cfg->SetSystemLanguage(Service::CFG::SystemLanguage::LANGUAGE_JP);
                                cfg->UpdateConfigNANDSavegame();
                                system.RequestReset();
                            }

                            if (ImGui::RadioButton("English",
                                                   language ==
                                                       Service::CFG::SystemLanguage::LANGUAGE_EN)) {
                                cfg->SetSystemLanguage(Service::CFG::SystemLanguage::LANGUAGE_EN);
                                cfg->UpdateConfigNANDSavegame();
                                system.RequestReset();
                            }

                            if (ImGui::RadioButton("French",
                                                   language ==
                                                       Service::CFG::SystemLanguage::LANGUAGE_FR)) {
                                cfg->SetSystemLanguage(Service::CFG::SystemLanguage::LANGUAGE_FR);
                                cfg->UpdateConfigNANDSavegame();
                                system.RequestReset();
                            }

                            if (ImGui::RadioButton("German",
                                                   language ==
                                                       Service::CFG::SystemLanguage::LANGUAGE_DE)) {
                                cfg->SetSystemLanguage(Service::CFG::SystemLanguage::LANGUAGE_DE);
                                cfg->UpdateConfigNANDSavegame();
                                system.RequestReset();
                            }

                            if (ImGui::RadioButton("Italian",
                                                   language ==
                                                       Service::CFG::SystemLanguage::LANGUAGE_IT)) {
                                cfg->SetSystemLanguage(Service::CFG::SystemLanguage::LANGUAGE_IT);
                                cfg->UpdateConfigNANDSavegame();
                                system.RequestReset();
                            }

                            if (ImGui::RadioButton("Spanish",
                                                   language ==
                                                       Service::CFG::SystemLanguage::LANGUAGE_ES)) {
                                cfg->SetSystemLanguage(Service::CFG::SystemLanguage::LANGUAGE_ES);
                                cfg->UpdateConfigNANDSavegame();
                                system.RequestReset();
                            }

                            if (ImGui::RadioButton("Simplified Chinese",
                                                   language ==
                                                       Service::CFG::SystemLanguage::LANGUAGE_ZH)) {
                                cfg->SetSystemLanguage(Service::CFG::SystemLanguage::LANGUAGE_ZH);
                                cfg->UpdateConfigNANDSavegame();
                                system.RequestReset();
                            }

                            if (ImGui::RadioButton("Korean",
                                                   language ==
                                                       Service::CFG::SystemLanguage::LANGUAGE_KO)) {
                                cfg->SetSystemLanguage(Service::CFG::SystemLanguage::LANGUAGE_KO);
                                cfg->UpdateConfigNANDSavegame();
                                system.RequestReset();
                            }

                            if (ImGui::RadioButton("Dutch",
                                                   language ==
                                                       Service::CFG::SystemLanguage::LANGUAGE_NL)) {
                                cfg->SetSystemLanguage(Service::CFG::SystemLanguage::LANGUAGE_NL);
                                cfg->UpdateConfigNANDSavegame();
                                system.RequestReset();
                            }

                            if (ImGui::RadioButton("Portugese",
                                                   language ==
                                                       Service::CFG::SystemLanguage::LANGUAGE_PT)) {
                                cfg->SetSystemLanguage(Service::CFG::SystemLanguage::LANGUAGE_PT);
                                cfg->UpdateConfigNANDSavegame();
                                system.RequestReset();
                            }

                            if (ImGui::RadioButton("Russian",
                                                   language ==
                                                       Service::CFG::SystemLanguage::LANGUAGE_RU)) {
                                cfg->SetSystemLanguage(Service::CFG::SystemLanguage::LANGUAGE_RU);
                                cfg->UpdateConfigNANDSavegame();
                                system.RequestReset();
                            }

                            if (ImGui::RadioButton("Traditional Chinese",
                                                   language ==
                                                       Service::CFG::SystemLanguage::LANGUAGE_TW)) {
                                cfg->SetSystemLanguage(Service::CFG::SystemLanguage::LANGUAGE_TW);
                                cfg->UpdateConfigNANDSavegame();
                                system.RequestReset();
                            }

                            ImGui::EndMenu();
                        }
                    }

                    ImGui::Text("Play Coins (may need to restart emulation)");
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

                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("View")) {
                ImGui::MenuItem("Cheats", nullptr, &show_cheats_window);

                if (ImGui::BeginMenu("Layout")) {
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

                    ImGui::Separator();

                    if (ImGui::MenuItem("Swap Screens", nullptr, &Settings::values.swap_screen)) {
                        Settings::Apply();
                        Settings::LogSettings();
                    }

                    if (ImGui::MenuItem("Upright Orientation", nullptr,
                                        &Settings::values.upright_screen)) {
                        Settings::Apply();
                        Settings::LogSettings();
                    }

                    ImGui::EndMenu();
                }

                if (ImGui::BeginMenu("Debugging")) {
                    if (ImGui::MenuItem("IPC Recorder", nullptr, &ipc_recorder_enabled)) {
                        ipc_records.clear();
                        auto& r = Core::System::GetInstance().Kernel().GetIPCRecorder();
                        r.SetEnabled(ipc_recorder_enabled);
                        if (ipc_recorder_enabled) {
                            ipc_recorder_callback =
                                r.BindCallback([&](const IPCDebugger::RequestRecord& record) {
                                    ipc_records[record.id] = record;
                                });
                        } else {
                            r.UnbindCallback(ipc_recorder_callback);
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
                if (ImGui::MenuItem("Screenshot")) {
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

                if (ImGui::MenuItem("Generate Launcher For Custom Controls")) {
#ifdef _WIN32
                    const std::string command =
                        fmt::format("cmd /c \"{}\" controls --generate-launcher", arg0);
#else
                    const std::string command = fmt::format(
                        "x-terminal-emulator -e \"\\\"{}\\\" controls --generate-launcher\"", arg0);
#endif
                    const int code = std::system(command.c_str());
                    LOG_INFO(Frontend, "{} exited with {}", command, code);
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
                                movie.StartPlayback(
                                    filename[0], [&] { messages.push_back("Playback finished"); });
                                break;
                            case Core::Movie::ValidationResult::GameDismatch:
                                messages.push_back(
                                    "Movie was recorded using a ROM with a different program ID");
                                movie.StartPlayback(
                                    filename[0], [&] { messages.push_back("Playback finished"); });
                                break;
                            case Core::Movie::ValidationResult::Invalid:
                                messages.push_back("Movie file doesn't have a valid header");
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
                        messages.push_back("Movie saved");
                    }

                    ImGui::EndMenu();
                }

                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Help")) {
                if (ImGui::MenuItem("Discord Server")) {
#ifdef _WIN32
                    const int code = std::system("start https://discord.gg/RNBCBzT");
#else
                    const int code = std::system("xdg-open https://discord.gg/RNBCBzT");
#endif
                    LOG_INFO(Frontend, "Opened Discord invite, exit code: {}", code);
                }

                if (ImGui::MenuItem("Open Data Folder")) {
#ifdef _WIN32
                    const int code = std::system(
                        fmt::format("start {}", FileUtil::GetUserPath(FileUtil::UserPath::UserDir))
                            .c_str());
#else
                    const int code =
                        std::system(fmt::format("xdg-open {}",
                                                FileUtil::GetUserPath(FileUtil::UserPath::UserDir))
                                        .c_str());
#endif
                    LOG_INFO(Frontend, "Opened data folder, exit code: {}", code);
                }

                ImGui::EndMenu();
            }

            ImGui::EndPopup();
        } else {
            system.frontend_paused = false;
        }
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
            // ignore it if a Dear ImGui window is focused
            if (ImGui::IsWindowFocused(ImGuiFocusedFlags_AnyWindow)) {
                return;
            }

            OnKeyEvent(static_cast<int>(event.key.keysym.scancode), event.key.state);
            break;
        case SDL_MOUSEMOTION:
            // ignore it if a Dear ImGui window is focused
            if (ImGui::IsWindowFocused(ImGuiFocusedFlags_AnyWindow)) {
                return;
            }

            // ignore if it came from touch
            if (event.button.which != SDL_TOUCH_MOUSEID) {
                OnMouseMotion(event.motion.x, event.motion.y);
            }

            break;
        case SDL_MOUSEBUTTONDOWN:
        case SDL_MOUSEBUTTONUP:
            // ignore it if a Dear ImGui window is focused
            if (ImGui::IsWindowFocused(ImGuiFocusedFlags_AnyWindow)) {
                return;
            }

            // ignore if it came from touch
            if (event.button.which != SDL_TOUCH_MOUSEID) {
                OnMouseButton(event.button.button, event.button.state, event.button.x,
                              event.button.y);
            }

            break;
        case SDL_FINGERDOWN:
            // ignore it if a Dear ImGui window is focused
            if (ImGui::IsWindowFocused(ImGuiFocusedFlags_AnyWindow)) {
                return;
            }

            OnFingerDown(event.tfinger.x, event.tfinger.y);
            break;
        case SDL_FINGERMOTION:
            // ignore it if a Dear ImGui window is focused
            if (ImGui::IsWindowFocused(ImGuiFocusedFlags_AnyWindow)) {
                return;
            }

            OnFingerMotion(event.tfinger.x, event.tfinger.y);
            break;
        case SDL_FINGERUP:
            // ignore it if a Dear ImGui window is focused
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

    const u64 current_program_id = system.Kernel().GetCurrentProcess()->codeset->program_id;
    if (program_id != current_program_id) {
        system.GetAppLoader().ReadTitle(program_name);

        const std::string window_title =
            fmt::format("vvctre {} | {}", version::vvctre.to_string(), program_name);

        SDL_SetWindowTitle(render_window, window_title.c_str());

        program_id = current_program_id;
    }
}

void EmuWindow_SDL2::MakeCurrent() {
    SDL_GL_MakeCurrent(render_window, gl_context);
}

void EmuWindow_SDL2::DoneCurrent() {
    SDL_GL_MakeCurrent(render_window, nullptr);
}

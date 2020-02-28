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
#include "core/core.h"
#include "core/hle/service/am/am.h"
#include "core/hle/service/nfc/nfc.h"
#include "core/settings.h"
#include "input_common/keyboard.h"
#include "input_common/main.h"
#include "input_common/motion_emu.h"
#include "input_common/sdl/sdl.h"
#include "video_core/renderer_base.h"
#include "video_core/video_core.h"
#include "vvctre/emu_window/emu_window_sdl2.h"

#ifdef USE_DISCORD_PRESENCE
#include "vvctre/discord_rp.h"
#endif

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
    if (SDL_SetWindowFullscreen(render_window, SDL_WINDOW_FULLSCREEN) == 0) {
        return;
    }

    LOG_ERROR(Frontend, "Fullscreening failed: {}", SDL_GetError());

    // Try a different fullscreening method
    LOG_INFO(Frontend, "Attempting to use borderless fullscreen...");
    if (SDL_SetWindowFullscreen(render_window, SDL_WINDOW_FULLSCREEN_DESKTOP) == 0) {
        return;
    }

    LOG_ERROR(Frontend, "Borderless fullscreening failed: {}", SDL_GetError());

    // Fallback algorithm: Maximise window.
    // Works on all systems (unless something is seriously wrong), so no fallback for this one.
    LOG_INFO(Frontend, "Falling back on a maximised window...");
    SDL_MaximizeWindow(render_window);
}

EmuWindow_SDL2::EmuWindow_SDL2(Core::System& system, bool fullscreen) : system(system) {
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
            if (ImGui::ListBoxHeader("")) {
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

    if (show_failed_to_read_the_file) {
        ImGui::OpenPopup("Error##FailedToReadTheFile");
    }

    if (ImGui::BeginPopupModal("Error##FailedToReadTheFile", &show_failed_to_read_the_file)) {
        ImGui::Text("Failed to read the file");
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
                    if (system.IsPoweredOn() && record.second.client_port.id != -1) {
                        service_name = system.ServiceManager().GetServiceNameByPortId(
                            static_cast<u32>(record.second.client_port.id));
                    }
                    if (service_name.empty()) {
                        service_name = record.second.server_session.name;
                        service_name = Common::ReplaceAll(service_name, "_Server", "");
                        service_name = Common::ReplaceAll(service_name, "_Client", "");
                    }
                    const std::string label = fmt::format(
                        "#{} - {} - {} (0x{:08X}) ({}) ({})", record.first, service_name,
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
                            ImGui::SetTooltip("ID: %d\nStatus: %d", record.first,
                                              static_cast<int>(record.second.status));
                        }
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
    } else if (ImGui::BeginPopupContextVoid(nullptr, ImGuiMouseButton_Middle)) {
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
                const std::vector<std::string> result =
                    pfd::open_file("Install CIA", ".", {"CTR Importable Archive", "*.cia"})
                        .result();

                if (!result.empty()) {
                    Service::AM::InstallCIA(result[0]);
                }
            }

            if (ImGui::BeginMenu("Amiibo")) {
                if (ImGui::MenuItem("Load")) {
                    const auto result = pfd::open_file("Load Amiibo", ".",
                                                       {"Amiibo Files", "*.bin", "Anything", "*"})
                                            .result();

                    if (!result.empty()) {
                        FileUtil::IOFile file(result[0], "rb");
                        Service::NFC::AmiiboData data;
                        if (file.ReadArray(&data, 1) == 1) {
                            std::shared_ptr<Service::NFC::Module::Interface> nfc =
                                system.ServiceManager().GetService<Service::NFC::Module::Interface>(
                                    "nfc:u");
                            nfc->LoadAmiibo(data);
                        } else {
                            show_failed_to_read_the_file = true;
                        }
                    }
                }

                if (ImGui::MenuItem("Remove")) {
                    std::shared_ptr<Service::NFC::Module::Interface> nfc =
                        system.ServiceManager().GetService<Service::NFC::Module::Interface>(
                            "nfc:u");
                    nfc->RemoveAmiibo();
                }

                ImGui::EndMenu();
            }

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("View")) {
            if (ImGui::BeginMenu("Layout")) {
                if (ImGui::MenuItem("Default")) {
                    Settings::values.layout_option = Settings::LayoutOption::Default;
                    Settings::Apply();
                }

                if (ImGui::MenuItem("Single Screen")) {
                    Settings::values.layout_option = Settings::LayoutOption::SingleScreen;
                    Settings::Apply();
                }

                if (ImGui::MenuItem("Large Screen")) {
                    Settings::values.layout_option = Settings::LayoutOption::LargeScreen;
                    Settings::Apply();
                }

                if (ImGui::MenuItem("Side by Side")) {
                    Settings::values.layout_option = Settings::LayoutOption::SideScreen;
                    Settings::Apply();
                }

                if (ImGui::MenuItem("Medium Screen")) {
                    Settings::values.layout_option = Settings::LayoutOption::MediumScreen;
                    Settings::Apply();
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
            if (ImGui::MenuItem("Continue/Pause")) {
                if (system.GetStatus() == Core::System::ResultStatus::Paused) {
                    system.SetStatus(Core::System::ResultStatus::Success);
                } else {
                    system.SetStatus(Core::System::ResultStatus::Paused);
                }
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

                                const auto rotate = [](const std::vector<u8>& input,
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

            ImGui::EndMenu();
        }

        ImGui::EndPopup();
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

    if (system.IsPoweredOn()) {
        const u64 current_program_id = system.Kernel().GetCurrentProcess()->codeset->program_id;
        if (program_id != current_program_id) {
            system.GetAppLoader().ReadTitle(program_name);

#ifdef USE_DISCORD_PRESENCE
            if (discord_rp == nullptr) {
                discord_rp = std::make_unique<DiscordRP>(program_name);
            } else {
                discord_rp->Update(program_name);
            }
#endif

            const std::string window_title =
                fmt::format("vvctre {} | {}", version::vvctre.to_string(), program_name);

            SDL_SetWindowTitle(render_window, window_title.c_str());

            program_id = current_program_id;
        }
    }
}

void EmuWindow_SDL2::MakeCurrent() {
    SDL_GL_MakeCurrent(render_window, gl_context);
}

void EmuWindow_SDL2::DoneCurrent() {
    SDL_GL_MakeCurrent(render_window, nullptr);
}

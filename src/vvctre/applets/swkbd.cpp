// Copyright 2020 vvctre emulator project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "video_core/renderer_opengl/renderer_opengl.h"
#include "vvctre/applets/swkbd.h"
#include "vvctre/emu_window/emu_window_sdl2.h"

#include <imgui_stdlib.h>

namespace Frontend {

SDL2_SoftwareKeyboard::SDL2_SoftwareKeyboard(EmuWindow_SDL2& emu_window) : emu_window(emu_window) {}

void SDL2_SoftwareKeyboard::Execute(const KeyboardConfig& config) {
    SoftwareKeyboard::Execute(config);

    std::atomic<bool> done{false};
    std::string text;
    u8 button = 0;
    ImGuiIO& io = ImGui::GetIO();

    EmuWindow_SDL2::WindowCallback cb;
    cb.function = [&] {
        ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f),
                                ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        if (ImGui::Begin("Keyboard", nullptr,
                         ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::InputTextWithHint("", config.hint_text.c_str(), &text);

            switch (config.button_config) {
            case ButtonConfig::None:
            case ButtonConfig::Single: {
                if (ImGui::Button(
                        (config.button_text[0].empty() ? SWKBD_BUTTON_OKAY : config.button_text[0])
                            .c_str())) {
                    done = true;
                }
                break;
            }

            case ButtonConfig::Dual: {
                const std::string cancel =
                    (config.button_text.size() < 1 || config.button_text[0].empty())
                        ? SWKBD_BUTTON_CANCEL
                        : config.button_text[0];
                const std::string ok =
                    (config.button_text.size() < 2 || config.button_text[1].empty())
                        ? SWKBD_BUTTON_OKAY
                        : config.button_text[1];
                if (ImGui::Button(cancel.c_str())) {
                    done = true;
                }
                ImGui::SameLine();
                if (ImGui::Button(ok.c_str())) {
                    button = 1;
                    done = true;
                }
                break;
            }

            case ButtonConfig::Triple: {
                const std::string cancel =
                    (config.button_text.size() < 1 || config.button_text[0].empty())
                        ? SWKBD_BUTTON_CANCEL
                        : config.button_text[0];
                const std::string forgot =
                    (config.button_text.size() < 2 || config.button_text[1].empty())
                        ? SWKBD_BUTTON_FORGOT
                        : config.button_text[1];
                const std::string ok =
                    (config.button_text.size() < 3 || config.button_text[2].empty())
                        ? SWKBD_BUTTON_OKAY
                        : config.button_text[2];
                if (ImGui::Button(cancel.c_str())) {
                    done = true;
                }
                ImGui::SameLine();
                if (ImGui::Button(forgot.c_str())) {
                    button = 1;
                    done = true;
                }
                ImGui::SameLine();
                if (ImGui::Button(ok.c_str())) {
                    button = 2;
                    done = true;
                }
            }
            }

            ImGui::End();
        }
    };

    emu_window.windows.emplace("SDL2_SoftwareKeyboard", &cb);

    while (emu_window.IsOpen() && !done) {
        VideoCore::g_renderer->SwapBuffers();
    }

    Finalize(text, button);

    ValidationError error;
    while ((error = ValidateInput(text)) != ValidationError::None) {
        switch (error) {
        case ValidationError::AtSignNotAllowed: {
            LOG_ERROR(Applet_SWKBD, "@ not allowed");
            break;
        }

        case ValidationError::BackslashNotAllowed: {
            LOG_ERROR(Applet_SWKBD, "\\ not allowed");
            break;
        }

        case ValidationError::BlankInputNotAllowed: {
            LOG_ERROR(Applet_SWKBD, "Blank input not allowed");
            break;
        }

        case ValidationError::ButtonOutOfRange: {
            LOG_ERROR(Applet_SWKBD, "Button out of range");
            break;
        }

        case ValidationError::CallbackFailed: {
            LOG_ERROR(Applet_SWKBD, "Callback failed");
            break;
        }

        case ValidationError::MaxDigitsExceeded: {
            LOG_ERROR(Applet_SWKBD, "Max digits exceeded");
            break;
        }

        case ValidationError::EmptyInputNotAllowed: {
            LOG_ERROR(Applet_SWKBD, "Empty input not allowed");
            break;
        }

        case ValidationError::FixedLengthRequired: {
            LOG_ERROR(Applet_SWKBD, "Text length is not correct (should be {} characters)",
                      config.max_text_length);
            break;
        }

        case ValidationError::MaxLengthExceeded: {
            LOG_ERROR(Applet_SWKBD, "Text is too long (should be no more than {} characters)",
                      config.max_text_length);
            break;
        }

        case ValidationError::None: {
            break;
        }

        case ValidationError::PercentNotAllowed: {
            LOG_ERROR(Applet_SWKBD, "% not allowed");
            break;
        }

        case ValidationError::ProfanityNotAllowed: {
            LOG_ERROR(Applet_SWKBD, "Profanity not allowed");
            break;
        }
        }
    }
}

void SDL2_SoftwareKeyboard::ShowError(const std::string& error) {
    LOG_ERROR(Applet_SWKBD, "{}", error);
}

} // namespace Frontend

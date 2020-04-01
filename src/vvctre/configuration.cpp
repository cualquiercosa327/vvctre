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
#include <indicators/progress_bar.hpp>
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
#include "core/movie.h"
#include "core/settings.h"
#include "input_common/keyboard.h"
#include "input_common/main.h"
#include "input_common/motion_emu.h"
#include "input_common/sdl/sdl.h"
#include "video_core/renderer_base.h"
#include "video_core/video_core.h"
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
        if (ImGui::Begin("FPS", nullptr,
                         ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings |
                             ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::SetWindowPos(ImVec2(), ImGuiCond_Once);
            ImGui::Text("%d FPS", static_cast<int>(ImGui::GetIO().Framerate));
        }
        ImGui::End();
        ImGui::SetNextWindowSize(ImVec2(io.DisplaySize));
        if (ImGui::Begin("", nullptr,
                         ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDecoration |
                             ImGuiWindowFlags_NoMove)) {
            ImGui::SetWindowPos(ImVec2(), ImGuiCond_Once);
            if (ImGui::BeginTabBar("##tabBar")) {
                if (ImGui::BeginTabItem("Start")) {
                    // File
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

                    // GDB Stub
                    ImGui::Checkbox("Enable GDB Stub", &Settings::values.use_gdbstub);
                    if (Settings::values.use_gdbstub) {
                        ImGui::Text("GDB Stub Port:");
                        ImGui::SameLine();
                        ImGui::InputScalar("##gdbstubport", ImGuiDataType_U16,
                                           &Settings::values.gdbstub_port);
                    }

                    // Start in Fullscreen Mode
                    ImGui::Checkbox("Start in Fullscreen Mode",
                                    &Settings::values.start_in_fullscreen_mode);

                    // Play Movie
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

                    // Record Movie
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

                    ImGui::EndTabItem();
                }

                ImGui::EndTabBar();
            }

            // OK
            if (!Settings::values.file_path.empty() && ImGui::Button("OK")) {
                Settings::Apply();
                return;
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
    SDL_Quit();
}

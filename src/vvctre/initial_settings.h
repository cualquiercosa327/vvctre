// Copyright 2020 vvctre project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <string>
#include <vector>
#include <imgui.h>

struct SDL_Window;

class InitialSettings {
public:
    InitialSettings();
    ~InitialSettings();

    void Run();

private:
    using SDL_GLContext = void*;

    SDL_Window* render_window = nullptr;
    SDL_GLContext gl_context;

    bool update_config_savegame = false;
};

// Copyright 2020 vvctre project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <imgui.h>

struct SDL_Window;

class Configuration {
public:
    Configuration();
    ~Configuration();

    void Run();

private:
    using SDL_GLContext = void*;

    SDL_Window* render_window = nullptr;
    SDL_GLContext gl_context;
};

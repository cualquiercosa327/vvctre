// Copyright 2020 vvctre project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

class PluginManager;
struct SDL_Window;

class InitialSettings {
public:
    explicit InitialSettings(PluginManager& plugin_manager);
    ~InitialSettings();

    void Run();

private:
    using SDL_GLContext = void*;

    SDL_Window* render_window = nullptr;
    SDL_GLContext gl_context;

    bool update_config_savegame = false;

    PluginManager& plugin_manager;
};

// Copyright 2020 vvctre project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

class PluginManager;
struct SDL_Window;

namespace Service::CFG {
class Module;
} // namespace Service::CFG

class InitialSettings {
public:
    explicit InitialSettings(PluginManager& plugin_manager, SDL_Window* window,
                             Service::CFG::Module& cfg);

private:
    bool update_config_savegame = false;
};

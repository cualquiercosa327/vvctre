// Copyright 2020 vvctre project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <string>
#include <tuple>
#include <vector>
#include "common/common_types.h"
#include "vvctre/common.h"

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

    std::vector<std::tuple<std::string, std::string>> installed;
    std::string installed_query;
};

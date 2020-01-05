// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#define CATCH_CONFIG_RUNNER
#include <catch2/catch.hpp>
#include "core/settings.h"

int main(int argc, char* argv[]) {
    Settings::values.use_custom_cpu_ticks = false;
    return Catch::Session().run(argc, argv);
}

// Copyright 2020 vvctre emulator project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <chrono>
#include <fmt/format.h>
#include "common/version.h"
#include "discord_rpc.h"
#include "vvctre/discord_rp.h"

DiscordRP::DiscordRP(const std::string& game) {
    Discord_Initialize("657225747915866157", nullptr, 1, nullptr);
    Update(game);
}

DiscordRP::~DiscordRP() {
    Discord_Shutdown();
}

void DiscordRP::Update(const std::string& game) {
    DiscordRichPresence presence{};
    presence.state = fmt::format("v{}", version::vvctre.to_string()).c_str();
    presence.details = game.c_str();
    presence.startTimestamp = std::chrono::duration_cast<std::chrono::seconds>(
                                  std::chrono::system_clock::now().time_since_epoch())
                                  .count();
    Discord_UpdatePresence(&presence);
}

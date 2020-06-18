// Copyright 2020 vvctre project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <string>
#include <tuple>
#include <vector>
#include "common/common_types.h"
#include "network/room_member.h"

extern const u8 vvctre_version_major;
extern const u8 vvctre_version_minor;
extern const u8 vvctre_version_patch;

struct CitraRoom {
    std::string name;
    std::string description;
    std::string owner;
    std::string ip;
    u16 port;
    u32 max_players;
    bool has_password;

    struct Member {};
    std::vector<Member> members;
};

using CitraRoomList = std::vector<CitraRoom>;

std::vector<std::tuple<std::string, std::string>> GetInstalledList();
CitraRoomList GetPublicCitraRooms();

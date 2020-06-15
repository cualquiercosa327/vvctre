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
    struct Member {
        // nothing is used
    };

    std::string name;
    std::string description;
    std::string owner;
    std::string ip;
    u16 port;
    u32 max_players;
    u32 net_version;
    bool has_password;

    std::vector<Member> members;
};

using CitraRoomList = std::vector<CitraRoom>;

std::vector<std::tuple<std::string, std::string>> GetInstalledList();
CitraRoomList GetPublicCitraRooms();

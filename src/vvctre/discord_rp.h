// Copyright 2020 vvctre emulator project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <string>

class DiscordRP {
public:
    DiscordRP(const std::string& game);
    ~DiscordRP();
    void Update(const std::string& game);
};

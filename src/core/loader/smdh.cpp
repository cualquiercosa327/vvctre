// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <cstring>
#include <vector>
#include "common/common_types.h"
#include "core/loader/loader.h"
#include "core/loader/smdh.h"

namespace Loader {

bool IsValidSMDH(const std::vector<u8>& smdh_data) {
    if (smdh_data.size() < sizeof(Loader::SMDH)) {
        return false;
    }

    u32 magic;
    std::memcpy(&magic, smdh_data.data(), sizeof(u32));

    return Loader::MakeMagic('S', 'M', 'D', 'H') == magic;
}

std::array<u16, 0x40> SMDH::GetShortTitle(Loader::SMDH::TitleLanguage language) const {
    return titles[static_cast<int>(language)].short_title;
}

} // namespace Loader

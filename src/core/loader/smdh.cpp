// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <cstring>
#include <vector>
#include "common/common_types.h"
#include "core/loader/loader.h"
#include "core/loader/smdh.h"
#include "video_core/utils.h"

namespace Loader {

std::vector<u16> SMDH::GetIcon(bool large) const {
    u32 size;
    const u8* icon_data;

    if (large) {
        size = 48;
        icon_data = large_icon.data();
    } else {
        size = 24;
        icon_data = small_icon.data();
    }

    std::vector<u16> icon(size * size);
    for (u32 x = 0; x < size; ++x) {
        for (u32 y = 0; y < size; ++y) {
            u32 coarse_y = y & ~7;
            const u8* pixel = icon_data + VideoCore::GetMortonOffset(x, y, 2) + coarse_y * size * 2;
            icon[x + size * y] = (pixel[1] << 8) + pixel[0];
        }
    }
    return icon;
}

} // namespace Loader

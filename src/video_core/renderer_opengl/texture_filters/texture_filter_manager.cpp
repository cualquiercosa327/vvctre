// Copyright 2019 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/logging/log.h"
#include "video_core/renderer_opengl/texture_filters/anime4k_ultrafast.h"
#include "video_core/renderer_opengl/texture_filters/texture_filter_manager.h"
#include "video_core/renderer_opengl/texture_filters/xbrz_cpu.h"
#include "video_core/renderer_opengl/texture_filters/xbrz_freescale.h"

namespace OpenGL {

template <typename T>
static std::pair<std::string, TextureFilterInfo> FilterMapPair = {T::GetInfo().name, T::GetInfo()};

struct NoFilter {
    static TextureFilterInfo GetInfo() {
        TextureFilterInfo info;
        info.name = "none";
        info.clamp_scale = {1, 1};
        info.constructor = [] { return nullptr; };
        return info;
    }
};

const std::map<std::string, TextureFilterInfo>& TextureFilterManager::TextureFilterMap() {
    static const std::map<std::string, TextureFilterInfo> filter_map{
        FilterMapPair<NoFilter>,
        FilterMapPair<Anime4kUltrafast>,
        FilterMapPair<XbrzFreescale>,
        FilterMapPair<XbrzCpu>,
    };
    return filter_map;
}

void TextureFilterManager::SetTextureFilter(const std::string& filter_name, u16 new_scale_factor) {
    if (name == filter_name && scale_factor == new_scale_factor)
        return;
    std::lock_guard<std::mutex> lock{mutex};
    name = filter_name;
    scale_factor = new_scale_factor;
    updated = true;
}

// <filter, scale_factor, delete cache>
std::tuple<TextureFilterInterface*, u16, bool> TextureFilterManager::GetTextureFilter() {
    if (!updated)
        return {filter.get(), scale_factor, false};
    updated = false;

    std::lock_guard<std::mutex> lock{mutex};
    auto iter = TextureFilterMap().find(name);
    if (iter == TextureFilterMap().end()) {
        LOG_ERROR(Render_OpenGL, "Invalid texture filter: {}", name);
        return {nullptr, 0, true};
    }

    const auto& filter_info = iter->second;
    filter = filter_info.constructor();
    u16 scale_unclamped = scale_factor;
    scale_factor =
        std::clamp(scale_factor, filter_info.clamp_scale.min, filter_info.clamp_scale.max);
    if (scale_unclamped != scale_factor) {
        LOG_ERROR(Render_OpenGL, "Invalid scale factor {}x for texture filter {}; Clamped to {}",
                  scale_unclamped, name, scale_factor);
    }

    return {filter.get(), scale_factor, true};
}

} // namespace OpenGL
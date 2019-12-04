// Copyright 2019 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "video_core/renderer_opengl/texture_filters/texture_filter_manager.h"

namespace OpenGL {

template <typename T>
static constexpr std::pair<std::string, TextureFilterManager::TextureFilterConstructor>
FilterMapPair(std::string name) {
    return {name,
            []() -> std::unique_ptr<TextureFilterInterface> { return std::make_unique<T>(); }};
}

const std::map<std::string_view, TextureFilterManager::TextureFilterConstructor>
    TextureFilterManager::texture_filter_map{
        {"none", []() -> std::unique_ptr<TextureFilterInterface> { return nullptr; }},
        FilterMapPair<Anime4kUltrafast>("Anime4K Ultrafast"),
        FilterMapPair<XbrzCpu>("xBRZ (CPU)"),
        FilterMapPair<XbrzFreescale>("xBRZ (GPU)"),
    };

void TextureFilterManager::SetTextureFilter(const std::string& filter_name, u16 new_scale_factor) {
    if (name == filter_name && scale_factor == new_scale_factor)
        return;
    std::lock_guard<std::mutex>{mutex};
    name = filter_name;
    scale_factor = new_scale_factor;
    updated = true;
}

std::tuple<TextureFilterInterface*, u16, bool> TextureFilterManager::GetTextureFilter() {
    if (!updated)
        return {filter.get(), scale_factor, false};

    std::lock_guard<std::mutex>{mutex};
    auto iter = texture_filter_map.find(name);
    if (iter == texture_filter_map.end()) {
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

    filter = iter->second();
    return {filter.get(), scale_factor, true};
}

} // namespace OpenGL
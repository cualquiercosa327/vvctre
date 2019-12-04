// Copyright 2019 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <atomic>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <tuple>

#include "video_core/renderer_opengl/texture_filters/texture_filter_interface.h"

namespace OpenGL {

class TextureFilterManager {
    std::atomic<bool> updated{false};
    std::mutex mutex;
    std::string name{"none"};
    u16 scale_factor{1};

    std::unique_ptr<TextureFilterInterface> filter;

public:
    // function ensures map is initialized before use
    static const std::map<std::string, TextureFilterInfo>& TextureFilterMap();

    static TextureFilterManager& GetInstance() {
        static TextureFilterManager singleton;
        return singleton;
    }

    void SetTextureFilter(const std::string& filter_name, u16 new_scale_factor);
    std::tuple<TextureFilterInterface*, u16, bool> GetTextureFilter();
};

} // namespace OpenGL
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

#include "common/logging/log.h"
#include "video_core/renderer_opengl/texture_filters/anime4k_ultrafast.h"
#include "video_core/renderer_opengl/texture_filters/texture_filter_interface.h"
#include "video_core/renderer_opengl/texture_filters/xbrz_cpu.h"
#include "video_core/renderer_opengl/texture_filters/xbrz_freescale.h"

namespace OpenGL {

class TextureFilterManager {
    using TextureFilterConstructor = std::unique_ptr<TextureFilterInterface> (*)();

    static TextureFilterManager singleton;

    std::atomic<bool> updated{false};
    std::mutex mutex;
    std::string name{"none"};
    u16 scale_factor{0};

    std::unique_ptr<TextureFilterInterface> filter;

public:
    static const std::map<std::string_view, TextureFilterConstructor> texture_filter_map;

    static TextureFilterManager& GetInstance() {
        return singleton;
    }

    void SetTextureFilter(const std::string& filter_name, u16 new_scale_factor);
    std::tuple<TextureFilterInterface*, u16, bool> GetTextureFilter();
};

} // namespace OpenGL
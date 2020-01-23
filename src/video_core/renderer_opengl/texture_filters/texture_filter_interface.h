// Copyright 2019 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <functional>
#include <string>
#include "common/common_types.h"

namespace OpenGL {

struct CachedSurface;
using Surface = std::shared_ptr<CachedSurface>;

class TextureFilterInterface {
public:
    u16 scale_factor{};
    virtual void scale(const Surface& surface) = 0;
    virtual ~TextureFilterInterface() = default;
};

// every texture filter should have a static GetInfo function
struct TextureFilterInfo {
    std::string name;
    struct {
        u16 min, max;
    } clamp_scale{1, 10};
    std::function<std::unique_ptr<TextureFilterInterface>()> constructor;
};

} // namespace OpenGL

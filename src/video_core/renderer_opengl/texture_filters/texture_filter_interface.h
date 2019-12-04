// Copyright 2019 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

namespace OpenGL {

struct CachedSurface;
using Surface = std::shared_ptr<CachedSurface>;

class TextureFilterInterface {
public:
    virtual void scale(const Surface& src_surface, const Surface& dst_surface) = 0;
    virtual ~TextureFilterInterface() = default;
};
} // namespace OpenGL

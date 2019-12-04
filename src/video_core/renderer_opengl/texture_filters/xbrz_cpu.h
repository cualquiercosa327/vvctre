// Copyright 2019 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <vector>

#include "video_core/renderer_opengl/texture_filters/texture_filter_interface.h"

namespace OpenGL {
class XbrzCpu : public TextureFilterInterface {
    OGLFramebuffer read_fbo, draw_fbo;
    OGLTexture tmp_tex;
    std::vector<u32> tmp_buf, upscaled_buf;

public:
    static TextureFilterInfo GetInfo() {
        TextureFilterInfo info;
        info.name = "xBRZ (CPU)";
        info.clamp_scale = {1, 6};
        info.constructor = std::make_unique<XbrzCpu>;
        return info;
    }

    XbrzCpu();
    void scale(const Surface& src_surface, const Surface& dst_surface) override;
};
} // namespace OpenGL

// Copyright 2019 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "video_core/renderer_opengl/gl_resource_manager.h"
#include "video_core/renderer_opengl/gl_state.h"
#include "video_core/renderer_opengl/texture_filters/texture_filter_interface.h"

namespace OpenGL {

class XbrzFreescale : public TextureFilterInterface {
    OpenGLState state{};

    OGLProgram program;
    OGLVertexArray vao;
    OGLFramebuffer draw_fbo;
    OGLSampler source_sampler;
    /// uniform locations
    GLint output_size{-1};

public:
    static TextureFilterInfo GetInfo() {
        TextureFilterInfo info;
        info.name = "xBRZ freescale";
        info.constructor = std::make_unique<XbrzFreescale>;
        return info;
    }

    XbrzFreescale();
    void scale(const Surface& surface) override;
};
} // namespace OpenGL

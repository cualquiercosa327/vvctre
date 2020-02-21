// Copyright 2019 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "video_core/renderer_opengl/gl_resource_manager.h"
#include "video_core/renderer_opengl/gl_state.h"
#include "video_core/renderer_opengl/texture_filters/texture_filter_interface.h"

namespace OpenGL {

class Anime4kUltrafast : public TextureFilterInterface {
public:
    static TextureFilterInfo GetInfo() {
        TextureFilterInfo info;
        info.name = "Anime4K Ultrafast";
        info.clamp_scale = {1, 2};
        info.constructor = std::make_unique<Anime4kUltrafast>;
        return info;
    }

    Anime4kUltrafast();
    void scale(const Surface& surface) override;

private:
    OpenGLState state{};

    OGLVertexArray vao;
    OGLBuffer vertices;
    OGLFramebuffer out_fbo;

    struct TempTex {
        OGLTexture tex;
        OGLFramebuffer fbo;
    };
    TempTex LUMAD;
    TempTex LUMAG;

    struct Program {
        OGLProgram prog;
        GLint d = -1;
    } gradient_program, gaussian_program, refine_program;
};

} // namespace OpenGL

// Copyright 2019 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "video_core/renderer_opengl/gl_rasterizer_cache.h"
#include "video_core/renderer_opengl/texture_filters/anime4k_ultrafast.h"

namespace OpenGL {

Anime4kUltrafast::Anime4kUltrafast() {
    auto cur_state = OpenGLState::GetCurState();
    glActiveTexture(GL_TEXTURE0);
    const auto setup_temp_tex = [this](TempTex& texture) {
        texture.fbo.Create();
        texture.tex.Create();
        state.draw.draw_framebuffer = texture.fbo.handle;
        state.texture_units[0].texture_2d = texture.tex.handle;
        state.Apply();
        glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                               texture.tex.handle, 0);
    };
    setup_temp_tex(LUMAD);
    setup_temp_tex(LUMAG);

    vao.Create();
    vertices.Create();
    out_fbo.Create();

    state.texture_units[1].texture_2d = LUMAD.tex.handle;
    state.texture_units[2].texture_2d = LUMAG.tex.handle;
    state.draw.vertex_array = vao.handle;
    state.draw.vertex_buffer = vertices.handle;
    state.Apply();
    constexpr GLfloat square_vertices[]{-1, 1, -1, -1, 1, 1, 1, -1};
    glBufferData(GL_ARRAY_BUFFER, sizeof(square_vertices), square_vertices, GL_STATIC_DRAW);

    const auto setup_program = [this](Program& program, const char* frag) {
        program.prog.Create(rect_vert, frag);
        state.draw.shader_program = program.prog.handle;
        state.Apply();
        program.d = glGetUniformLocation(program.prog.handle, "d");
        GLuint vertex_coord = glGetAttribLocation(program.prog.handle, "vertex_coord");
        glEnableVertexArrayAttrib(state.draw.vertex_array, vertex_coord);
        glVertexAttribPointer(vertex_coord, 2, GL_FLOAT, false, 0, 0);
    };
    setup_program(gradient_program, compute_gradient);
    setup_program(gaussian_program, compute_line_gaussian);
    setup_program(refine_program, refine);
    glUniform1i(glGetUniformLocation(refine_program.prog.handle, "LUMAD"), 1);
    glUniform1i(glGetUniformLocation(refine_program.prog.handle, "LUMAG"), 2);

    cur_state.Apply();
}

void Anime4kUltrafast::scale(const Surface& surface) {
    const auto cur_state = OpenGLState::GetCurState();
    glActiveTexture(GL_TEXTURE0);

    surface->res_scale *= scale_factor;
    const auto dest_rect = surface->GetScaledRect();
    OGLTexture dest_tex;
    dest_tex.Create();
    AllocateSurfaceTexture(dest_tex.handle, GetFormatTuple(surface->pixel_format),
                           dest_rect.GetWidth(), dest_rect.GetHeight());
    state.viewport = {(GLint)dest_rect.left, (GLint)dest_rect.bottom, (GLint)dest_rect.GetWidth(),
                      (GLint)dest_rect.GetHeight()};
    struct {
        GLfloat x, y;
    } d{1.f / dest_rect.GetWidth(), 1.f / dest_rect.GetHeight()};

    // gradient x pass
    state.texture_units[0].texture_2d = surface->texture.handle;
    state.draw.draw_framebuffer = out_fbo.handle;
    state.draw.shader_program = gradient_program.prog.handle;
    state.Apply();

    glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                           dest_tex.handle, 0);
    AllocateSurfaceTexture(LUMAD.tex.handle, GetFormatTuple(surface->pixel_format),
                           dest_rect.GetWidth(), dest_rect.GetHeight());
    AllocateSurfaceTexture(LUMAG.tex.handle, GetFormatTuple(surface->pixel_format),
                           dest_rect.GetWidth(), dest_rect.GetHeight());

    glUniform2f(gradient_program.d, d.x, 0);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    // gradient y pass
    state.texture_units[0].texture_2d = dest_tex.handle;
    state.draw.draw_framebuffer = LUMAD.fbo.handle;
    state.Apply();
    glUniform2f(gradient_program.d, 0, d.y);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    // gaussian x pass
    state.texture_units[0].texture_2d = LUMAD.tex.handle;
    state.draw.draw_framebuffer = out_fbo.handle;
    state.draw.shader_program = gaussian_program.prog.handle;
    state.Apply();
    glUniform2f(gaussian_program.d, d.x, 0);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    // gradient y pass
    state.texture_units[0].texture_2d = dest_tex.handle;
    state.draw.draw_framebuffer = LUMAG.fbo.handle;
    state.Apply();
    glUniform2f(gaussian_program.d, 0, d.y);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    // refine pass
    state.texture_units[0].texture_2d = surface->texture.handle;
    state.draw.draw_framebuffer = out_fbo.handle;
    state.draw.shader_program = refine_program.prog.handle;
    state.Apply();
    glUniform2f(refine_program.d, d.x, d.y);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    surface->texture = std::move(dest_tex);

    glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0);
    cur_state.Apply();
}

} // namespace OpenGL

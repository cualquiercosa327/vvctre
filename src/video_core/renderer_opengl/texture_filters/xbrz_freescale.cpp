// Copyright 2019 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "video_core/renderer_opengl/gl_rasterizer_cache.h"
#include "video_core/renderer_opengl/texture_filters/xbrz_freescale.h"

namespace OpenGL {

XbrzFreescale::XbrzFreescale() {
    OpenGLState cur_state = OpenGLState::GetCurState();

    program.Create(shader_vert, shader_frag);
    vao.Create();
    vertices.Create();
    draw_fbo.Create();

    state.draw.shader_program = program.handle;
    state.draw.vertex_array = vao.handle;
    state.draw.vertex_buffer = vertices.handle;
    state.draw.shader_program = program.handle;
    state.draw.draw_framebuffer = draw_fbo.handle;

    state.Apply();

    vertex_coord = glGetAttribLocation(program.handle, "vertex_coord");
    output_size = glGetUniformLocation(program.handle, "output_size");
    constexpr GLfloat square_vertices[]{-1, 1, -1, -1, 1, 1, 1, -1};
    glBufferData(GL_ARRAY_BUFFER, sizeof(square_vertices), square_vertices, GL_STATIC_DRAW);
    glEnableVertexArrayAttrib(state.draw.vertex_array, vertex_coord);
    glVertexAttribPointer(vertex_coord, 2, GL_FLOAT, false, 0, 0);

    cur_state.Apply();
}

void XbrzFreescale::scale(const Surface& surface) {
    const OpenGLState cur_state = OpenGLState::GetCurState();

    state.texture_units[0].texture_2d = surface->texture.handle;
    surface->res_scale *= scale_factor;
    const auto dest_rect = surface->GetScaledRect();

    OGLTexture dest_tex;
    dest_tex.Create();
    AllocateSurfaceTexture(dest_tex.handle, GetFormatTuple(surface->pixel_format),
                           dest_rect.GetWidth(), dest_rect.GetHeight());

    state.viewport = {(GLint)dest_rect.left, (GLint)dest_rect.bottom, (GLint)dest_rect.GetWidth(),
                      (GLint)dest_rect.GetHeight()};
    state.Apply();

    glUniform2i(output_size, dest_rect.GetWidth(), dest_rect.GetHeight());
    glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                           dest_tex.handle, 0);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, NULL, 0);
    surface->texture = std::move(dest_tex);

    cur_state.Apply();
}

} // namespace OpenGL
// Copyright 2019 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <thread>

#include <xbrz/xbrz.h>

#include "video_core/renderer_opengl/gl_rasterizer_cache.h"
#include "video_core/renderer_opengl/gl_state.h"
#include "xbrz_cpu.h"

namespace OpenGL {

XbrzCpu::XbrzCpu() {
    tmp_tex.Create();
    read_fbo.Create();
    draw_fbo.Create();
}

void XbrzCpu::scale(const Surface& src_surface, const Surface& dst_surface) {
    OpenGLState cur_state = OpenGLState::GetCurState();
    OpenGLState state = cur_state;

    // xbrz::scale expects 32bit aligment
    const u32* src_buf = nullptr;
    GLuint dest_tex = 0;

    if (dst_surface->type != SurfaceParams::SurfaceType::Texture) {
        // Convert to BGRA, download, allocate output texture
        AllocateSurfaceTexture(tmp_tex.handle, tex_tuple, src_surface->GetScaledWidth(),
                               src_surface->GetScaledHeight());
        BlitTextures(src_surface->texture.handle, src_surface->GetRect(), tmp_tex.handle,
                     src_surface->GetRect(), SurfaceParams::SurfaceType::Texture, read_fbo.handle,
                     draw_fbo.handle);

        state.texture_units[0].texture_2d = tmp_tex.handle;
        state.Apply();

        glActiveTexture(GL_TEXTURE0);
        tmp_buf.resize(src_surface->GetScaledWidth() * src_surface->GetScaledHeight());
        glGetTexImage(GL_TEXTURE_2D, 0, tex_tuple.format, tex_tuple.type, tmp_buf.data());
        src_buf = tmp_buf.data();

        AllocateSurfaceTexture(tmp_tex.handle, tex_tuple, dst_surface->GetScaledWidth(),
                               dst_surface->GetScaledHeight());
        dest_tex = tmp_tex.handle;
    } else {
        src_buf = reinterpret_cast<u32*>(src_surface->gl_buffer.get());
        dest_tex = dst_surface->texture.handle;
    }

    auto xbrzFormat = (dst_surface->pixel_format == SurfaceParams::PixelFormat::RGB8 ||
                       dst_surface->pixel_format == SurfaceParams::PixelFormat::RGB565)
                          ? xbrz::ColorFormat::RGB
                          : xbrz::ColorFormat::ARGB;

    upscaled_buf.resize(dst_surface->GetScaledWidth() * dst_surface->GetScaledHeight());

    u32 rows_per_thread = std::max(
        ((src_surface->GetScaledHeight() - 1) / std::thread::hardware_concurrency()) + 1, 16u);
    std::vector<std::thread> threads;
    for (u32 y = 0; y < src_surface->GetScaledHeight(); y += rows_per_thread) {
        threads.emplace_back([res_scale = dst_surface->res_scale, src_rect = src_surface->GetRect(),
                              upscaled_buf = upscaled_buf.data(), xbrzFormat, src_buf,
                              rows_per_thread, y] {
            xbrz::scale(res_scale, src_buf, upscaled_buf, src_rect.GetWidth(), src_rect.GetHeight(),
                        xbrzFormat, xbrz::ScalerCfg{}, y,
                        std::min(y + rows_per_thread, src_rect.GetHeight()));
        });
    }
    for (auto& thread : threads) {
        thread.join();
    }

    state.texture_units[0].texture_2d = dest_tex;
    state.Apply();

    glActiveTexture(GL_TEXTURE0);
    glTexImage2D(GL_TEXTURE_2D, 0, tex_tuple.internal_format, dst_surface->GetScaledWidth(),
                 dst_surface->GetScaledHeight(), 0, tex_tuple.format, tex_tuple.type,
                 upscaled_buf.data());

    if (dst_surface->type != SurfaceParams::SurfaceType::Texture) {
        BlitTextures(tmp_tex.handle, dst_surface->GetScaledRect(), dst_surface->texture.handle,
                     dst_surface->GetScaledRect(), SurfaceParams::SurfaceType::Texture,
                     read_fbo.handle, draw_fbo.handle);
    }
    cur_state.Apply();
}

} // namespace OpenGL

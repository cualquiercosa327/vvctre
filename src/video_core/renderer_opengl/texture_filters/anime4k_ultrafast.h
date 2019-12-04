// Copyright 2019 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "video_core/renderer_opengl/gl_resource_manager.h"
#include "video_core/renderer_opengl/gl_state.h"
#include "video_core/renderer_opengl/texture_filters/texture_filter_interface.h"

namespace OpenGL {

// MIT License
//
// Copyright(c) 2019 bloc97
//
// Permission is hereby granted,
// free of charge,
// to any person obtaining a copy of this software and associated documentation
// files(the "Software"),
// to deal in the Software without restriction, including without limitation the rights to use,
// copy, modify, merge, publish, distribute, sublicense, and / or sell copies of the Software,
// and to permit persons to whom the Software is furnished to do so,
// subject to the following conditions :
//
// The above copyright notice and this permission notice shall be included in all copies
// or
// substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS",
// WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
// INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND
// NONINFRINGEMENT.IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
//
// adapted from
// https://github.com/bloc97/Anime4K/blob/533cee5f7018d0e57ad2a26d76d43f13b9d8782a/glsl/Anime4K_Adaptive_v1.0RC2_UltraFast.glsl
class Anime4kUltrafast : public TextureFilterInterface {

    static constexpr char rect_vert[] = R"(
in vec4 vertex_coord;

out vec2 tex_coord;

void main() {
    gl_Position = vertex_coord;
    tex_coord = (vertex_coord.xy + 1) / 2;
}
)";

    static constexpr char compute_gradient[] = R"(
in vec2 tex_coord;

out vec4 frag_color;

uniform sampler2D tex_input;
uniform vec2 d;

vec4 Tex(vec2 offset) {
    return texture(tex_input, tex_coord + offset);
}

float GetLum(vec4 c){
    const vec3 w = vec3(1/3.0, 1/2.0, 1/6.0);
    return dot(c.rgb, w);
}

vec4 ComputeGradX() {
    // [tl  t tr]
    // [ l  c  r]
    // [bl  b br]
    float l = GetLum(Tex(-d));
    float c = GetLum(Tex(vec2(0)));
    float r = GetLum(Tex(d));


    //Horizontal Gradient
    // [-1  0  1]
    // [-2  0  2]
    // [-1  0  1]
    float xgrad = (-l + r);

    //Vertical Gradient
    // [-1 -2 -1]
    // [ 0  0  0]
    // [ 1  2  1]
    float ygrad = (l + c + c + r);

    //Computes the luminance's gradient
    return vec4(xgrad, ygrad, 0, 0);
}

vec4 ComputeGradY(){
    // [tl  t tr]
    // [ l cc  r]
    // [bl  b br]
    vec4 t = Tex(-d);
    vec4 c = Tex(vec2(0));
    vec4 b = Tex(d);

    //Horizontal Gradient
    // [-1  0  1]
    // [-2  0  2]
    // [-1  0  1]
    float xgrad = (t.x + c.x + c.x + b.x);

    // Vertical Gradient
    // [-1 -2 -1]
    // [ 0  0  0]
    // [ 1  2  1]
    float ygrad = (-t.y + b.y);

    // Computes the luminance's gradient
    return vec4(1 - clamp(sqrt(xgrad * xgrad + ygrad * ygrad), 0, 1), 0, 0, 0);
}

void main(){
    frag_color = (d.x != 0) ? ComputeGradX() : ComputeGradY();
}
)";

    static constexpr char compute_line_gaussian[] = R"(
in vec2 tex_coord;

out vec4 frag_color;

uniform sampler2D tex_input;
uniform vec2 d;

#define Tex(offset) texture(tex_input, tex_coord + offset)

void main() {
    vec4 g = Tex(-d * 2) * 0.187691;
    g += Tex(-d) * 0.206038;
    g += Tex(vec2(0)) * 0.212543;
    g += Tex(d) * 0.206038;
    g += Tex(d * 2) * 0.187691;

    frag_color = 1 - g;
}

)";

    static constexpr char refine[] = R"(
in vec2 tex_coord;

out vec4 frag_color;

uniform sampler2D HOOKED, LUMAD, LUMAG;
uniform vec2 d;

#define HOOKED_tex(offset) texture(HOOKED, tex_coord + offset)
#define LUMAD_tex(offset) texture(LUMAD, tex_coord + offset)
#define LUMAG_tex(offset) texture(LUMAG, tex_coord + offset)

#define LINE_DETECT_THRESHOLD 0.4
#define MAX_STRENGTH 0.6

#define strength (min((1.0 / textureSize(HOOKED, 0)).x / d.x, 1))
#define lineprob (LUMAG_tex(tex_coord).x)

// the original shader used the alpha channel for luminance,
// which doesn't work for our use case
struct RGBAL{
    vec4 c;
    float l;
};

vec4 getAverage(vec4 cc, vec4 a, vec4 b, vec4 c) {
    float realstrength = clamp(strength, 0, MAX_STRENGTH);
    return cc * (1 - realstrength) + ((a + b + c) / 3) * realstrength;
}

RGBAL GetRGBAL(vec2 offset) {
    return RGBAL(HOOKED_tex(offset), LUMAD_tex(vec2(0)).x);
}

float min3v(float a, float b, float c) {
    return min(min(a, b), c);
}

float max3v(float a, float b, float c) {
    return max(max(a, b), c);
}

vec4 Compute()  {
    if (lineprob < LINE_DETECT_THRESHOLD) {
        return HOOKED_tex(vec2(0));
    }

    RGBAL cc = GetRGBAL(vec2(0));
    RGBAL t = GetRGBAL(vec2(0, -d.y));
    RGBAL tl = GetRGBAL(vec2(-d.x, -d.y));
    RGBAL tr = GetRGBAL(vec2(d.x, -d.y));

    RGBAL l = GetRGBAL(vec2(-d.x, 0));
    RGBAL r = GetRGBAL(vec2(d.x, 0));

    RGBAL b = GetRGBAL(vec2(0, d.y));
    RGBAL bl = GetRGBAL(vec2(-d.x, d.y));
    RGBAL br = GetRGBAL(vec2(d.x, d.y));

    //Kernel 0 and 4
    float maxDark = max3v(br.l, b.l, bl.l);
    float minLight = min3v(tl.l, t.l, tr.l);

    if (minLight > cc.l && minLight > maxDark) {
        return getAverage(cc.c, tl.c, t.c, tr.c);
    } else {
        maxDark = max3v(tl.l, t.l, tr.l);
        minLight = min3v(br.l, b.l, bl.l);
        if (minLight > cc.l && minLight > maxDark) {
            return getAverage(cc.c, br.c, b.c, bl.c);
        }
    }

    //Kernel 1 and 5
    maxDark = max3v(cc.l, l.l, b.l);
    minLight = min3v(r.l, t.l, tr.l);

    if (minLight > maxDark) {
        return getAverage(cc.c, r.c, t.c, tr.c);
    } else {
        maxDark = max3v(cc.l, r.l, t.l);
        minLight = min3v(bl.l, l.l, b.l);
        if (minLight > maxDark) {
            return getAverage(cc.c, bl.c, l.c, b.c);
        }
    }

    //Kernel 2 and 6
    maxDark = max3v(l.l, tl.l, bl.l);
    minLight = min3v(r.l, br.l, tr.l);

    if (minLight > cc.l && minLight > maxDark) {
        return getAverage(cc.c, r.c, br.c, tr.c);
    } else {
        maxDark = max3v(r.l, br.l, tr.l);
        minLight = min3v(l.l, tl.l, bl.l);
        if (minLight > cc.l && minLight > maxDark) {
            return getAverage(cc.c, l.c, tl.c, bl.c);
        }
    }

    //Kernel 3 and 7
    maxDark = max3v(cc.l, l.l, t.l);
    minLight = min3v(r.l, br.l, b.l);

    if (minLight > maxDark) {
        return getAverage(cc.c, r.c, br.c, b.c);
    } else {
        maxDark = max3v(cc.l, r.l, b.l);
        minLight = min3v(t.l, l.l, tl.l);
        if (minLight > maxDark) {
            return getAverage(cc.c, t.c, l.c, tl.c);
        }
    }

    return cc.c;
}

void main(){
    frag_color = Compute();
}
)";

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

public:
    static TextureFilterInfo GetInfo() {
        TextureFilterInfo info;
        info.name = "Anime4K Ultrafast";
        info.constructor = std::make_unique<Anime4kUltrafast>;
        return info;
    }

    Anime4kUltrafast();
    void scale(const Surface& src_surface, const Surface& dst_surface) override;
};

} // namespace OpenGL

// Copyright 2019 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "video_core/renderer_opengl/gl_resource_manager.h"
#include "video_core/renderer_opengl/gl_state.h"
#include "video_core/renderer_opengl/texture_filters/texture_filter_interface.h"

namespace OpenGL {

// xBRZ freescale
// based on :
// 4xBRZ shader - Copyright (C) 2014-2016 DeSmuME team
//
// This file is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 2 of the License, or
// (at your option) any later version.
//
// This file is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with the this software.  If not, see <http://www.gnu.org/licenses/>.
/*
   Hyllian's xBR-vertex code and texel mapping
   Copyright (C) 2011/2016 Hyllian - sergiogdb@gmail.com
   Permission is hereby granted, free of charge, to any person obtaining a copy
   of this software and associated documentation files (the "Software"), to deal
   in the Software without restriction, including without limitation the rights
   to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
   copies of the Software, and to permit persons to whom the Software is
   furnished to do so, subject to the following conditions:
   The above copyright notice and this permission notice shall be included in
   all copies or substantial portions of the Software.
   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
   OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
   THE SOFTWARE.
*/
// adapted from
// https://github.com/libretro/glsl-shaders/blob/d7a8b8eb2a61a5732da4cbe2e0f9ad30600c3f17/xbrz/shaders/xbrz-freescale.glsl
class XbrzFreescale : public TextureFilterInterface {

    static constexpr char shader_vert[] = R"(
in vec4 vertex_coord;

void main() {
    gl_Position = vertex_coord;
}
)";

    static constexpr char shader_frag[] = R"(
out vec4 frag_color;

uniform ivec2 output_size;
uniform sampler2D tex;

#define BLEND_NONE 0
#define BLEND_NORMAL 1
#define BLEND_DOMINANT 2
#define LUMINANCE_WEIGHT 1.0
#define EQUAL_COLOR_TOLERANCE 30.0 / 255.0
#define STEEP_DIRECTION_THRESHOLD 2.2
#define DOMINANT_DIRECTION_THRESHOLD 3.6

float ColorDist(vec4 a, vec4 b) {
    const vec3 w = vec3(0.2627, 0.6780, 0.0593);
    const float scale_b = 0.5 / (1.0 - w.b);
    const float scale_r = 0.5 / (1.0 - w.r);
    vec4 diff = a - b;
    float y = dot(diff.rgb, w);
    float cb = scale_b * (diff.b - y);
    float cr = scale_r * (diff.r - y);
    float d = sqrt(((LUMINANCE_WEIGHT * y) * (LUMINANCE_WEIGHT * y)) + (cb * cb) + (cr * cr));
    return sqrt(a.a * b.a * d * d + diff.a * diff.a);
}

bool IsPixEqual(const vec4 pixA, const vec4 pixB) {
    return ColorDist(pixA, pixB) < EQUAL_COLOR_TOLERANCE;
}

float GetLeftRatio(vec2 center, vec2 origin, vec2 direction, vec2 scale) {
    vec2 P0 = center - origin;
    vec2 proj = direction * (dot(P0, direction) / dot(direction, direction));
    vec2 distv = P0 - proj;
    vec2 orth = vec2(-direction.y, direction.x);
    float side = sign(dot(P0, orth));
    float v = side * length(distv * scale);
    return smoothstep(-sqrt(2.0) / 2.0, sqrt(2.0) / 2.0, v);
}

#define eq(a, b) (a == b)
#define neq(a, b) (a != b)
#define P(x, y) texture(tex, coord + vec2(x, y) / source_size)

void main() {
    vec2 source_size = textureSize(tex, 0);
    vec2 scale = output_size / source_size;
    vec2 pos = fract(gl_FragCoord.xy / output_size * source_size) - vec2(0.5, 0.5);
    vec2 coord = gl_FragCoord.xy / output_size - pos / source_size;
    //---------------------------------------
    // Input Pixel Mapping:  -|x|x|x|-
    //                       x|A|B|C|x
    //                       x|D|E|F|x
    //                       x|G|H|I|x
    //                       -|x|x|x|-
    vec4 A = P(-1, -1);
    vec4 B = P(0, -1);
    vec4 C = P(1, -1);
    vec4 D = P(-1, 0);
    vec4 E = P(0, 0);
    vec4 F = P(1, 0);
    vec4 G = P(-1, 1);
    vec4 H = P(0, 1);
    vec4 I = P(1, 1);
    // blendResult Mapping: x|y|
    //                      w|z|
    ivec4 blendResult = ivec4(BLEND_NONE, BLEND_NONE, BLEND_NONE, BLEND_NONE);
    // Preprocess corners
    // Pixel Tap Mapping: -|-|-|-|-
    //                    -|-|B|C|-
    //                    -|D|E|F|x
    //                    -|G|H|I|x
    //                    -|-|x|x|-
    if (!((eq(E, F) && eq(H, I)) || (eq(E, H) && eq(F, I)))) {
        float dist_H_F = ColorDist(G, E) + ColorDist(E, C) + ColorDist(P(0, 2), I) +
                         ColorDist(I, P(2, 0)) + (4.0 * ColorDist(H, F));
        float dist_E_I = ColorDist(D, H) + ColorDist(H, P(1, 2)) + ColorDist(B, F) +
                         ColorDist(F, P(2, 1)) + (4.0 * ColorDist(E, I));
        bool dominantGradient = (DOMINANT_DIRECTION_THRESHOLD * dist_H_F) < dist_E_I;
        blendResult.z = ((dist_H_F < dist_E_I) && neq(E, F) && neq(E, H))
                            ? ((dominantGradient) ? BLEND_DOMINANT : BLEND_NORMAL)
                            : BLEND_NONE;
    }
    // Pixel Tap Mapping: -|-|-|-|-
    //                    -|A|B|-|-
    //                    x|D|E|F|-
    //                    x|G|H|I|-
    //                    -|x|x|-|-
    if (!((eq(D, E) && eq(G, H)) || (eq(D, G) && eq(E, H)))) {
        float dist_G_E = ColorDist(P(-2, 1), D) + ColorDist(D, B) + ColorDist(P(-1, 2), H) +
                         ColorDist(H, F) + (4.0 * ColorDist(G, E));
        float dist_D_H = ColorDist(P(-2, 0), G) + ColorDist(G, P(0, 2)) + ColorDist(A, E) +
                         ColorDist(E, I) + (4.0 * ColorDist(D, H));
        bool dominantGradient = (DOMINANT_DIRECTION_THRESHOLD * dist_D_H) < dist_G_E;
        blendResult.w = ((dist_G_E > dist_D_H) && neq(E, D) && neq(E, H))
                            ? ((dominantGradient) ? BLEND_DOMINANT : BLEND_NORMAL)
                            : BLEND_NONE;
    }
    // Pixel Tap Mapping: -|-|x|x|-
    //                    -|A|B|C|x
    //                    -|D|E|F|x
    //                    -|-|H|I|-
    //                    -|-|-|-|-
    if (!((eq(B, C) && eq(E, F)) || (eq(B, E) && eq(C, F)))) {
        float dist_E_C = ColorDist(D, B) + ColorDist(B, P(1, -2)) + ColorDist(H, F) +
                         ColorDist(F, P(2, -1)) + (4.0 * ColorDist(E, C));
        float dist_B_F = ColorDist(A, E) + ColorDist(E, I) + ColorDist(P(0, -2), C) +
                         ColorDist(C, P(2, 0)) + (4.0 * ColorDist(B, F));
        bool dominantGradient = (DOMINANT_DIRECTION_THRESHOLD * dist_B_F) < dist_E_C;
        blendResult.y = ((dist_E_C > dist_B_F) && neq(E, B) && neq(E, F))
                            ? ((dominantGradient) ? BLEND_DOMINANT : BLEND_NORMAL)
                            : BLEND_NONE;
    }
    // Pixel Tap Mapping: -|x|x|-|-
    //                    x|A|B|C|-
    //                    x|D|E|F|-
    //                    -|G|H|-|-
    //                    -|-|-|-|-
    if (!((eq(A, B) && eq(D, E)) || (eq(A, D) && eq(B, E)))) {
        float dist_D_B = ColorDist(P(-2, 0), A) + ColorDist(A, P(0, -2)) + ColorDist(G, E) +
                         ColorDist(E, C) + (4.0 * ColorDist(D, B));
        float dist_A_E = ColorDist(P(-2, -1), D) + ColorDist(D, H) + ColorDist(P(-1, -2), B) +
                         ColorDist(B, F) + (4.0 * ColorDist(A, E));
        bool dominantGradient = (DOMINANT_DIRECTION_THRESHOLD * dist_D_B) < dist_A_E;
        blendResult.x = ((dist_D_B < dist_A_E) && neq(E, D) && neq(E, B))
                            ? ((dominantGradient) ? BLEND_DOMINANT : BLEND_NORMAL)
                            : BLEND_NONE;
    }
    vec4 res = E;
    // Pixel Tap Mapping: -|-|-|-|-
    //                    -|-|B|C|-
    //                    -|D|E|F|x
    //                    -|G|H|I|x
    //                    -|-|x|x|-
    if (blendResult.z != BLEND_NONE) {
        float dist_F_G = ColorDist(F, G);
        float dist_H_C = ColorDist(H, C);
        bool doLineBlend = (blendResult.z == BLEND_DOMINANT ||
                            !((blendResult.y != BLEND_NONE && !IsPixEqual(E, G)) ||
                              (blendResult.w != BLEND_NONE && !IsPixEqual(E, C)) ||
                              (IsPixEqual(G, H) && IsPixEqual(H, I) && IsPixEqual(I, F) &&
                               IsPixEqual(F, C) && !IsPixEqual(E, I))));
        vec2 origin = vec2(0.0, 1.0 / sqrt(2.0));
        ivec2 direction = ivec2(1, -1);
        if (doLineBlend) {
            bool haveShallowLine =
                (STEEP_DIRECTION_THRESHOLD * dist_F_G <= dist_H_C) && neq(E, G) && neq(D, G);
            bool haveSteepLine =
                (STEEP_DIRECTION_THRESHOLD * dist_H_C <= dist_F_G) && neq(E, C) && neq(B, C);
            origin = haveShallowLine ? vec2(0.0, 0.25) : vec2(0.0, 0.5);
            direction.x += haveShallowLine ? 1 : 0;
            direction.y -= haveSteepLine ? 1 : 0;
        }
        vec4 blendPix = mix(H, F, step(ColorDist(E, F), ColorDist(E, H)));
        res = mix(res, blendPix, GetLeftRatio(pos, origin, direction, scale));
    }
    // Pixel Tap Mapping: -|-|-|-|-
    //                    -|A|B|-|-
    //                    x|D|E|F|-
    //                    x|G|H|I|-
    //                    -|x|x|-|-
    if (blendResult.w != BLEND_NONE) {
        float dist_H_A = ColorDist(H, A);
        float dist_D_I = ColorDist(D, I);
        bool doLineBlend = (blendResult.w == BLEND_DOMINANT ||
                            !((blendResult.z != BLEND_NONE && !IsPixEqual(E, A)) ||
                              (blendResult.x != BLEND_NONE && !IsPixEqual(E, I)) ||
                              (IsPixEqual(A, D) && IsPixEqual(D, G) && IsPixEqual(G, H) &&
                               IsPixEqual(H, I) && !IsPixEqual(E, G))));
        vec2 origin = vec2(-1.0 / sqrt(2.0), 0.0);
        ivec2 direction = ivec2(1, 1);
        if (doLineBlend) {
            bool haveShallowLine =
                (STEEP_DIRECTION_THRESHOLD * dist_H_A <= dist_D_I) && neq(E, A) && neq(B, A);
            bool haveSteepLine =
                (STEEP_DIRECTION_THRESHOLD * dist_D_I <= dist_H_A) && neq(E, I) && neq(F, I);
            origin = haveShallowLine ? vec2(-0.25, 0.0) : vec2(-0.5, 0.0);
            direction.y += haveShallowLine ? 1 : 0;
            direction.x += haveSteepLine ? 1 : 0;
        }
        origin = origin;
        direction = direction;
        vec4 blendPix = mix(H, D, step(ColorDist(E, D), ColorDist(E, H)));
        res = mix(res, blendPix, GetLeftRatio(pos, origin, direction, scale));
    }
    // Pixel Tap Mapping: -|-|x|x|-
    //                    -|A|B|C|x
    //                    -|D|E|F|x
    //                    -|-|H|I|-
    //                    -|-|-|-|-
    if (blendResult.y != BLEND_NONE) {
        float dist_B_I = ColorDist(B, I);
        float dist_F_A = ColorDist(F, A);
        bool doLineBlend = (blendResult.y == BLEND_DOMINANT ||
                            !((blendResult.x != BLEND_NONE && !IsPixEqual(E, I)) ||
                              (blendResult.z != BLEND_NONE && !IsPixEqual(E, A)) ||
                              (IsPixEqual(I, F) && IsPixEqual(F, C) && IsPixEqual(C, B) &&
                               IsPixEqual(B, A) && !IsPixEqual(E, C))));
        vec2 origin = vec2(1.0 / sqrt(2.0), 0.0);
        ivec2 direction = ivec2(-1, -1);
        if (doLineBlend) {
            bool haveShallowLine =
                (STEEP_DIRECTION_THRESHOLD * dist_B_I <= dist_F_A) && neq(E, I) && neq(H, I);
            bool haveSteepLine =
                (STEEP_DIRECTION_THRESHOLD * dist_F_A <= dist_B_I) && neq(E, A) && neq(D, A);
            origin = haveShallowLine ? vec2(0.25, 0.0) : vec2(0.5, 0.0);
            direction.y -= haveShallowLine ? 1 : 0;
            direction.x -= haveSteepLine ? 1 : 0;
        }
        vec4 blendPix = mix(F, B, step(ColorDist(E, B), ColorDist(E, F)));
        res = mix(res, blendPix, GetLeftRatio(pos, origin, direction, scale));
    }
    // Pixel Tap Mapping: -|x|x|-|-
    //                    x|A|B|C|-
    //                    x|D|E|F|-
    //                    -|G|H|-|-
    //                    -|-|-|-|-
    if (blendResult.x != BLEND_NONE) {
        float dist_D_C = ColorDist(D, C);
        float dist_B_G = ColorDist(B, G);
        bool doLineBlend = (blendResult.x == BLEND_DOMINANT ||
                            !((blendResult.w != BLEND_NONE && !IsPixEqual(E, C)) ||
                              (blendResult.y != BLEND_NONE && !IsPixEqual(E, G)) ||
                              (IsPixEqual(C, B) && IsPixEqual(B, A) && IsPixEqual(A, D) &&
                               IsPixEqual(D, G) && !IsPixEqual(E, A))));
        vec2 origin = vec2(0.0, -1.0 / sqrt(2.0));
        ivec2 direction = ivec2(-1, 1);
        if (doLineBlend) {
            bool haveShallowLine =
                (STEEP_DIRECTION_THRESHOLD * dist_D_C <= dist_B_G) && neq(E, C) && neq(F, C);
            bool haveSteepLine =
                (STEEP_DIRECTION_THRESHOLD * dist_B_G <= dist_D_C) && neq(E, G) && neq(H, G);
            origin = haveShallowLine ? vec2(0.0, -0.25) : vec2(0.0, -0.5);
            direction.x -= haveShallowLine ? 1 : 0;
            direction.y += haveSteepLine ? 1 : 0;
        }
        vec4 blendPix = mix(D, B, step(ColorDist(E, B), ColorDist(E, D)));
        res = mix(res, blendPix, GetLeftRatio(pos, origin, direction, scale));
    }
    frag_color = res;
}
)";

    OpenGLState state{};

    OGLProgram program;
    OGLVertexArray vao;
    OGLBuffer vertices;
    OGLFramebuffer draw_fbo;
    /// attrib locations
    GLint vertex_coord;
    /// uniform locations
    GLint output_size;

public:
    XbrzFreescale();

    void scale(const Surface& src_surface, const Surface& dst_surface) override;
};
} // namespace OpenGL

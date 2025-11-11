//
// Created by . on 2/20/25.
//

#ifndef JSMOOCH_EMUS_RASTERIZE_TRI_H
#define JSMOOCH_EMUS_RASTERIZE_TRI_H

#include "helpers/int.h"

// All coordinates to PS1 are drawn with integer values

struct RT_POINT2D {
    i32 x, y;
    u32 u, v;
    u32 r, g, b;
};

struct RT_POINT2Df {
    float x, y;
    u32 u, v;
    u32 r, g, b;
};


struct RT_TEXTURE_SAMPLER;
typedef u16 (*RT_texture_sample_func)(RT_TEXTURE_SAMPLER *ts, i32 u, i32 v);
struct RT_TEXTURE_SAMPLER {
    u32 page_x, page_y, base_addr, clut_addr;
    u8 *VRAM;
    RT_texture_sample_func sample;
};

struct PS1_GPU;
void RT_draw_flat_triangle(PS1_GPU *this, RT_POINT2D *v0, RT_POINT2D *v1, RT_POINT2D *v2, u32 color);
struct PS1_GPU_TEXTURE_SAMPLER;
void RT_draw_flat_tex_triangle(PS1_GPU *this, RT_POINT2D *v0, RT_POINT2D *v1, RT_POINT2D *v2, PS1_GPU_TEXTURE_SAMPLER *ts);
void RT_draw_flat_tex_triangle_modulated(PS1_GPU *this, RT_POINT2D *v0, RT_POINT2D *v1, RT_POINT2D *v2, u32 color, PS1_GPU_TEXTURE_SAMPLER *ts);
void RT_draw_flat_tex_triangle_modulated_semi(PS1_GPU *this, RT_POINT2D *v0, RT_POINT2D *v1, RT_POINT2D *v2, u32 color, PS1_GPU_TEXTURE_SAMPLER *ts);
void RT_draw_shaded_tex_triangle_modulated(PS1_GPU *this, RT_POINT2D *v0, RT_POINT2D *v1, RT_POINT2D *v2, PS1_GPU_TEXTURE_SAMPLER *ts);
void RT_draw_shaded_tex_triangle_modulated_semi(PS1_GPU *this, RT_POINT2D *v0, RT_POINT2D *v1, RT_POINT2D *v2, PS1_GPU_TEXTURE_SAMPLER *ts);
void RT_draw_flat_tex_triangle_semi(PS1_GPU *this, RT_POINT2D *v0, RT_POINT2D *v1, RT_POINT2D *v2, PS1_GPU_TEXTURE_SAMPLER *ts);
void RT_draw_flat_triangle_semi(PS1_GPU *this, RT_POINT2D *v0, RT_POINT2D *v1, RT_POINT2D *v2, u32 r, u32 g, u32 b);
void RT_draw_shaded_triangle(PS1_GPU *this, RT_POINT2D *v0, RT_POINT2D *v1, RT_POINT2D *v2);
void RT_draw_shaded_triangle_semi(PS1_GPU *this, RT_POINT2D *v0, RT_POINT2D *v1, RT_POINT2D *v2);
#endif //JSMOOCH_EMUS_RASTERIZE_TRI_H

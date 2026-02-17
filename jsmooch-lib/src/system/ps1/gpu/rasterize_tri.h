//
// Created by . on 2/20/25.
//

#pragma once

#include "helpers/int.h"
namespace PS1::GPU {
    // All coordinates to PS1 are drawn with integer values

struct RT_POINT2D {
    void xy_from_cmd(u32 cmd) { x = cmd & 0xFFFF; y = cmd >> 16; x = SIGNe11to32(x); y = SIGNe11to32(y); }
    void color24_from_cmd(u32 cmd) { r = cmd & 0xFF; g = (cmd >> 8) & 0xFF; b = (cmd >> 16) & 0xFF; }
    void uv_from_cmd(u32 cmd) { u = cmd & 0xFF; v = (cmd >> 8) & 0xFF; }
    i32 x{}, y{};
    u32 u{}, v{};
    u32 r{}, g{}, b{};
};

struct RT_POINT2Df {
    float x{}, y{};
    u32 u{}, v{};
    u32 r{}, g{}, b{};
};


struct RT_TEXTURE_SAMPLER;
typedef u16 (*RT_texture_sample_func)(RT_TEXTURE_SAMPLER *ts, i32 u, i32 v);
struct core;
struct RT_TEXTURE_SAMPLER {
    u32 page_x{}, page_y{}, base_addr{}, clut_addr{};
    u8 *VRAM{};
    RT_texture_sample_func sample{};
};

}
//
// Created by . on 2/20/25.
//

#include "pixel_helpers.h"
#include "ps1_gpu.h"
#include "helpers/multisize_memaccess.cpp"

namespace PS1::GPU {
#define DOY (y + draw_y_offset)
#define DOX (x + draw_x_offset)

void core::setpix(i32 y, i32 x, u32 color, u32 is_tex, u32 tex_mask)
{
    // VRAM is 512 1024-wide 16-bit words. so 2048 bytes per line
    i32 ry = DOY;
    i32 rx = DOX;
    if ((ry < draw_area_top) || (ry > draw_area_bottom)) return;
    if ((rx < draw_area_left) || (rx > draw_area_right)) return;

    u32 addr = ((2048*ry)+(rx*2)) & 0xFFFFF;

    if (preserve_masked_pixels) {
        u16 v = cR16(VRAM, addr);
        if (v & 0x8000) return;
    }
    if (is_tex) {
        if (!(tex_mask || (color !=0))) return;
    }
    u32 mask_bit = is_tex * (tex_mask | force_set_mask_bit);
    cW16(VRAM, addr, color | mask_bit);
    //cW16(VRAM, addr, color);
}

void core::setpix_split(i32 y, i32 x, u32 r, u32 g, u32 b, u32 is_tex, u32 tex_mask)
{
    // VRAM is 512 1024-wide 16-bit words. so 2048 bytes per line
    i32 ry = DOY;
    i32 rx = DOX;
    if ((ry < draw_area_top) || (ry > draw_area_bottom)) return;
    if ((rx < draw_area_left) || (rx > draw_area_right)) return;
    u32 addr = (2048*ry)+(rx*2);

    if (preserve_masked_pixels) {
        u16 v = cR16(VRAM, addr);
        if (v & 0x8000) return;
    }
    u32 color = r | (g << 5) | (b << 10);
    if (is_tex) {
        if (!(tex_mask || (color !=0))) return;
    }
    u32 mask_bit = is_tex * (tex_mask | force_set_mask_bit);
    cW16(VRAM, addr, color | mask_bit);
}

static inline u32 BLEND1(u32 b, u32 f)
{
    // B + F
    u32 o = b + f;
    return o > 0x1F ? 0x1F : o;
}

static inline u32 BLEND2(u32 b, u32 f)
{
    // B - F
    i32 o = (i32)b - (i32)f;
    return o < 0 ? 0 : (u32)o;
}

static inline u32 BLEND3(u32 b, u32 f)
{
    // B + F/4
    u32 o = b + (f >> 2);
    return o > 0x1F ? 0x1F : o;
}


static u32 blend_semi15(u32 mode, u32 b, u32 f)
{
    // move 0, 5, 10
    // to 0, 8, 16
    // expand 15 to 24 bit, KINDA
    u32 b_b = (b >> 10) & 0x1F;
    u32 b_g = (b >> 5) & 0x1F;
    u32 b_r = b & 0x1F;

    u32 f_b = (f >> 10) & 0x1F;
    u32 f_g = (f >> 5) & 0x1F;
    u32 f_r = f & 0x1F;

    u32 o_b = 0;
    u32 o_g = 0;
    u32 o_r = 0;

    switch(mode) {
        case 0:
#define BLEND(b, f) (((b) >> 1) + ((f) >> 1))
            o_b = BLEND(b_b, f_b);
            o_g = BLEND(b_g, f_g);
            o_r = BLEND(b_r, f_r);
#undef BLEND
            break;
        case 1:
            o_b = BLEND1(b_b, f_b);
            o_g = BLEND1(b_g, f_g);
            o_r = BLEND1(b_r, f_r);
            break;
        case 2:
            o_b = BLEND2(b_b, f_b);
            o_g = BLEND2(b_g, f_g);
            o_r = BLEND2(b_r, f_r);
            break;
        case 3:
            o_b = BLEND3(b_b, f_b);
            o_g = BLEND3(b_g, f_g);
            o_r = BLEND3(b_r, f_r);
            break;
    }
    return (o_b << 10) | (o_g << 5) | o_r;
}

static u32 blend_semi15_split(u32 mode, u32 b, u32 f_r, u32 f_g, u32 f_b)
{
    // move 0, 5, 10
    // to 0, 8, 16
    // expand 15 to 24 bit, KINDA
    u32 b_b = (b >> 10) & 0x1F;
    u32 b_g = (b >> 5) & 0x1F;
    u32 b_r = b & 0x1F;

    u32 o_b = 0;
    u32 o_g = 0;
    u32 o_r = 0;

    switch(mode) {
        case 0:
#define BLEND(b, f) (((b) >> 1) + ((f) >> 1))
            o_b = BLEND(b_b, f_b);
            o_g = BLEND(b_g, f_g);
            o_r = BLEND(b_r, f_r);
#undef BLEND
            break;
        case 1:
            o_b = BLEND1(b_b, f_b);
            o_g = BLEND1(b_g, f_g);
            o_r = BLEND1(b_r, f_r);
            break;
        case 2:
            o_b = BLEND2(b_b, f_b);
            o_g = BLEND2(b_g, f_g);
            o_r = BLEND2(b_r, f_r);
            break;
        case 3:
            o_b = BLEND3(b_b, f_b);
            o_g = BLEND3(b_g, f_g);
            o_r = BLEND3(b_r, f_r);
            break;
    }
    return (o_b << 10) | (o_g << 5) | o_r;
}



void core::semipix(i32 y, i32 x, u32 color, u32 is_tex, u32 tex_mask)
{
    // VRAM is 512 1024-wide 16-bit words. so 2048 bytes per line
    i32 ry = DOY;
    i32 rx = DOX;
    if ((ry < draw_area_top) || (ry > draw_area_bottom)) return;
    if ((rx < draw_area_left) || (rx > draw_area_right)) return;
    u32 addr = ((2048*ry)+(rx*2)) & 0xFFFFF;

    if (preserve_masked_pixels) {
        u16 v = cR16(VRAM, addr);
        if (v & 0x8000) return;
    }

    if (tex_mask) {
        // First sample...
        u32 bg = cR16(VRAM, addr);

        // Then blend...
        color = blend_semi15(semi_transparency, bg, color);
    }
    else {
        if (color == 0) return;
    }

    // Now write...
    u32 mask_bit = is_tex * (tex_mask | force_set_mask_bit);
    cW16(VRAM, addr, color | mask_bit);
}

void core::semipixm(i32 y, i32 x, u32 color, u32 mode, u32 is_tex, u32 tex_mask)
{
    // VRAM is 512 1024-wide 16-bit words. so 2048 bytes per line
    i32 ry = DOY;
    i32 rx = DOX;
    if ((ry < draw_area_top) || (ry > draw_area_bottom)) return;
    if ((rx < draw_area_left) || (rx > draw_area_right)) return;
    u32 addr = ((2048*ry)+(rx*2)) & 0xFFFFF;

    if (preserve_masked_pixels) {
        u16 v = cR16(VRAM, addr);
        if (v & 0x8000) return;
    }

    if (tex_mask) {
        // First sample...
        u32 bg = cR16(VRAM, addr);

        // Then blend...
        color = blend_semi15(mode, bg, color);
    }
    else {
        if (color == 0) return;
    }

    // Now write...
    u32 mask_bit = is_tex * (tex_mask | force_set_mask_bit);
    cW16(VRAM, addr, color | mask_bit);
}

void core::semipix_split(i32 y, i32 x, u32 r, u32 g, u32 b, u32 is_tex, u32 tex_mask)
{
    // VRAM is 512 1024-wide 16-bit words. so 2048 bytes per line
    i32 ry = DOY;
    i32 rx = DOX;
    if ((ry < draw_area_top) || (ry > draw_area_bottom)) return;
    if ((rx < draw_area_left) || (rx > draw_area_right)) return;
    u32 addr = ((2048*ry)+(rx*2)) & 0xFFFFF;

    if (preserve_masked_pixels) {
        u16 v = cR16(VRAM, addr);
        if (v & 0x8000) return;
    }

    u32 color;
    if (tex_mask) {
        // First sample...
        u32 bg = cR16(VRAM, addr);

        // Then blend...
        color = blend_semi15_split(semi_transparency, bg, r, g, b);
    }
    else {
        color = r | (g << 5) | (b << 10);
        if (color == 0) return;
    }

    // Now write...
    u32 mask_bit = is_tex * (tex_mask | force_set_mask_bit);
    cW16(VRAM, addr, color | mask_bit);
}

}
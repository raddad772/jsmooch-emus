//
// Created by . on 1/2/25.
//

#ifndef JSMOOCH_EMUS_COLOR_H
#define JSMOOCH_EMUS_COLOR_H

#include <stdio.h>
#include "helpers/int.h"

static inline u32 ps1_to_screen(u32 color)
{
    u32 r = (color >> 10) & 0x1F;
    u32 g = (color >> 5) & 0x1F;
    u32 b = color & 0x1F;
    return 0xFF000000 | ((r << 3 | r >> 2) << 16) | ((g << 3 | g >> 2) << 8) | ((b << 3 | b >> 2));
}

//18 to 24-bit
static inline u32 nds_to_screen(u32 color)
{
    u32 b = (color >> 12) & 0x3F;
    u32 g = (color >> 6) & 0x3F;
    u32 r = color & 0x3F;
    //r = (r << 2) | (r >> 4);
    //g = (g << 2) | (g >> 4);
    //b = (b << 2) | (b >> 4);
    r <<= 2;
    g <<= 2;
    b <<= 2;
    return 0xFF000000 | (b << 16) | (g << 8) | r;
}

static const u32 tg16_lvl[8] = {
    0, 36, 72, 109,
    145, 182, 218, 255
};

static inline u32 tg16_to_screen(u32 color)
{
    u32 g = tg16_lvl[(color >> 6) & 7];
    u32 r = tg16_lvl[(color >> 3) & 7];
    u32 b = tg16_lvl[color & 7];
    return 0xFF000000 | r | (g << 8) | (b << 16);
}


static inline u32 gba_to_screen(u32 color)
{
    u32 r = (color >> 10) & 0x1F;
    u32 g = (color >> 5) & 0x1F;
    u32 b = color & 0x1F;
    return 0xFF000000 | ((r << 3 | r >> 2) << 16) | ((g << 3 | g >> 2) << 8) | ((b << 3 | b >> 2));
}

static inline u32 gba_coeff(u32 color, u32 co)
{
    i32 c = (i32)color;
    i32 b = (c >> 10) & 0x1F;
    i32 g = ((c >> 5) & 0x1F) << 1;
    g |= (c >> 15) & 1;
    i32 r = c & 0x1F;
    b = ((b * co) + 8) >> 4;
    g = ((g * co) + 8) >> 4;
    r = ((r * co) + 8) >> 4;
    return ((b << 16) | (g << 8) | r);
}

static inline u32 nds_coeff(u32 color, u32 co)
{
    i32 c = (i32)color;
    i32 b = (c >> 12) & 0x3F;
    i32 g = (c >> 6) & 0x3F;
    i32 r = c & 0x3F;
    b = ((b * co) + 8) >> 4;
    g = ((g * co) + 8) >> 4;
    r = ((r * co) + 8) >> 4;
    return ((b << 16) | (g << 8) | r);
}


static inline u32 nds_alpha(u32 target_a, u32 target_b, u32 co1, u32 co2)
{
    u32 c = nds_coeff(target_a, co1) + nds_coeff(target_b, co2);
    u32 b = (c >> 16) & 0xFF;
    u32 g = (c >> 8) & 0xFF;
    u32 r = c & 0xFF;
    if (b > 0x3F) b = 0x3F;
    if (g > 0x3F) g = 0x3F;
    if (r > 0x3F) r = 0x3F;
    return (b << 12) | (g << 6) | r;
}


static inline u32 gba_alpha(u32 target_a, u32 target_b, u32 co1, u32 co2)
{
    u32 c = gba_coeff(target_a, co1) + gba_coeff(target_b, co2);
    u32 b = (c >> 16) & 0xFF;
    u32 g = (c >> 8) & 0xFF;
    u32 r = c & 0xFF;
    if (b > 0x1F) b = 0x1F;
    if (g > 0x3F) g = 0x3F;
    if (r > 0x1F) r = 0x1F;
    return (b << 10) | ((g & 0x3E) << 4) | r | ((g & 1) << 15);
}

static inline u32 gba_brighten(u32 color, i32 co)
{
    i32 c = (i32)color;
    i32 b = (c >> 10) & 0x1F;
    i32 g = ((c >> 5) & 0x1F) << 1;
    g |= (c >> 15) & 1;
    i32 r = c & 0x1F;

    b += (((31 - b) * co) + 8) >> 4;
    g += (((63 - g) * co) + 8) >> 4;
    r += (((31 - r) * co) + 8) >> 4;

    return (b << 10) | ((g & 0x3E) << 4) | r | ((g & 1) << 15);
}

static inline u32 nds_brighten(u32 color, i32 co)
{
    i32 c = (i32)color;
    i32 b = (c >> 12) & 0x3F;
    i32 g = (c >> 6) & 0x3F;
    i32 r = c & 0x3F;

    b += (((63 - b) * co) + 8) >> 4;
    g += (((63 - g) * co) + 8) >> 4;
    r += (((63 - r) * co) + 8) >> 4;

    return (b << 12) | (g << 6) | r;
}


static inline u32 nds_darken(u32 color, i32 co)
{
    i32 c = (i32)color;
    i32 b = (c >> 12) & 0x3F;
    i32 g = (c >> 6) & 0x3F;
    i32 r = c & 0x3F;

    b -= ((b * co) + 7) >> 4;
    g -= ((g * co) + 7) >> 4;
    r -= ((r * co) + 7) >> 4;

    return (b << 12) | (g << 6) | r;
}


static inline u32 gba_darken(u32 color, i32 co)
{
    i32 c = (i32)color;
    i32 b = (c >> 10) & 0x1F;
    i32 g = ((c >> 5) & 0x1F) << 1;
    g |= (c >> 15) & 1;
    i32 r = c & 0x1F;

    b -= ((b * co) + 7) >> 4;
    g -= ((g * co) + 7) >> 4;
    r -= ((r * co) + 7) >> 4;

    return (b << 10) | ((g & 0x3E) << 4) | r | ((g & 1) << 15);
}

static inline u32 tg16_decode_line(u32 chr01, u32 chr23)
{
    u32 out = 0;
    for (u32 i = 0; i < 8; i++) {
        out <<= 4;
        u32 data = chr01 & 1;
        data |= (chr01 >> 7) & 2;
        data |= (chr23 << 2) & 4;
        data |= (chr23 >> 5) & 8;
        out |= data;
        chr01 >>= 1;
        chr23 >>= 1;
    }
    return out;
}


#endif //JSMOOCH_EMUS_COLOR_H

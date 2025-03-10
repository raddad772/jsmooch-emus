//
// Created by . on 1/2/25.
//

#ifndef JSMOOCH_EMUS_COLOR_H
#define JSMOOCH_EMUS_COLOR_H

#include "helpers/int.h"

static inline u32 ps1_to_screen(u32 color)
{
    u32 r = (color >> 10) & 0x1F;
    u32 g = (color >> 5) & 0x1F;
    u32 b = color & 0x1F;
    return 0xFF000000 | ((r << 3 | r >> 2) << 16) | ((g << 3 | g >> 2) << 8) | ((b << 3 | b >> 2));
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

#endif //JSMOOCH_EMUS_COLOR_H

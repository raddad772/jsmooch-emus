//
// Created by . on 2/20/25.
//

#ifndef JSMOOCH_EMUS_PIXEL_HELPERS_H
#define JSMOOCH_EMUS_PIXEL_HELPERS_H

#include "helpers/int.h"

struct PS1_GPU;
void setpix(struct PS1_GPU *this, i32 y, i32 x, u32 color, u32 is_tex, u32 tex_mask);
void setpix_split(struct PS1_GPU *this, i32 y, i32 x, u32 r, u32 g, u32 b, u32 is_tex, u32 tex_mask);
void semipix(struct PS1_GPU *this, i32 y, i32 x, u32 color, u32 is_tex, u32 tex_mask);
void semipixm(struct PS1_GPU *this, i32 y, i32 x, u32 color, u32 mode, u32 is_tex, u32 tex_mask);
void semipix_split(struct PS1_GPU *this, i32 y, i32 x, u32 r, u32 g, u32 b, u32 is_tex, u32 tex_mask);

static inline u32 BGR24to15(u32 c)
{
    return (((c >> 19) & 0x1F) << 10) |
           (((c >> 11) & 0x1F) << 5) |
           ((c >> 3) & 0x1F);
}

#endif //JSMOOCH_EMUS_PIXEL_HELPERS_H

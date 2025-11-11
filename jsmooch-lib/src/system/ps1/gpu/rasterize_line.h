//
// Created by . on 2/20/25.
//



#ifndef JSMOOCH_EMUS_RASTERIZE_LINE_H
#define JSMOOCH_EMUS_RASTERIZE_LINE_H

#include "helpers/int.h"

struct PS1_GPU;
void bresenham_opaque(PS1_GPU *this, RT_POINT2D *v1, RT_POINT2D *v2, u32 color);

#endif //JSMOOCH_EMUS_RASTERIZE_LINE_H

//
// Created by . on 2/20/25.
//
#include "ps1_gpu.h"
#include "rasterize_line.h"
#include "pixel_helpers.h"
namespace PS1::GPU {
void core::bresenham_opaque(RT_POINT2D *v1, RT_POINT2D *v2, u32 color)
{
    i32 m_new = 2 * (v2->y - v1->y);
    i32 slope_error_new = m_new - (v2->x - v1->x);
    for (i32 x = v1->x, y = v1->y; x <= v2->x; x++) {
        // Draw!
        setpix(y, x, color, 0, 0);

        // Add slope to increment angle formed
        slope_error_new += m_new;

        // Slope error reached limit, time to
        // increment y and update slope error.
        if (slope_error_new >= 0) {
            y++;
            slope_error_new -= 2 * (v2->x - v1->x);
        }
    }
}

#define CLAMP(x, low, high) ((x) < (low) ? (low) : ((x) > (high) ? (high) : (x)))

void core::bresenham_shaded_opaque(RT_POINT2D *v1, RT_POINT2D *v2)
{
    i32 dx = v2->x - v1->x;
    i32 dy = v2->y - v1->y;

    i32 m_new = 2 * dy;
    i32 slope_error_new = m_new - dx;

    // Fixed-point color interpolation (16.16)
    i32 r = v1->r << 16;
    i32 g = v1->g << 16;
    i32 b = v1->b << 16;

    i32 dr = ((v2->r - v1->r) << 16) / dx;
    i32 dg = ((v2->g - v1->g) << 16) / dx;
    i32 db = ((v2->b - v1->b) << 16) / dx;

    for (i32 x = v1->x, y = v1->y; x <= v2->x; x++) {
        i32 mr = dr >> 19;
        i32 mg = dg >> 19;
        i32 mb = dg >> 19;
        mr = CLAMP(mr, 0, 31) >> 3;
        mg = CLAMP(mg, 0, 31) >> 3;
        mb = CLAMP(mb, 0, 31) >> 3;
        setpix_split(y, x, mr, mg, mb, 0, 0);

        r += dr;
        g += dg;
        b += db;

        slope_error_new += m_new;
        if (slope_error_new >= 0) {
            y++;
            slope_error_new -= 2 * dx;
        }
    }
}

void core::bresenham_semi(RT_POINT2D *v1, RT_POINT2D *v2, u32 color)
{
    i32 m_new = 2 * (v2->y - v1->y);
    i32 slope_error_new = m_new - (v2->x - v1->x);
    for (i32 x = v1->x, y = v1->y; x <= v2->x; x++) {
        // Draw!
        semipix(y, x, color, 0, 0, true);

        // Add slope to increment angle formed
        slope_error_new += m_new;

        // Slope error reached limit, time to
        // increment y and update slope error.
        if (slope_error_new >= 0) {
            y++;
            slope_error_new -= 2 * (v2->x - v1->x);
        }
    }
}
}
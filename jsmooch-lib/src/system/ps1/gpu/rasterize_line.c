//
// Created by . on 2/20/25.
//
#include "ps1_gpu.h"
#include "rasterize_line.h"
#include "pixel_helpers.h"

void bresenham_opaque(struct PS1_GPU *this, RT_POINT2D *v1, RT_POINT2D *v2, u32 color)
{
    i32 m_new = 2 * (v2->y - v1->y);
    i32 slope_error_new = m_new - (v2->x - v1->x);
    for (i32 x = v1->x, y = v1->y; x <= v2->x; x++) {
        // Draw!
        setpix(this, y, x, color, 0, 0);

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

//
// Created by . on 2/20/25.
//

#include <cmath>

#include "rasterize_tri.h"
#include "pixel_helpers.h"
#include "ps1_gpu.h"

namespace PS1::GPU {

#define CLAMP(x, low, high) ((x) < (low) ? (low) : ((x) > (high) ? (high) : (x)))

static constexpr i32 dithertable[16] = {
    -4,  +0,  -3,  +1,
+2,  -2,  +3,  -1,
-3,  +1,  -4,  +0,
+3,  -1,  +2,  -2,
};

float edge_function (const RT_POINT2D *a, const RT_POINT2D *b, const RT_POINT2D *c)  {
    return (b->x - a->x) * (c->y - a->y) - (b->y - a->y) * (c->x - a->x);
};

#ifndef MIN
#define MIN(a,b) ((a) < (b) ? (a) : (b))
#endif

#ifndef MAX
#define MAX(a,b) ((a) > (b) ? (a) : (b))
#endif

static inline i32 MIN3(const i32 a, const i32 b, const i32 c)
{
    const i32 mab = MIN(a,b);
    return MIN(mab, c);
}

static inline i32 MAX3(const i32 a, const i32 b, const i32 c)
{
    const i32 mab = MAX(a,b);
    return MAX(mab, c);
}

static inline i64 cpz(RT_POINT2D *v0, RT_POINT2D *v1, RT_POINT2D *v2) {
    return (v1->x - v0->x) * (v2->y - v0->y) - (v1->y - v0->y) * (v2->x - v0->x);
}

static inline bool check3(RT_POINT2D *a, RT_POINT2D *b, RT_POINT2D *c) {
    i64 cp = cpz(a, b, c);
    if (cp < 0) return false;
    if (cp == 0) {
        if (b->y > a->y) return false;
        if (b->y == a->y && b->x < a->x) return false;
    }
    return true;
}

static inline bool is_inside_triangle(RT_POINT2D *p, RT_POINT2D *v0, RT_POINT2D *v1, RT_POINT2D *v2) {
    if (!check3(v0, v1, p)) return false;
    if (!check3(v1, v2, p)) return false;
    if (!check3(v2, v0, p)) return false;
    return true;
}

static inline void compute_barycentric(i64 cp, RT_POINT2D *p, RT_POINT2D *v0, RT_POINT2D *v1, RT_POINT2D *v2, i64 *lambdas) {
    if (cp == 0) {
        lambdas[0] = lambdas[1] = lambdas[2] = (1L << 32) / 3;
        return;
    }
    i64 r = (1L << 32) / cp;
    lambdas[0] = cpz(v1, v2, p) * r;
    lambdas[1] = cpz(v2, v0, p) * r;
    lambdas[2] = (1L << 32) - lambdas[0] - lambdas[1];
}

constexpr float EDGE_EPS = 1e-5f;

void core::RT_draw_flat_triangle(RT_POINT2D *v0, RT_POINT2D *v1, RT_POINT2D *v2, const u32 color) {
    const i32 minX = MIN3(v0->x, v1->x, v2->x);
    const i32 minY = MIN3(v0->y, v1->y, v2->y);
    const i32 maxX = MAX3(v0->x, v1->x, v2->x);
    const i32 maxY = MAX3(v0->y, v1->y, v2->y);
    if (((maxY - minY) > 511) || ((maxX - minX) > 1023)) return;

    i64 cross_product_z = cpz(v0, v1, v2);
    if (cross_product_z < 0) {
        RT_POINT2D *sa = v0;
        v0 = v1;
        v1 = sa;
        cross_product_z = cpz(v0, v1, v2);
    }

    // Initialise our point
    RT_POINT2D p;

    for (p.y = minY; p.y < maxY; p.y++) {
        for (p.x = minX; p.x < maxX; p.x++) {
            if (is_inside_triangle(&p, v0, v1, v2)) {
                // Draw the pixel
                setpix(p.y, p.x, color, 0);
            }
        }
    }
}

void core::RT_draw_flat_tex_triangle_modulated(RT_POINT2D *v0, RT_POINT2D *v1, RT_POINT2D *v2, u32 color, TEXTURE_SAMPLER *ts)
{
    const i32 minX = MIN3(v0->x, v1->x, v2->x);
    const i32 minY = MIN3(v0->y, v1->y, v2->y);
    const i32 maxX = MAX3(v0->x, v1->x, v2->x);
    const i32 maxY = MAX3(v0->y, v1->y, v2->y);
    if (((maxY - minY) > 511) || ((maxX - minX) > 1023)) return;

    i64 cross_product_z = cpz(v0, v1, v2);
    if (cross_product_z < 0) {
        RT_POINT2D *sa = v0;
        v0 = v1;
        v1 = sa;
        cross_product_z = cpz(v0, v1, v2);
    }
    i64 r_mul = (color & 0xFF) << 1;
    i64 g_mul = (color >> 7) & 0x1FE;
    i64 b_mul = (color >> 15) & 0x1FE;

    // Initialise our point
    RT_POINT2D p;
    i64 lambda[3];

    // Loop through all the pixels of the bounding box
    for (p.y = minY; p.y < maxY; p.y++) {
        for (p.x = minX; p.x < maxX; p.x++) {
            if (is_inside_triangle(&p, v0, v1, v2)) {
                compute_barycentric(cross_product_z, &p, v0, v1, v2, lambda);
                i64 u = ((lambda[0] * v0->u) + (lambda[1] * v1->u) + (lambda[2] * v2->u)) >> 32;
                i64 v = ((lambda[0] * v0->v) + (lambda[1] * v1->v) + (lambda[2] * v2->v)) >> 32;

                color = ts->sample(ts, static_cast<i32>(u) & 0xFF, static_cast<i32>(v) & 0xFF);
                if (color == 0) continue;
                i64 mr = ((color & 0x1f) * r_mul) >> 5;
                i64 mg = (((color >> 5) & 0x1f) * g_mul) >> 5;
                i64 mb = (((color >> 10) & 0x1f) * b_mul) >> 5;
                if (io.GPUSTAT.dither) {
                    u32 caddr = ditherP(p);
                    mr += dithertable[caddr];
                    mg += dithertable[caddr];
                    mb += dithertable[caddr];
                }
                mr = CLAMP(mr, 0, 255) >> 3;
                mg = CLAMP(mg, 0, 255) >> 3;
                mb = CLAMP(mb, 0, 255) >> 3;

                setpix_split(p.y, p.x, static_cast<u32>(mr), static_cast<u32>(mg), static_cast<u32>(mb), color & 0x8000);
            }
        }
    }
}

void core::RT_draw_flat_tex_triangle_modulated_semi(RT_POINT2D *v0, RT_POINT2D *v1, RT_POINT2D *v2, u32 color, TEXTURE_SAMPLER *ts) {
    const i32 minX = MIN3(v0->x, v1->x, v2->x);
    const i32 minY = MIN3(v0->y, v1->y, v2->y);
    const i32 maxX = MAX3(v0->x, v1->x, v2->x);
    const i32 maxY = MAX3(v0->y, v1->y, v2->y);
    if (((maxY - minY) > 511) || ((maxX - minX) > 1023)) return;

    i64 cross_product_z = cpz(v0, v1, v2);
    if (cross_product_z < 0) {
        RT_POINT2D *sa = v0;
        v0 = v1;
        v1 = sa;
        cross_product_z = cpz(v0, v1, v2);
    }

    i64 r_mul = (color & 0xFF) << 1;
    i64 g_mul = (color >> 7) & 0x1FE;
    i64 b_mul = (color >> 15) & 0x1FE;

    // Initialise our point
    RT_POINT2D p;
    i64 lambda[3];

    // Loop through all the pixels of the bounding box
    for (p.y = minY; p.y < maxY; p.y++) {
        for (p.x = minX; p.x < maxX; p.x++) {
            if (is_inside_triangle(&p, v0, v1, v2)) {
                compute_barycentric(cross_product_z, &p, v0, v1, v2, lambda);

                // Interpolate the colours at point P
                i64 u = ((lambda[0] * v0->u) + (lambda[1] * v1->u) + (lambda[2] * v2->u)) >> 32;
                i64 v = ((lambda[0] * v0->v) + (lambda[1] * v1->v) + (lambda[2] * v2->v)) >> 32;

                color = ts->sample(ts, static_cast<i32>(u) & 0xFF, static_cast<i32>(v) & 0xFF);
                if (color == 0) continue;
                i64 mr = ((color & 0x1f) * r_mul) >> 5;
                i64 mg = (((color >> 5) & 0x1f) * g_mul) >> 5;
                i64 mb = (((color >> 10) & 0x1f) * b_mul) >> 5;
                if (io.GPUSTAT.dither) {
                    u32 caddr = ditherP(p);
                    mr += dithertable[caddr];
                    mg += dithertable[caddr];
                    mb += dithertable[caddr];
                }
                mr = CLAMP(mr, 0, 255) >> 3;
                mg = CLAMP(mg, 0, 255) >> 3;
                mb = CLAMP(mb, 0, 255) >> 3;

                // Draw the pixe
                semipix_split(p.y, p.x, static_cast<u32>(mr), static_cast<u32>(mg), static_cast<u32>(mb), color & 0x8000, false);
            }
        }
    }
}

void core::RT_draw_shaded_tex_triangle_modulated_semi(RT_POINT2D *v0, RT_POINT2D *v1, RT_POINT2D *v2, TEXTURE_SAMPLER *ts)
{
    const i32 minX = MIN3(v0->x, v1->x, v2->x);
    const i32 minY = MIN3(v0->y, v1->y, v2->y);
    const i32 maxX = MAX3(v0->x, v1->x, v2->x);
    const i32 maxY = MAX3(v0->y, v1->y, v2->y);
    if (((maxY - minY) > 511) || ((maxX - minX) > 1023)) return;

    i64 cross_product_z = cpz(v0, v1, v2);
    if (cross_product_z < 0) {
        RT_POINT2D *sa = v0;
        v0 = v1;
        v1 = sa;
        cross_product_z = cpz(v0, v1, v2);
    }

    // Initialise our point
    RT_POINT2D p;
    i64 lambda[3];

    // Loop through all the pixels of the bounding box
    for (p.y = minY; p.y < maxY; p.y++) {
        for (p.x = minX; p.x < maxX; p.x++) {
            if (is_inside_triangle(&p, v0, v1, v2)) {
                compute_barycentric(cross_product_z, &p, v0, v1, v2, lambda);
                // Interpolate the colours at point P
                i64 u = ((lambda[0] * v0->u) + (lambda[1] * v1->u) + (lambda[2] * v2->u)) >> 32;
                i64 v = ((lambda[0] * v0->v) + (lambda[1] * v1->v) + (lambda[2] * v2->v)) >> 32;

                i64 r_mul = ((lambda[0] * v0->r) + (lambda[1] * v1->r) + (lambda[2] * v2->r));
                i64 g_mul = ((lambda[0] * v0->g) + (lambda[1] * v1->g) + (lambda[2] * v2->g));
                i64 b_mul = ((lambda[0] * v0->b) + (lambda[1] * v1->b) + (lambda[2] * v2->b));

                u32 color = ts->sample(ts, static_cast<i32>(u) & 0xFF, static_cast<i32>(v) & 0xFF);
                if (color == 0) continue;
                i64 mr = ((color & 0x1f) * r_mul) >> 37;
                i64 mg = (((color >> 5) & 0x1f) * g_mul) >> 37;
                i64 mb = (((color >> 10) & 0x1f) * b_mul) >> 37;
                if (io.GPUSTAT.dither) {
                    u32 caddr = ditherP(p);
                    mr += dithertable[caddr];
                    mg += dithertable[caddr];
                    mb += dithertable[caddr];
                }
                mr = CLAMP(mr, 0, 255) >> 3;
                mg = CLAMP(mg, 0, 255) >> 3;
                mb = CLAMP(mb, 0, 255) >> 3;

                // Draw the pixe
                semipix_split(p.y, p.x, static_cast<u32>(mr), static_cast<u32>(mg), static_cast<u32>(mb), color & 0x8000, false);
            }
        }
    }
}

void core::RT_draw_shaded_tex_triangle_modulated(RT_POINT2D *v0, RT_POINT2D *v1, RT_POINT2D *v2, TEXTURE_SAMPLER *ts)
{
    const i32 minX = MIN3(v0->x, v1->x, v2->x);
    const i32 minY = MIN3(v0->y, v1->y, v2->y);
    const i32 maxX = MAX3(v0->x, v1->x, v2->x);
    const i32 maxY = MAX3(v0->y, v1->y, v2->y);
    if (((maxY - minY) > 511) || ((maxX - minX) > 1023)) return;

    i64 cross_product_z = cpz(v0, v1, v2);
    if (cross_product_z < 0) {
        RT_POINT2D *sa = v0;
        v0 = v1;
        v1 = sa;
        cross_product_z = cpz(v0, v1, v2);
    }

    // Initialise our point
    RT_POINT2D p;
    i64 lambda[3];

    // Loop through all the pixels of the bounding box
    for (p.y = minY; p.y < maxY; p.y++) {
        for (p.x = minX; p.x < maxX; p.x++) {
            if (is_inside_triangle(&p, v0, v1, v2)) {
                compute_barycentric(cross_product_z, &p, v0, v1, v2, lambda);
                // Interpolate the colours at point P
                i64 u = ((lambda[0] * v0->u) + (lambda[1] * v1->u) + (lambda[2] * v2->u)) >> 32;
                i64 v = ((lambda[0] * v0->v) + (lambda[1] * v1->v) + (lambda[2] * v2->v)) >> 32;

                i64 r_mul = ((lambda[0] * v0->r) + (lambda[1] * v1->r) + (lambda[2] * v2->r));
                i64 g_mul = ((lambda[0] * v0->g) + (lambda[1] * v1->g) + (lambda[2] * v2->g));
                i64 b_mul = ((lambda[0] * v0->b) + (lambda[1] * v1->b) + (lambda[2] * v2->b));

                u32 color = ts->sample(ts, static_cast<i32>(u) & 0xFF, static_cast<i32>(v) & 0xFF);
                if (color == 0) continue;
                i64 mr = ((color & 0x1f) * r_mul) >> 36;
                i64 mg = (((color >> 5) & 0x1f) * g_mul) >> 36;
                i64 mb = (((color >> 10) & 0x1f) * b_mul) >> 36;
                if (io.GPUSTAT.dither) {
                    u32 caddr = ditherP(p);
                    mr += dithertable[caddr];
                    mg += dithertable[caddr];
                    mb += dithertable[caddr];
                }
                mr = CLAMP(mr, 0, 255) >> 3;
                mg = CLAMP(mg, 0, 255) >> 3;
                mb = CLAMP(mb, 0, 255) >> 3;
                u32 c = mr | (mg << 5) | (mb << 10);

                // Draw the pixe
                setpix(p.y, p.x, c | (color & 0x8000), color);
            }
        }
    }
}

void core::RT_draw_flat_tex_triangle(RT_POINT2D *v0, RT_POINT2D *v1, RT_POINT2D *v2, TEXTURE_SAMPLER *ts)
{
    const i32 minX = MIN3(v0->x, v1->x, v2->x);
    const i32 minY = MIN3(v0->y, v1->y, v2->y);
    const i32 maxX = MAX3(v0->x, v1->x, v2->x);
    const i32 maxY = MAX3(v0->y, v1->y, v2->y);
    if (((maxY - minY) > 511) || ((maxX - minX) > 1023)) return;

    i64 cross_product_z = cpz(v0, v1, v2);
    if (cross_product_z < 0) {
        RT_POINT2D *sa = v0;
        v0 = v1;
        v1 = sa;
        cross_product_z = cpz(v0, v1, v2);
    }

    // Initialise our point
    RT_POINT2D p;
    i64 lambda[3];

    // Loop through all the pixels of the bounding box
    for (p.y = minY; p.y < maxY; p.y++) {
        for (p.x = minX; p.x < maxX; p.x++) {
            if (is_inside_triangle(&p, v0, v1, v2)) {
                compute_barycentric(cross_product_z, &p, v0, v1, v2, lambda);

                // Interpolate the colours at point P
                i64 u = ((lambda[0] * v0->u) + (lambda[1] * v1->u) + (lambda[2] * v2->u)) >> 32;
                i64 v = ((lambda[0] * v0->v) + (lambda[1] * v1->v) + (lambda[2] * v2->v)) >> 32;

                u32 color = ts->sample(ts, static_cast<i32>(u) & 0xFF, static_cast<i32>(v) & 0xFF);
                if (color == 0) continue;

                // Draw the pixel
                setpix(p.y, p.x, color & 0x7FFF, color & 0x8000);
            }
        }
    }
}


void core::RT_draw_flat_tex_triangle_semi(RT_POINT2D *v0, RT_POINT2D *v1, RT_POINT2D *v2, TEXTURE_SAMPLER *ts)
{
    const i32 minX = MIN3(v0->x, v1->x, v2->x);
    const i32 minY = MIN3(v0->y, v1->y, v2->y);
    const i32 maxX = MAX3(v0->x, v1->x, v2->x);
    const i32 maxY = MAX3(v0->y, v1->y, v2->y);
    if (((maxY - minY) > 511) || ((maxX - minX) > 1023)) return;

    i64 cross_product_z = cpz(v0, v1, v2);
    if (cross_product_z < 0) {
        RT_POINT2D *sa = v0;
        v0 = v1;
        v1 = sa;
        cross_product_z = cpz(v0, v1, v2);
    }

    // Initialise our point
    RT_POINT2D p;
    i64 lambda[3];

    // Loop through all the pixels of the bounding box
    for (p.y = minY; p.y < maxY; p.y++) {
        for (p.x = minX; p.x < maxX; p.x++) {
            if (is_inside_triangle(&p, v0, v1, v2)) {
                compute_barycentric(cross_product_z, &p, v0, v1, v2, lambda);
                i64 u = ((lambda[0] * v0->u) + (lambda[1] * v1->u) + (lambda[2] * v2->u)) >> 32;
                i64 v = ((lambda[0] * v0->v) + (lambda[1] * v1->v) + (lambda[2] * v2->v)) >> 32;

                u16 color = ts->sample(ts, static_cast<i32>(u) & 0xFF, static_cast<i32>(v) & 0xFF);
                if (color == 0) continue;

                // Draw the pixel
                semipixm(p.y, p.x, color & 0x7FFF, ts->semi_mode, color & 0x8000, false);
            }
        }
    }
}

void core::RT_draw_flat_triangle_semi(RT_POINT2D *v0, RT_POINT2D *v1, RT_POINT2D *v2, u32 r, u32 g, u32 b) {
    const i32 minX = MIN3(v0->x, v1->x, v2->x);
    const i32 minY = MIN3(v0->y, v1->y, v2->y);
    const i32 maxX = MAX3(v0->x, v1->x, v2->x);
    const i32 maxY = MAX3(v0->y, v1->y, v2->y);
    if (((maxY - minY) > 511) || ((maxX - minX) > 1023)) return;

    i64 cross_product_z = cpz(v0, v1, v2);
    if (cross_product_z < 0) {
        RT_POINT2D *sa = v0;
        v0 = v1;
        v1 = sa;
        cross_product_z = cpz(v0, v1, v2);
    }

    // Initialise our point
    RT_POINT2D p;

    // Loop through all the pixels of the bounding box
    for (p.y = minY; p.y < maxY; p.y++) {
        for (p.x = minX; p.x < maxX; p.x++) {
            if (is_inside_triangle(&p, v0, v1, v2)) {
                // Draw the pixel
                semipix_split(p.y, p.x, r >> 3, g >> 3, b >> 3, 0, true);
            }
        }
    }
}


void core::RT_draw_shaded_triangle_semi(RT_POINT2D *v0, RT_POINT2D *v1, RT_POINT2D *v2) {
    const i32 minX = MIN3(v0->x, v1->x, v2->x);
    const i32 minY = MIN3(v0->y, v1->y, v2->y);
    const i32 maxX = MAX3(v0->x, v1->x, v2->x);
    const i32 maxY = MAX3(v0->y, v1->y, v2->y);
    if (((maxY - minY) > 511) || ((maxX - minX) > 1023)) return;

    i64 cross_product_z = cpz(v0, v1, v2);
    if (cross_product_z < 0) {
        RT_POINT2D *sa = v0;
        v0 = v1;
        v1 = sa;
        cross_product_z = cpz(v0, v1, v2);
    }

    // Initialise our point
    RT_POINT2D p;
    i64 lambda[3];

    // Loop through all the pixels of the bounding box
    for (p.y = minY; p.y < maxY; p.y++) {
        for (p.x = minX; p.x < maxX; p.x++) {
            if (is_inside_triangle(&p, v0, v1, v2)) {
                compute_barycentric(cross_product_z, &p, v0, v1, v2, lambda);
                // Interpolate the colors at point P
                i64 mr = ((lambda[0] * v0->r) + (lambda[1] * v1->r) + (lambda[2] * v2->r)) >> 32;
                i64 mg = ((lambda[0] * v0->g) + (lambda[1] * v1->g) + (lambda[2] * v2->g)) >> 32;
                i64 mb = ((lambda[0] * v0->b) + (lambda[1] * v1->b) + (lambda[2] * v2->b)) >> 32;
                if (io.GPUSTAT.dither) {
                    u32 caddr = ditherP(p);
                    mr += dithertable[caddr];
                    mg += dithertable[caddr];
                    mb += dithertable[caddr];
                }
                mr = CLAMP(mr, 0, 255) >> 3;
                mg = CLAMP(mg, 0, 255) >> 3;
                mb = CLAMP(mb, 0, 255) >> 3;

                // Draw the pixel
                semipix_split(p.y, p.x, static_cast<u32>(mr) >> 3, static_cast<u32>(mg) >> 3, static_cast<u32>(mb) >> 3, 0, true);
            }
        }
    }
}

void core::RT_draw_shaded_triangle(RT_POINT2D *v0, RT_POINT2D *v1, RT_POINT2D *v2) {
    //float ABC = edge_function(v0, v1, v2);
    // Calculate the edge function for the whole triangle (ABC)
    const i32 minX = MIN3(v0->x, v1->x, v2->x);
    const i32 minY = MIN3(v0->y, v1->y, v2->y);
    const i32 maxX = MAX3(v0->x, v1->x, v2->x);
    const i32 maxY = MAX3(v0->y, v1->y, v2->y);
    if (((maxY - minY) > 511) || ((maxX - minX) > 1023)) return;

    i64 cross_product_z = cpz(v0, v1, v2);
    if (cross_product_z < 0) {
        RT_POINT2D *sa = v0;
        v0 = v1;
        v1 = sa;
        cross_product_z = cpz(v0, v1, v2);
    }

    // Initialise our point
    RT_POINT2D p;
    i64 lambda[3];

    // Loop through all the pixels of the bounding box
    for (p.y = minY; p.y < maxY; p.y++) {
        for (p.x = minX; p.x < maxX; p.x++) {
            if (is_inside_triangle(&p, v0, v1, v2)) {
                compute_barycentric(cross_product_z, &p, v0, v1, v2, lambda);
                i64 r = ((lambda[0] * v0->r) + (lambda[1] * v1->r) + (lambda[2] * v2->r)) >> 32;
                i64 g = ((lambda[0] * v0->g) + (lambda[1] * v1->g) + (lambda[2] * v2->g)) >> 32;
                i64 b = ((lambda[0] * v0->b) + (lambda[1] * v1->b) + (lambda[2] * v2->b)) >> 32;
                if (io.GPUSTAT.dither) {
                    u32 caddr = ditherP(p);
                    r += dithertable[caddr];
                    g += dithertable[caddr];
                    b += dithertable[caddr];
                }
                r = CLAMP(r, 0, 255) >> 3;
                g = CLAMP(g, 0, 255) >> 3;
                b = CLAMP(b, 0, 255) >> 3;
                setpix_split(p.y, p.x, static_cast<u32>(r), static_cast<u32>(g), static_cast<u32>(b), 0);
            }
        }
    }
}
}

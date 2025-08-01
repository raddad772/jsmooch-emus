//
// Created by . on 2/20/25.
//

#include <math.h>
#include <stdio.h>

#include "rasterize_tri.h"
#include "pixel_helpers.h"
#include "ps1_gpu.h"
#include "helpers/multisize_memaccess.c"

float edge_function (struct RT_POINT2D *a, struct RT_POINT2D *b, struct RT_POINT2D *c)  {
    return (b->x - a->x) * (c->y - a->y) - (b->y - a->y) * (c->x - a->x);
};


#ifndef MIN
#define MIN(a,b) ((a) < (b) ? (a) : (b))
#endif

#ifndef MAX
#define MAX(a,b) ((a) > (b) ? (a) : (b))
#endif

static inline i32 MIN3(i32 a, i32 b, i32 c)
{
    i32 mab = MIN(a,b);
    return MIN(mab, c);
}

static inline i32 MAX3(i32 a, i32 b, i32 c)
{
    i32 mab = MAX(a,b);
    return MAX(mab, c);
}

void RT_draw_flat_triangle(struct PS1_GPU *this, struct RT_POINT2D *v0, struct RT_POINT2D *v1, struct RT_POINT2D *v2, u32 color) {
    struct RT_POINT2D *sa;

    // Calculate the edge function for the whole triangle (ABC)
    float ABC = edge_function(v0, v1, v2);

    if (ABC < 0) {
        sa = v2;
        v2 = v1;
        v1 = sa;
    }

    // Initialise our point
    struct RT_POINT2D p;
    p.x = p.y = 0;

    // Get the bounding box of the triangle
    i32 minX = MIN3(v0->x, v1->x, v2->x);
    i32 minY = MIN3(v0->y, v1->y, v2->y);
    i32 maxX = MAX3(v0->x, v1->x, v2->x);
    i32 maxY = MAX3(v0->y, v1->y, v2->y);

    // Get edges for top-left testing...
    struct RT_POINT2Df edge0, edge1, edge2;
    edge0.x = (float) (v2->x - v1->x);
    edge0.y = (float) (v2->y - v1->y);

    edge1.x = (float) (v0->x - v2->x);
    edge1.y = (float) (v0->y - v2->y);

    edge2.x = (float) (v1->x - v0->x);
    edge2.y = (float) (v1->y - v0->y);

    // Loop through all the pixels of the bounding box
    for (p.y = minY; p.y < maxY; p.y++) {
        for (p.x = minX; p.x < maxX; p.x++) {
            // Calculate our edge functions
            float w0 = edge_function(v1, v2, &p); // w0
            float w1 = edge_function(v2, v0, &p); // w2
            float w2 = edge_function(v0, v1, &p); // w1

            u32 overlaps = w1 >= 0 && w0 >= 0 && w2 >= 0;

            // If the point is on the edge, test if it is a top or left edge,
            // otherwise test if the edge function is positive
            overlaps &= (w0 == 0 ? ((edge0.y == 0 && edge0.x > 0) || edge0.y > 0) : (w0 > 0));
            overlaps &= (w1 == 0 ? ((edge1.y == 0 && edge1.x > 0) || edge1.y > 0) : (w1 > 0));
            overlaps &= (w2 == 0 ? ((edge2.y == 0 && edge2.x > 0) || edge2.y > 0) : (w2 > 0));

            // If all the edge functions are positive, the point is inside the triangle
            if (overlaps) {
                // Draw the pixel
                setpix(this, p.y, p.x, color, 0, 0);
            }
        }
    }
}

void RT_draw_flat_tex_triangle_modulated(struct PS1_GPU *this, struct RT_POINT2D *v0, struct RT_POINT2D *v1, struct RT_POINT2D *v2, u32 color, struct PS1_GPU_TEXTURE_SAMPLER *ts)
{
    struct RT_POINT2D *sa;

    // Calculate the edge function for the whole triangle (ABC)
    float ABC = edge_function(v0, v1, v2);

    if (ABC < 0) {
        sa = v2;
        v2 = v1;
        v1 = sa;
        ABC = edge_function(v0, v1, v2);
    }

    float ABCr = 1.0f / ABC;

    static const float rp = 1.0f / 128.0f;
    float r = ((float)(color & 0xFF)) * rp;
    float g = ((float)((color >> 8) & 0xFF)) * rp;
    float b = ((float)((color >> 16) & 0xFF)) * rp;

    float v0u = (float)v0->u;
    float v0v = (float)v0->v;

    float v1u = (float)v1->u;
    float v1v = (float)v1->v;

    float v2u = (float)v2->u;
    float v2v = (float)v2->v;

    // Initialise our point
    struct RT_POINT2D p;
    p.x = p.y = 0;

    // Get the bounding box of the triangle
    i32 minX = MIN3(v0->x, v1->x, v2->x);
    i32 minY = MIN3(v0->y, v1->y, v2->y);
    i32 maxX = MAX3(v0->x, v1->x, v2->x);
    i32 maxY = MAX3(v0->y, v1->y, v2->y);

    // Get edges for top-left testing...
    struct RT_POINT2Df edge0, edge1, edge2;
    edge0.x = (float) (v2->x - v1->x);
    edge0.y = (float) (v2->y - v1->y);

    edge1.x = (float) (v0->x - v2->x);
    edge1.y = (float) (v0->y - v2->y);

    edge2.x = (float) (v1->x - v0->x);
    edge2.y = (float) (v1->y - v0->y);

    // Loop through all the pixels of the bounding box
    for (p.y = minY; p.y < maxY; p.y++) {
        for (p.x = minX; p.x < maxX; p.x++) {
            // Calculate our edge functions
            float w0 = edge_function(v1, v2, &p); // w0 BCP
            float w1 = edge_function(v2, v0, &p); // w1 CAP
            float w2 = edge_function(v0, v1, &p); // w2 ABP

            // Normalise the edge functions by dividing by the total area to get the barycentric coordinates

            u32 overlaps = w1 >= 0 && w0 >= 0 && w2 >= 0;

            // If the point is on the edge, test if it is a top or left edge,
            // otherwise test if the edge function is positive
            overlaps &= (w0 == 0 ? ((edge0.y == 0 && edge0.x > 0) || edge0.y > 0) : (w0 > 0));
            overlaps &= (w1 == 0 ? ((edge1.y == 0 && edge1.x > 0) || edge1.y > 0) : (w1 > 0));
            overlaps &= (w2 == 0 ? ((edge2.y == 0 && edge2.x > 0) || edge2.y > 0) : (w2 > 0));

            // If all the edge functions are positive, the point is inside the triangle
            if (overlaps) {
                float weightA = w0 * ABCr;
                float weightB = w1 * ABCr;
                float weightC = w2 * ABCr;
                // Interpolate the colours at point P
                float u = v0u * weightA + v1u * weightB + v2u * weightC;
                float v = v0v * weightA + v1v * weightB + v2v * weightC;

                color = ts->sample(ts, (i32)u, (i32)v);
                float mb = (float)(((color >> 10) & 0x1F)) * b;
                float mg = (float)((color >> 5) & 0x1F) * g;
                float mr = (float)(color & 0x1F) * r;
                if (mr > 31.0f) mr = 31.0f;
                if (mg > 31.0f) mg = 31.0f;
                if (mb > 31.0f) mb = 31.0f;

                // Draw the pixe
                setpix_split(this, p.y, p.x, (u32)mr, (u32)mg, (u32)mb, 1, color & 0x8000);
            }
        }
    }
}

void RT_draw_flat_tex_triangle_modulated_semi(struct PS1_GPU *this, struct RT_POINT2D *v0, struct RT_POINT2D *v1, struct RT_POINT2D *v2, u32 color, struct PS1_GPU_TEXTURE_SAMPLER *ts) {
    struct RT_POINT2D *sa;

    // Calculate the edge function for the whole triangle (ABC)
    float ABC = edge_function(v0, v1, v2);

    if (ABC < 0) {
        sa = v2;
        v2 = v1;
        v1 = sa;
        ABC = edge_function(v0, v1, v2);
    }

    float ABCr = 1.0f / ABC;

    static const float rp = 1.0f / 128.0f;
    float r = ((float) (color & 0xFF)) * rp;
    float g = ((float) ((color >> 8) & 0xFF)) * rp;
    float b = ((float) ((color >> 16) & 0xFF)) * rp;

    float v0u = (float) v0->u;
    float v0v = (float) v0->v;

    float v1u = (float) v1->u;
    float v1v = (float) v1->v;

    float v2u = (float) v2->u;
    float v2v = (float) v2->v;

    // Initialise our point
    struct RT_POINT2D p;
    p.x = p.y = 0;

    // Get the bounding box of the triangle
    i32 minX = MIN3(v0->x, v1->x, v2->x);
    i32 minY = MIN3(v0->y, v1->y, v2->y);
    i32 maxX = MAX3(v0->x, v1->x, v2->x);
    i32 maxY = MAX3(v0->y, v1->y, v2->y);

    // Get edges for top-left testing...
    struct RT_POINT2Df edge0, edge1, edge2;
    edge0.x = (float) (v2->x - v1->x);
    edge0.y = (float) (v2->y - v1->y);

    edge1.x = (float) (v0->x - v2->x);
    edge1.y = (float) (v0->y - v2->y);

    edge2.x = (float) (v1->x - v0->x);
    edge2.y = (float) (v1->y - v0->y);

    // Loop through all the pixels of the bounding box
    for (p.y = minY; p.y < maxY; p.y++) {
        for (p.x = minX; p.x < maxX; p.x++) {
            // Calculate our edge functions
            float w0 = edge_function(v1, v2, &p); // w0 BCP
            float w1 = edge_function(v2, v0, &p); // w1 CAP
            float w2 = edge_function(v0, v1, &p); // w2 ABP

            // Normalise the edge functions by dividing by the total area to get the barycentric coordinates
            u32 overlaps = w1 >= 0 && w0 >= 0 && w2 >= 0;

            // If the point is on the edge, test if it is a top or left edge,
            // otherwise test if the edge function is positive
            overlaps &= (w0 == 0 ? ((edge0.y == 0 && edge0.x > 0) || edge0.y > 0) : (w0 > 0));
            overlaps &= (w1 == 0 ? ((edge1.y == 0 && edge1.x > 0) || edge1.y > 0) : (w1 > 0));
            overlaps &= (w2 == 0 ? ((edge2.y == 0 && edge2.x > 0) || edge2.y > 0) : (w2 > 0));

            // If all the edge functions are positive, the point is inside the triangle
            if (overlaps) {
                float weightA = w0 * ABCr;
                float weightB = w1 * ABCr;
                float weightC = w2 * ABCr;
                // Interpolate the colours at point P
                float u = v0u * weightA + v1u * weightB + v2u * weightC;
                float v = v0v * weightA + v1v * weightB + v2v * weightC;

                color = ts->sample(ts, (i32) u, (i32) v);
                float mb = (float) (((color >> 10) & 0x1F)) * b;
                float mg = (float) ((color >> 5) & 0x1F) * g;
                float mr = (float) (color & 0x1F) * r;
                if (mr > 31.0f) mr = 31.0f;
                if (mg > 31.0f) mg = 31.0f;
                if (mb > 31.0f) mb = 31.0f;

                // Draw the pixe
                semipix_split(this, p.y, p.x, (u32)mr, (u32)mg, (u32)mb, 1, color & 0x8000);
            }
        }
    }
}

void RT_draw_shaded_tex_triangle_modulated_semi(struct PS1_GPU *this, struct RT_POINT2D *v0, struct RT_POINT2D *v1, struct RT_POINT2D *v2, struct PS1_GPU_TEXTURE_SAMPLER *ts)
{
    struct RT_POINT2D *sa;

    // Calculate the edge function for the whole triangle (ABC)
    float ABC = edge_function(v0, v1, v2);

    if (ABC < 0) {
        sa = v2;
        v2 = v1;
        v1 = sa;
        ABC = edge_function(v0, v1, v2);
    }

    float ABCr = 1.0f / ABC;

    static const float rp = 1.0f / 128.0f;
    float v0r = (float)v0->r * rp;
    float v0g = (float)v0->g * rp;
    float v0b = (float)v0->b * rp;

    float v1r = (float)v1->r * rp;
    float v1g = (float)v1->g * rp;
    float v1b = (float)v1->b * rp;

    float v2r = (float)v2->r * rp;
    float v2g = (float)v2->g * rp;
    float v2b = (float)v2->b * rp;

    float v0u = (float)v0->u;
    float v0v = (float)v0->v;

    float v1u = (float)v1->u;
    float v1v = (float)v1->v;

    float v2u = (float)v2->u;
    float v2v = (float)v2->v;

    // Initialise our point
    struct RT_POINT2D p;
    p.x = p.y = 0;

    // Get the bounding box of the triangle
    i32 minX = MIN3(v0->x, v1->x, v2->x);
    i32 minY = MIN3(v0->y, v1->y, v2->y);
    i32 maxX = MAX3(v0->x, v1->x, v2->x);
    i32 maxY = MAX3(v0->y, v1->y, v2->y);

    // Get edges for top-left testing...
    struct RT_POINT2Df edge0, edge1, edge2;
    edge0.x = (float) (v2->x - v1->x);
    edge0.y = (float) (v2->y - v1->y);

    edge1.x = (float) (v0->x - v2->x);
    edge1.y = (float) (v0->y - v2->y);

    edge2.x = (float) (v1->x - v0->x);
    edge2.y = (float) (v1->y - v0->y);

    // Loop through all the pixels of the bounding box
    for (p.y = minY; p.y < maxY; p.y++) {
        for (p.x = minX; p.x < maxX; p.x++) {
            // Calculate our edge functions
            float w0 = edge_function(v1, v2, &p); // w0 BCP
            float w1 = edge_function(v2, v0, &p); // w1 CAP
            float w2 = edge_function(v0, v1, &p); // w2 ABP

            // Normalise the edge functions by dividing by the total area to get the barycentric coordinates

            u32 overlaps = w1 >= 0 && w0 >= 0 && w2 >= 0;

            // If the point is on the edge, test if it is a top or left edge,
            // otherwise test if the edge function is positive
            overlaps &= (w0 == 0 ? ((edge0.y == 0 && edge0.x > 0) || edge0.y > 0) : (w0 > 0));
            overlaps &= (w1 == 0 ? ((edge1.y == 0 && edge1.x > 0) || edge1.y > 0) : (w1 > 0));
            overlaps &= (w2 == 0 ? ((edge2.y == 0 && edge2.x > 0) || edge2.y > 0) : (w2 > 0));

            // If all the edge functions are positive, the point is inside the triangle
            if (overlaps) {
                float weightA = w0 * ABCr;
                float weightB = w1 * ABCr;
                float weightC = w2 * ABCr;
                // Interpolate the colours at point P
                float u = v0u * weightA + v1u * weightB + v2u * weightC;
                float v = v0v * weightA + v1v * weightB + v2v * weightC;

                float r = v0r * weightA + v1r * weightB + v2r * weightC;
                float g = v0g * weightA + v1g * weightB + v2g * weightC;
                float b = v0b * weightA + v1b * weightB + v2b * weightC;

                u32 color = ts->sample(ts, (i32)u, (i32)v);
                float mb = (float)((color >> 10) & 0x1F) * b;
                float mg = (float)((color >> 5) & 0x1F) * g;
                float mr = (float)(color & 0x1F) * r;
                if (mr > 31.0f) mr = 31.0f;
                if (mg > 31.0f) mg = 31.0f;
                if (mb > 31.0f) mb = 31.0f;

                // Draw the pixe
                semipix_split(this, p.y, p.x, (u32)mr, (u32)mg, (u32)mb, 1, color & 0x8000);
            }
        }
    }
}

void RT_draw_shaded_tex_triangle_modulated(struct PS1_GPU *this, struct RT_POINT2D *v0, struct RT_POINT2D *v1, struct RT_POINT2D *v2, struct PS1_GPU_TEXTURE_SAMPLER *ts)
{
    struct RT_POINT2D *sa;

    // Calculate the edge function for the whole triangle (ABC)
    float ABC = edge_function(v0, v1, v2);

    if (ABC < 0) {
        sa = v2;
        v2 = v1;
        v1 = sa;
        ABC = edge_function(v0, v1, v2);
    }

    float ABCr = 1.0f / ABC;

    static const float rp = 1.0f / 128.0f;
    float v0r = (float)v0->r * rp;
    float v0g = (float)v0->g * rp;
    float v0b = (float)v0->b * rp;

    float v1r = (float)v1->r * rp;
    float v1g = (float)v1->g * rp;
    float v1b = (float)v1->b * rp;

    float v2r = (float)v2->r * rp;
    float v2g = (float)v2->g * rp;
    float v2b = (float)v2->b * rp;

    float v0u = (float)v0->u;
    float v0v = (float)v0->v;

    float v1u = (float)v1->u;
    float v1v = (float)v1->v;

    float v2u = (float)v2->u;
    float v2v = (float)v2->v;

    // Initialise our point
    struct RT_POINT2D p;
    p.x = p.y = 0;

    // Get the bounding box of the triangle
    i32 minX = MIN3(v0->x, v1->x, v2->x);
    i32 minY = MIN3(v0->y, v1->y, v2->y);
    i32 maxX = MAX3(v0->x, v1->x, v2->x);
    i32 maxY = MAX3(v0->y, v1->y, v2->y);

    // Get edges for top-left testing...
    struct RT_POINT2Df edge0, edge1, edge2;
    edge0.x = (float) (v2->x - v1->x);
    edge0.y = (float) (v2->y - v1->y);

    edge1.x = (float) (v0->x - v2->x);
    edge1.y = (float) (v0->y - v2->y);

    edge2.x = (float) (v1->x - v0->x);
    edge2.y = (float) (v1->y - v0->y);

    // Loop through all the pixels of the bounding box
    for (p.y = minY; p.y < maxY; p.y++) {
        for (p.x = minX; p.x < maxX; p.x++) {
            // Calculate our edge functions
            float w0 = edge_function(v1, v2, &p); // w0 BCP
            float w1 = edge_function(v2, v0, &p); // w1 CAP
            float w2 = edge_function(v0, v1, &p); // w2 ABP

            // Normalise the edge functions by dividing by the total area to get the barycentric coordinates

            u32 overlaps = w1 >= 0 && w0 >= 0 && w2 >= 0;

            // If the point is on the edge, test if it is a top or left edge,
            // otherwise test if the edge function is positive
            overlaps &= (w0 == 0 ? ((edge0.y == 0 && edge0.x > 0) || edge0.y > 0) : (w0 > 0));
            overlaps &= (w1 == 0 ? ((edge1.y == 0 && edge1.x > 0) || edge1.y > 0) : (w1 > 0));
            overlaps &= (w2 == 0 ? ((edge2.y == 0 && edge2.x > 0) || edge2.y > 0) : (w2 > 0));

            // If all the edge functions are positive, the point is inside the triangle
            if (overlaps) {
                float weightA = w0 * ABCr;
                float weightB = w1 * ABCr;
                float weightC = w2 * ABCr;
                // Interpolate the colours at point P
                float u = v0u * weightA + v1u * weightB + v2u * weightC;
                float v = v0v * weightA + v1v * weightB + v2v * weightC;

                float r = v0r * weightA + v1r * weightB + v2r * weightC;
                float g = v0g * weightA + v1g * weightB + v2g * weightC;
                float b = v0b * weightA + v1b * weightB + v2b * weightC;

                u32 color = ts->sample(ts, (i32)u, (i32)v);
                float mb = (float)((color >> 10) & 0x1F) * b;
                float mg = (float)((color >> 5) & 0x1F) * g;
                float mr = (float)(color & 0x1F) * r;
                if (mr > 31.0f) mr = 31.0f;
                if (mg > 31.0f) mg = 31.0f;
                if (mb > 31.0f) mb = 31.0f;

                // Draw the pixe
                setpix_split(this, p.y, p.x, (u32)mr, (u32)mg, (u32)mb, 1, color & 0x8000);
            }
        }
    }
}

void RT_draw_flat_tex_triangle(struct PS1_GPU *this, struct RT_POINT2D *v0, struct RT_POINT2D *v1, struct RT_POINT2D *v2, struct PS1_GPU_TEXTURE_SAMPLER *ts)
{
    struct RT_POINT2D *sa;

    // Calculate the edge function for the whole triangle (ABC)
    float ABC = edge_function(v0, v1, v2);

    if (ABC < 0) {
        sa = v2;
        v2 = v1;
        v1 = sa;
        ABC = edge_function(v0, v1, v2);
    }

    float ABCr = 1.0f / ABC;

    float v0u = (float)v0->u;
    float v0v = (float)v0->v;

    float v1u = (float)v1->u;
    float v1v = (float)v1->v;

    float v2u = (float)v2->u;
    float v2v = (float)v2->v;

    // Initialise our point
    struct RT_POINT2D p;
    p.x = p.y = 0;

    // Get the bounding box of the triangle
    i32 minX = MIN3(v0->x, v1->x, v2->x);
    i32 minY = MIN3(v0->y, v1->y, v2->y);
    i32 maxX = MAX3(v0->x, v1->x, v2->x);
    i32 maxY = MAX3(v0->y, v1->y, v2->y);

    // Get edges for top-left testing...
    struct RT_POINT2Df edge0, edge1, edge2;
    edge0.x = (float) (v2->x - v1->x);
    edge0.y = (float) (v2->y - v1->y);

    edge1.x = (float) (v0->x - v2->x);
    edge1.y = (float) (v0->y - v2->y);

    edge2.x = (float) (v1->x - v0->x);
    edge2.y = (float) (v1->y - v0->y);

    // Loop through all the pixels of the bounding box
    for (p.y = minY; p.y < maxY; p.y++) {
        for (p.x = minX; p.x < maxX; p.x++) {
            // Calculate our edge functions
            float w0 = edge_function(v1, v2, &p); // w0 BCP
            float w1 = edge_function(v2, v0, &p); // w1 CAP
            float w2 = edge_function(v0, v1, &p); // w2 ABP

            // Normalise the edge functions by dividing by the total area to get the barycentric coordinates

            u32 overlaps = w1 >= 0 && w0 >= 0 && w2 >= 0;

            // If the point is on the edge, test if it is a top or left edge,
            // otherwise test if the edge function is positive
            overlaps &= (w0 == 0 ? ((edge0.y == 0 && edge0.x > 0) || edge0.y > 0) : (w0 > 0));
            overlaps &= (w1 == 0 ? ((edge1.y == 0 && edge1.x > 0) || edge1.y > 0) : (w1 > 0));
            overlaps &= (w2 == 0 ? ((edge2.y == 0 && edge2.x > 0) || edge2.y > 0) : (w2 > 0));

            // If all the edge functions are positive, the point is inside the triangle
            if (overlaps) {
                float weightA = w0 * ABCr;
                float weightB = w1 * ABCr;
                float weightC = w2 * ABCr;
                // Interpolate the colours at point P
                float u = v0u * weightA + v1u * weightB + v2u * weightC;
                float v = v0v * weightA + v1v * weightB + v2v * weightC;

                u16 color = ts->sample(ts, (i32)u, (i32)v);

                // Draw the pixel
                setpix(this, p.y, p.x, color & 0x7FFF, 1, color & 0x8000);
            }
        }
    }
}


void RT_draw_flat_tex_triangle_semi(struct PS1_GPU *this, struct RT_POINT2D *v0, struct RT_POINT2D *v1, struct RT_POINT2D *v2, struct PS1_GPU_TEXTURE_SAMPLER *ts)
{
    struct RT_POINT2D *sa;

    // Calculate the edge function for the whole triangle (ABC)
    float ABC = edge_function(v0, v1, v2);

    if (ABC < 0) {
        sa = v2;
        v2 = v1;
        v1 = sa;
        ABC = edge_function(v0, v1, v2);
    }
    float ABCr = 1.0f / ABC;

    float v0u = (float)v0->u;
    float v0v = (float)v0->v;

    float v1u = (float)v1->u;
    float v1v = (float)v1->v;

    float v2u = (float)v2->u;
    float v2v = (float)v2->v;

    // Initialise our point
    struct RT_POINT2D p;
    p.x = p.y = 0;

    // Get the bounding box of the triangle
    i32 minX = MIN3(v0->x, v1->x, v2->x);
    i32 minY = MIN3(v0->y, v1->y, v2->y);
    i32 maxX = MAX3(v0->x, v1->x, v2->x);
    i32 maxY = MAX3(v0->y, v1->y, v2->y);

    // Get edges for top-left testing...
    struct RT_POINT2Df edge0, edge1, edge2;
    edge0.x = (float) (v2->x - v1->x);
    edge0.y = (float) (v2->y - v1->y);

    edge1.x = (float) (v0->x - v2->x);
    edge1.y = (float) (v0->y - v2->y);

    edge2.x = (float) (v1->x - v0->x);
    edge2.y = (float) (v1->y - v0->y);

    // Loop through all the pixels of the bounding box
    for (p.y = minY; p.y < maxY; p.y++) {
        for (p.x = minX; p.x < maxX; p.x++) {
            // Calculate our edge functions
            float w0 = edge_function(v1, v2, &p); // w0 BCP
            float w1 = edge_function(v2, v0, &p); // w1 CAP
            float w2 = edge_function(v0, v1, &p); // w2 ABP

            // Normalise the edge functions by dividing by the total area to get the barycentric coordinates

            u32 overlaps = w1 >= 0 && w0 >= 0 && w2 >= 0;

            // If the point is on the edge, test if it is a top or left edge,
            // otherwise test if the edge function is positive
            overlaps &= (w0 == 0 ? ((edge0.y == 0 && edge0.x > 0) || edge0.y > 0) : (w0 > 0));
            overlaps &= (w1 == 0 ? ((edge1.y == 0 && edge1.x > 0) || edge1.y > 0) : (w1 > 0));
            overlaps &= (w2 == 0 ? ((edge2.y == 0 && edge2.x > 0) || edge2.y > 0) : (w2 > 0));

            // If all the edge functions are positive, the point is inside the triangle
            if (overlaps) {
                float weightA = w0 * ABCr;
                float weightB = w1 * ABCr;
                float weightC = w2 * ABCr;
                // Interpolate the colours at point P
                float u = v0u * weightA + v1u * weightB + v2u * weightC;
                float v = v0v * weightA + v1v * weightB + v2v * weightC;

                u16 color = ts->sample(ts, (i32)u, (i32)v);

                // Draw the pixel
                semipixm(this, p.y, p.x, color & 0x7FFF, ts->semi_mode, 1, color & 0x8000);
            }
        }
    }
}

void RT_draw_flat_triangle_semi(struct PS1_GPU *this, struct RT_POINT2D *v0, struct RT_POINT2D *v1, struct RT_POINT2D *v2, u32 r, u32 g, u32 b) {
    struct RT_POINT2D *sa;

    // Calculate the edge function for the whole triangle (ABC)
    float ABC = edge_function(v0, v1, v2);

    if (ABC < 0) {
        sa = v2;
        v2 = v1;
        v1 = sa;
    }

    // Initialise our point
    struct RT_POINT2D p;
    p.x = p.y = 0;

    // Get the bounding box of the triangle
    i32 minX = MIN3(v0->x, v1->x, v2->x);
    i32 minY = MIN3(v0->y, v1->y, v2->y);
    i32 maxX = MAX3(v0->x, v1->x, v2->x);
    i32 maxY = MAX3(v0->y, v1->y, v2->y);

    // Get edges for top-left testing...
    struct RT_POINT2Df edge0, edge1, edge2;
    edge0.x = (float) (v2->x - v1->x);
    edge0.y = (float) (v2->y - v1->y);

    edge1.x = (float) (v0->x - v2->x);
    edge1.y = (float) (v0->y - v2->y);

    edge2.x = (float) (v1->x - v0->x);
    edge2.y = (float) (v1->y - v0->y);

    // Loop through all the pixels of the bounding box
    for (p.y = minY; p.y < maxY; p.y++) {
        for (p.x = minX; p.x < maxX; p.x++) {
            // Calculate our edge functions
            float w0 = edge_function(v1, v2, &p); // w0
            float w1 = edge_function(v2, v0, &p); // w2
            float w2 = edge_function(v0, v1, &p); // w1

            u32 overlaps = w1 >= 0 && w0 >= 0 && w2 >= 0;

            // If the point is on the edge, test if it is a top or left edge,
            // otherwise test if the edge function is positive
            overlaps &= (w0 == 0 ? ((edge0.y == 0 && edge0.x > 0) || edge0.y > 0) : (w0 > 0));
            overlaps &= (w1 == 0 ? ((edge1.y == 0 && edge1.x > 0) || edge1.y > 0) : (w1 > 0));
            overlaps &= (w2 == 0 ? ((edge2.y == 0 && edge2.x > 0) || edge2.y > 0) : (w2 > 0));

            // If all the edge functions are positive, the point is inside the triangle
            if (overlaps) {
                // Draw the pixel
                semipix_split(this, p.y, p.x, r >> 3, g >> 3, b >> 3, 0, 1);
            }
        }
    }
}


void RT_draw_shaded_triangle(struct PS1_GPU *this, struct RT_POINT2D *v0, struct RT_POINT2D *v1, struct RT_POINT2D *v2) {
    struct RT_POINT2D *sa;

    // Calculate the edge function for the whole triangle (ABC)
    float ABC = edge_function(v0, v1, v2);

    if (ABC < 0) {
        sa = v2;
        v2 = v1;
        v1 = sa;
        ABC = edge_function(v0, v1, v2);
    }
    float ABCr = 1.0f / ABC;

    float v0r = (float)v0->r;
    float v0g = (float)v0->g;
    float v0b = (float)v0->b;

    float v1r = (float)v1->r;
    float v1g = (float)v1->g;
    float v1b = (float)v1->b;

    float v2r = (float)v2->r;
    float v2g = (float)v2->g;
    float v2b = (float)v2->b;

    // Initialise our point
    struct RT_POINT2D p;
    p.x = p.y = 0;

    // Get the bounding box of the triangle
    i32 minX = MIN3(v0->x, v1->x, v2->x);
    i32 minY = MIN3(v0->y, v1->y, v2->y);
    i32 maxX = MAX3(v0->x, v1->x, v2->x);
    i32 maxY = MAX3(v0->y, v1->y, v2->y);

    // Get edges for top-left testing...
    struct RT_POINT2Df edge0, edge1, edge2;
    edge0.x = (float) (v2->x - v1->x);
    edge0.y = (float) (v2->y - v1->y);

    edge1.x = (float) (v0->x - v2->x);
    edge1.y = (float) (v0->y - v2->y);

    edge2.x = (float) (v1->x - v0->x);
    edge2.y = (float) (v1->y - v0->y);

    // Loop through all the pixels of the bounding box
    for (p.y = minY; p.y < maxY; p.y++) {
        for (p.x = minX; p.x < maxX; p.x++) {
            // Calculate our edge functions
            float w0 = edge_function(v1, v2, &p); // w0 BCP
            float w1 = edge_function(v2, v0, &p); // w1 CAP
            float w2 = edge_function(v0, v1, &p); // w2 ABP

            // Normalise the edge functions by dividing by the total area to get the barycentric coordinates

            u32 overlaps = w1 >= 0 && w0 >= 0 && w2 >= 0;

            // If the point is on the edge, test if it is a top or left edge,
            // otherwise test if the edge function is positive
            overlaps &= (w0 == 0 ? ((edge0.y == 0 && edge0.x > 0) || edge0.y > 0) : (w0 > 0));
            overlaps &= (w1 == 0 ? ((edge1.y == 0 && edge1.x > 0) || edge1.y > 0) : (w1 > 0));
            overlaps &= (w2 == 0 ? ((edge2.y == 0 && edge2.x > 0) || edge2.y > 0) : (w2 > 0));

            // If all the edge functions are positive, the point is inside the triangle
            if (overlaps) {
                float weightA = w0 * ABCr;
                float weightB = w1 * ABCr;
                float weightC = w2 * ABCr;
                // Interpolate the colours at point P
                float r = v0r * weightA + v1r * weightB + v2r * weightC;
                float g = v0g * weightA + v1g * weightB + v2g * weightC;
                float b = v0b * weightA + v1b * weightB + v2b * weightC;

                // Draw the pixel
                setpix_split(this, p.y, p.x, (u32)r >> 3, (u32)g >> 3, (u32)b >> 3, 0, 0);
            }
        }
    }
}

void RT_draw_shaded_triangle_semi(struct PS1_GPU *this, struct RT_POINT2D *v0, struct RT_POINT2D *v1, struct RT_POINT2D *v2) {
    struct RT_POINT2D *sa;

    // Calculate the edge function for the whole triangle (ABC)
    float ABC = edge_function(v0, v1, v2);

    if (ABC < 0) {
        sa = v2;
        v2 = v1;
        v1 = sa;
        ABC = edge_function(v0, v1, v2);
    }
    float ABCr = 1.0f / ABC;

    float v0r = (float)v0->r;
    float v0g = (float)v0->g;
    float v0b = (float)v0->b;

    float v1r = (float)v1->r;
    float v1g = (float)v1->g;
    float v1b = (float)v1->b;

    float v2r = (float)v2->r;
    float v2g = (float)v2->g;
    float v2b = (float)v2->b;

    // Initialise our point
    struct RT_POINT2D p;
    p.x = p.y = 0;

    // Get the bounding box of the triangle
    i32 minX = MIN3(v0->x, v1->x, v2->x);
    i32 minY = MIN3(v0->y, v1->y, v2->y);
    i32 maxX = MAX3(v0->x, v1->x, v2->x);
    i32 maxY = MAX3(v0->y, v1->y, v2->y);

    // Get edges for top-left testing...
    struct RT_POINT2Df edge0, edge1, edge2;
    edge0.x = (float) (v2->x - v1->x);
    edge0.y = (float) (v2->y - v1->y);

    edge1.x = (float) (v0->x - v2->x);
    edge1.y = (float) (v0->y - v2->y);

    edge2.x = (float) (v1->x - v0->x);
    edge2.y = (float) (v1->y - v0->y);

    // Loop through all the pixels of the bounding box
    for (p.y = minY; p.y < maxY; p.y++) {
        for (p.x = minX; p.x < maxX; p.x++) {
            // Calculate our edge functions
            float w0 = edge_function(v1, v2, &p); // w0 BCP
            float w1 = edge_function(v2, v0, &p); // w1 CAP
            float w2 = edge_function(v0, v1, &p); // w2 ABP

            // Normalise the edge functions by dividing by the total area to get the barycentric coordinates

            u32 overlaps = w1 >= 0 && w0 >= 0 && w2 >= 0;

            // If the point is on the edge, test if it is a top or left edge,
            // otherwise test if the edge function is positive
            overlaps &= (w0 == 0 ? ((edge0.y == 0 && edge0.x > 0) || edge0.y > 0) : (w0 > 0));
            overlaps &= (w1 == 0 ? ((edge1.y == 0 && edge1.x > 0) || edge1.y > 0) : (w1 > 0));
            overlaps &= (w2 == 0 ? ((edge2.y == 0 && edge2.x > 0) || edge2.y > 0) : (w2 > 0));

            // If all the edge functions are positive, the point is inside the triangle
            if (overlaps) {
                float weightA = w0 * ABCr;
                float weightB = w1 * ABCr;
                float weightC = w2 * ABCr;
                // Interpolate the colours at point P
                float r = v0r * weightA + v1r * weightB + v2r * weightC;
                float g = v0g * weightA + v1g * weightB + v2g * weightC;
                float b = v0b * weightA + v1b * weightB + v2b * weightC;

                // Draw the pixel
                semipix_split(this, p.y, p.x, (u32)r >> 3, (u32)g >> 3, (u32)b >> 3, 0, 1);
            }
        }
    }
}
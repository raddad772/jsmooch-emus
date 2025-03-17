//
// Created by . on 3/12/25.
//

#include <stdlib.h>
#include <math.h>
#include "nds_re.h"
#include "nds_bus.h"

static void clear_line(struct NDS *this, struct NDS_RE_LINEBUFFER *l)
{
    for (u32 x = 0; x < 256; x++) {
        l->poly_id[x] = this->re.io.clear.poly_id;
        l->rgb_top[x] = this->re.io.clear.color;
        l->rgb_bottom[x] = this->re.io.clear.color;
        l->alpha[x] = this->re.io.clear.alpha;
        l->depth[x] = INT32_MAX; //this->re.io.clear.depth;
        //l->depth[x] = this->re.io.clear.depth;
    }
}

#define CCW 0
#define CW 1

struct NDS_RE_EDGE {
    struct NDS_RE_VERTEX *v[2];
    i32 num[2];
    i32 dir;
    u32 found;
};

#define DBGL 165

void find_closest_points_marked(struct NDS *this, struct NDS_GE_BUFFERS *b, struct NDS_RE_POLY *p, u32 comp_y, struct NDS_RE_EDGE *edges)
{
    struct NDS_RE_VERTEX *first_vertex = &b->vertex[p->first_vertex_ptr + p->highest_vertex];
    /*printf("\nHIGHEST VERTEX %d: %d, %d", p->highest_vertex, first_vertex->xx, first_vertex->yy);
    printf("\nAll vertices of polygon:");
    for (u32 i = 0; i < p->num_vertices; i++) {
        struct NDS_RE_VERTEX *yv = &b->vertex[p->first_vertex_ptr+i];
        printf("\n%d: %d %d", i, yv->xx, yv->yy);
    }
    printf("\n...");*/

    edges[0].num[1] = p->highest_vertex;
    edges[0].v[1] = first_vertex;
    edges[0].dir = p->num_vertices - 1;

    edges[1].num[1] = p->highest_vertex;
    edges[1].v[1] = first_vertex;
    edges[1].dir = 1;
    edges[0].found = edges[1].found = 0;
    u32 total_found = 0;

    for (u32 i = 0; i < (p->num_vertices + 1); i++) {
        for (i32 j = 0; j < 2; j++) {
            if (edges[j].found) continue;
            edges[j].num[0] = edges[j].num[1];
            edges[j].v[0] = edges[j].v[1];

            edges[j].num[1] = (edges[j].num[1] + edges[j].dir) % (i32)p->num_vertices;
            edges[j].v[1] = &b->vertex[edges[j].num[1] + p->first_vertex_ptr];

            //printf("\nEdge check %d: %d,%d to %d,%d", j, edges[j].v[0]->xx, edges[j].v[0]->yy, edges[j].v[1]->xx, edges[j].v[1]->yy);
            // TODO: verify vs. winding order if slope is positive or negative properly?
            // original though twas just to verify direction but that doesn't work...

            // Check if edge intersects scanline
            i32 min_y = edges[j].v[0]->yy;
            i32 max_y = edges[j].v[1]->yy;
            if (max_y < min_y) {
                i32 t = min_y;
                min_y = max_y;
                max_y = t;
            }

            if ((min_y > comp_y) || (max_y < comp_y)) {
                //printf("\nEdge %d: rejected because %d >= %d > %d not true!", j, min_y, comp_y, max_y);
                continue;
            }

            if (min_y == max_y) {
                //printf("\nEdge %d: rejected because horizontal", j);
                continue; // TODO: breaks horizontal lines!?
            }
            //if (j == 0)
                //printf("\nFound edge %d: %d,%d to %d,%d", j, edges[j].v[0]->xx, edges[j].v[0]->yy, edges[j].v[1]->xx, edges[j].v[1]->yy);
            total_found++;
            edges[j].found = 1;
        }
        if (total_found == 2) break;
    }
    if (total_found < 2) printf("\nFAILED to find one of the edges!!");
    for (u32 j = 0; j < 2; j++) {
        if (edges[j].v[0]->yy > edges[j].v[1]->yy) {
            struct NDS_RE_VERTEX *t = edges[j].v[0];
            edges[j].v[0] = edges[j].v[1];
            edges[j].v[1] = t;
        }
    }

    //for (u32 j = 0; j < 2; j++)
    //    printf("\nEdge %d vtx 0 %d %d RGB %04x", j, edges[j].v[0]->xx, edges[j].v[0]->yy, edges[j].v[0]->color);
}

#define EXTRACTB(x) (((x) >> 12) & 0x3F)
#define EXTRACTG(x) (((x) >> 6) & 0x3F)
#define EXTRACTR(x) ((x) & 0x3F)

static void lerp_edge_to_vtx(struct NDS_RE_EDGE *e, struct NDS_RE_VERTEX *v, i32 y)
{
    float y_diff = e->v[1]->yy - e->v[0]->yy;
    float x_diff = e->v[1]->xx - e->v[0]->xx;
    float w_diff = e->v[1]->ww - e->v[0]->ww;
    float z_diff = e->v[1]->zz - e->v[0]->zz;

    //printf("\nEXTRACT V0 %05x", e->v[0]->color);
    float rstart = EXTRACTR(e->v[0]->color);
    float gstart = EXTRACTG(e->v[0]->color);
    float bstart = EXTRACTB(e->v[0]->color);
    float r_diff = EXTRACTR(e->v[1]->color) - rstart;
    float g_diff = EXTRACTG(e->v[1]->color) - gstart;
    float b_diff = EXTRACTB(e->v[1]->color) - bstart;

    float yrec = 1.0f / y_diff;
    float yminor = y - e->v[0]->yy;
    yrec *= yminor;
    float x_loc = (x_diff * yrec) + (float)e->v[0]->xx;
    float r_loc = (r_diff * yrec) + rstart;
    float g_loc = (g_diff * yrec) + gstart;
    float b_loc = (b_diff * yrec) + bstart;
    float w_loc = (w_diff * yrec) + e->v[0]->ww;
    float z_loc = (w_diff * yrec) + e->v[0]->ww;

    v->yy = y;
    v->xx = (u32)x_loc;
    v->lr = (u32)r_loc;
    if (v->lr > 63) v->lr = 63;
    if (v->lr < 0) v->lr = 0;
    v->lg = (u32)g_loc;
    if (v->lg > 63) v->lg = 63;
    if (v->lg < 0) v->lg = 0;
    v->lb = (u32)b_loc;
    if (v->lb > 63) v->lb = 63;
    if (v->lb < 0) v->lb = 0;
    v->ww = (i32)w_loc;
    v->zz = (i32)z_loc;
    //printf("\nLRGB: %d, %d, %d", v->lr, v->lg, v->lb);
}

static float vtx_to_float(i32 v)
{
    return ((float)v) / 4096.0f;
}


void render_line(struct NDS *this, struct NDS_GE_BUFFERS *b, i32 line_num)
{
    struct NDS_RE_LINEBUFFER *line = &this->re.out.linebuffer[line_num];
    //printf("\n\nLine num %d", line_num);
    clear_line(this, line);
    u32 test_byte = line_num >> 3;
    u32 test_bit = 1 << (line_num & 7);

    u32 global_tex_enable = this->re.io.DISP3DCNT.texture_mapping;

    struct NDS_RE_EDGE edges[2];
    for (u32 poly_num = 0; poly_num < b->polygon_index; poly_num++) {
        struct NDS_RE_POLY *p = &b->polygon[poly_num];

        if (p->attr.alpha == 0) {
            printf("\nskip poly %d as hidden alpha", poly_num);
            continue;
        }
        //if (poly_num > 0) break;
        if (!p->attr.render_back && !p->front_facing) {
            printf("\nPOLY %d SKIPPED FOR BACK RENDER NOT ENABLE", poly_num);
            continue;
        }
        if (!p->attr.render_front && p->front_facing) {
            printf("\nPOLY %d SKIPPED FOR FRONT RENDER NOT ENABLE", poly_num);
            continue;
        }

        // Polygon does not intersect this line
        if (!(p->lines_on_bitfield[test_byte] & test_bit)) {
            //printf("\nSkip poly %d", poly_num);
            continue;
        }

        // Find the correct points to lerp
        find_closest_points_marked(this, b, p, line_num, edges);

        struct NDS_RE_VERTEX lerped[2];

        lerp_edge_to_vtx(&edges[0], &lerped[0], line_num);
        lerp_edge_to_vtx(&edges[1], &lerped[1], line_num);
        u32 xorby = lerped[0].xx > lerped[1].xx ? 1 : 0;
        struct NDS_RE_VERTEX *left = &lerped[0 ^ xorby];
        struct NDS_RE_VERTEX *right = &lerped[1 ^ xorby];

        /*printf("\nTop left   : %d %d    Top right   : %d %d", edges[0 ^ xorby].v[0]->xx, edges[0 ^ xorby].v[0]->yy, edges[1 ^ xorby].v[0]->xx, edges[1 ^ xorby].v[0]->yy);
        printf("\nTLRGB: %04x           TRRGB: %04x", edges[0 ^ xorby].v[0]->color, edges[0 ^ xorby].v[1]->color);
        printf("\nBottom left: %d %d    Bottom right: %d %d ", edges[0 ^ xorby].v[1]->xx, edges[0 ^ xorby].v[1]->yy, edges[1 ^ xorby].v[1]->xx, edges[1 ^ xorby].v[1]->yy);
        printf("\nleft %d, right %d %d %d", left->xx, right->xx, left->lb, right->lb);*/

        float x_steps = 1.0f / (float)(right->xx - left->xx);
        i32 r_steps = right->lr - left->lr;
        i32 g_steps = right->lg - left->lg;
        i32 b_steps = right->lb - left->lb;
        float r_step = (float)r_steps * x_steps;
        float g_step = (float)g_steps * x_steps;
        float b_step = (float)b_steps * x_steps;
        float cr = left->lr;
        float cg = left->lg;
        float cb = left->lb;
        float depth_step, depth;
        float s_step, s;
        float t_step, t;
        i32 depth_r, depth_l;
        if (b->depth_buffering_w) {
            if (p->w_normalization_right) {
                depth_r = right->ww >> p->w_normalization_right;
                depth_l = left->ww >> p->w_normalization_right;
            }
            else {
                depth_r = right->ww << p->w_normalization_left;
                depth_l = left->ww << p->w_normalization_left;
            }
        }
        else {
            depth_r = right->zz;
            depth_l = left->zz;
            //  (((Z * 0x4000) / W) + 0x3FFF) * 0x200
            //depth_r = (((right->zz * 0x4000) / right->ww) + 0x3FFF) * 0x200;
            //depth_l = (((left->zz * 0x4000) / right->ww) + 0x3FFF) * 0x200;

            //depth_r = ((((right->zz >> 8) * 0x4000) / right->ww) + 0x3FFF) * 0x200;
            //depth_l = ((((left->zz >> 8) * 0x4000) / right->ww) + 0x3FFF) * 0x200;
        }

        i32 depth_steps = depth_r - depth_l;
        depth_step = (float)depth_steps * x_steps;
        depth = depth_l;

        if (global_tex_enable) {
            s_step = (float)(right->uv[0] - left->uv[0]) * x_steps;
            s = left->uv[0];
            t_step = (float)(right->uv[1] - left->uv[1]) * x_steps;
            t = left->uv[1];
        }


        u32 rside = right->xx > 255 ? 255 : right->xx;

        for (u32 x = left->xx; x < rside; x++) {
            if (x < 256) {
                // Test Z and early-out
                u32 comparison;
                //printf("\n!%08x %f", (i32)depth, vtx_to_float((i32)depth));
                if (p->attr.depth_test_mode == 0) comparison = (i32)depth < line->depth[x];
                else comparison = (u32)depth == line->depth[x];
                if (comparison) {
                    u32 pix_r = ((u32)cr) >> 1;
                    u32 pix_g = ((u32)cg) >> 1;
                    u32 pix_b = ((u32)cb) >> 1;

                    if (global_tex_enable) {
                        // Sample texture-map
                    }
                    line->rgb_top[x] = pix_r | (pix_g << 5) | (pix_b << 10);
                    line->depth[x] = (u32)depth;
                }
                depth += depth_step;
                cr += r_step;
                cg += g_step;
                cb += b_step;
            }
        }
    }
}

void NDS_RE_render_frame(struct NDS *this)
{
    struct NDS_GE_BUFFERS *b = &this->ge.buffers[this->ge.ge_has_buffer ^ 1];
    for (u32 i = 0; i < 192; i++)
        render_line(this, b, i);
}

void NDS_RE_init(struct NDS *this)
{

}

void NDS_RE_reset(struct NDS *this)
{

}

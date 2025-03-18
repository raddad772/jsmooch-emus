//
// Created by . on 3/12/25.
//

#include <stdlib.h>
#include <math.h>
#include "nds_re.h"
#include "nds_bus.h"
#include "nds_vram.h"

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
    struct NDS_RE_VERTEX *first_vertex = &b->vertex[p->vertex_pointers[p->highest_vertex]];
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
            edges[j].v[1] = &b->vertex[p->vertex_pointers[edges[j].num[1]]];

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
            if (min_y > max_y) {
                struct NDS_RE_VERTEX *t = edges[j].v[0];
                edges[j].v[0] = edges[j].v[1];
                edges[j].v[1] = t;

            }
            edges[j].found = 1;
        }
        if (total_found == 2) break;
    }
    if (total_found < 2) printf("\nFAILED to find one of the edges!!");

    //for (u32 j = 0; j < 2; j++)
    //    printf("\nEdge %d vtx 0 %d %d RGB %04x", j, edges[j].v[0]->xx, edges[j].v[0]->yy, edges[j].v[0]->color);
}

#define EXTRACTB(x) (((x) >> 12) & 0x3F)
#define EXTRACTG(x) (((x) >> 6) & 0x3F)
#define EXTRACTR(x) ((x) & 0x3F)

static void lerp_edge_to_vtx(struct NDS_RE_EDGE *e, struct NDS_RE_VERTEX *v, i32 y, u32 do_tex)
{
    float y_diff = e->v[1]->yy - e->v[0]->yy;
    float x_diff = e->v[1]->xx - e->v[0]->xx;
    float w_diff = e->v[1]->ww - e->v[0]->ww;
    float z_diff = e->v[1]->zz - e->v[0]->zz;
    float s_diff, t_diff, s_start, t_start;
    float s_loc=0, t_loc=0;


    float y_recip = 1.0f / y_diff;
    float yminor = y - e->v[0]->yy;
    y_recip *= yminor;
    if (do_tex) {
        s_diff = e->v[1]->uv[0] - e->v[0]->uv[0];
        t_diff = e->v[1]->uv[1] - e->v[0]->uv[1];
        s_start = e->v[0]->uv[0];
        t_start = e->v[0]->uv[1];
        s_loc = (s_diff * y_recip) + s_start;
        t_loc = (t_diff * y_recip) + t_start;
        v->uv[0]=(i32)s_loc;
        v->uv[1]=(i32)t_loc;
    }


    //printf("\nEXTRACT V0 %05x", e->v[0]->color);
    float rstart = EXTRACTR(e->v[0]->color);
    float gstart = EXTRACTG(e->v[0]->color);
    float bstart = EXTRACTB(e->v[0]->color);
    float r_diff = EXTRACTR(e->v[1]->color) - rstart;
    float g_diff = EXTRACTG(e->v[1]->color) - gstart;
    float b_diff = EXTRACTB(e->v[1]->color) - bstart;

    float x_loc = (x_diff * y_recip) + (float)e->v[0]->xx;
    float r_loc = (r_diff * y_recip) + rstart;
    float g_loc = (g_diff * y_recip) + gstart;
    float b_loc = (b_diff * y_recip) + bstart;
    float w_loc = (w_diff * y_recip) + e->v[0]->ww;
    float z_loc = (w_diff * y_recip) + e->v[0]->ww;

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

static float uv_to_float(i32 v)
{
    return ((float)v) / 16.0f;
}

static float vtx_to_float(i32 v)
{
    return ((float)v) / 4096.0f;
}

static void sample_texture_palette_4bpp(struct NDS *this, struct NDS_RE_TEX_SAMPLER *ts, struct NDS_RE_POLY *p, u32 s, u32 t, u32 *r, u32 *g, u32 *b, u32 *a)
{
    u32 addr = ((t * ts->t_size) + s);
    u32 c = NDS_VRAM_tex_read(this, addr >> 1, 1) & 0xFF;
    if (addr & 1) c >>= 4;
    c &= 0x0F;
    c = NDS_VRAM_pal_read(this, ts->pltt_base + c, 2);

    *r = (c & 0x1F) << 1;
    *g = ((c >> 5) & 0x1F) << 1;
    *b = ((c >> 10) & 0x1F) << 1;
    *a = ((c >> 15) & 1) * 63;
}

static void sample_texture_palette_2bpp(struct NDS *this, struct NDS_RE_TEX_SAMPLER *ts, struct NDS_RE_POLY *p, u32 s, u32 t, u32 *r, u32 *g, u32 *b, u32 *a)
{
    u32 addr = ((t * ts->t_size) + s);
    u32 c = NDS_VRAM_tex_read(this, addr >> 2, 1) & 0xFF;
    for (u32 i = 0; i < (addr & 3); i++) {
        c >>= 2;
    }
    c &= 3;
    c = NDS_VRAM_pal_read(this, ts->pltt_base + c, 2);

    *r = (c & 0x1F) << 1;
    *g = ((c >> 5) & 0x1F) << 1;
    *b = ((c >> 10) & 0x1F) << 1;
    *a = ((c >> 15) & 1) * 63;
}


static void sample_texture_direct(struct NDS *this, struct NDS_RE_TEX_SAMPLER *ts, struct NDS_RE_POLY *p, u32 s, u32 t, u32 *r, u32 *g, u32 *b, u32 *a)
{
    // 16-bit read from VRAM @ ptr ((t * size) + s) * 2
    u32 c = NDS_VRAM_tex_read(this, (((t * ts->t_size) + s) << 1), 2) & 0x7FFF;
    *r = (c & 0x1F) << 1;
    *g = ((c >> 5) & 0x1F) << 1;
    *b = ((c >> 10) & 0x1F) << 1;
    *a = ((c >> 15) & 1) * 63;
}

static void final_tex_coord(struct NDS_RE_TEX_SAMPLER *ts, i32 *s, i32 *t)
{
    //printf("\nINPUT S,T: %f, %f", uv_to_float(*s), uv_to_float(*t));
    i32 ms = (i32)*s;
    i32 mt = (i32)*t;
    // Round to nearest!
    ms += 8;
    mt += 8;
    if (ts->s_repeat)
        ms &= ts->s_fp_mask;
    else {
        if (ms < 0) ms = 0;
        if (ms > ts->s_fp_mask) ms = ts->s_fp_mask;
    }

    if (ts->t_repeat)
        mt &= ts->t_fp_mask;
    else {
        if (mt < 0) mt = 0;
        if (mt > ts->t_fp_mask) mt = ts->t_fp_mask;
    }

    //printf("\nS SIZE:%d T SIZE:%d", ts->s_size_fp, ts->t_size_fp);
    // clamped = repeat 000?
    // TODO: add clamping, mirroring, flipping, etc.

    // We wont' do that for now
    *s = ms >> 4;
    *t = mt >> 4;

    //printf("\nFINAL S,T: %d, %d", *s, *t);

}

/*static void get_vram_ptr(struct NDS *this, struct tex_sampler *ts)
{
    u32 addr =
    ts->tex_ptr
}*/


static void fill_tex_sampler(struct NDS *this, struct NDS_RE_POLY *p)
{
    struct NDS_RE_TEX_SAMPLER *ts = &p->sampler;
    ts->s_size = 8 << p->tex_param.sz_s;
    ts->t_size = 8 << p->tex_param.sz_t;
    ts->pltt_base = p->tex_param.format == 2 ? p->pltt_base << 3 : p->pltt_base << 4;
    ts->s_size_fp = (ts->s_size << 4);
    ts->t_size_fp = (ts->t_size << 4);
    ts->s_fp_mask = ts->s_size_fp - 1;
    ts->t_fp_mask = ts->t_size_fp - 1;
    ts->s_sub_shift = (3 + p->tex_param.sz_s) + 4; // 4 in these lines is for the 4-bit fixed point addition
    ts->t_sub_shift = (3 + p->tex_param.sz_t) + 4;
    ts->s_flip = p->tex_param.flip_s;
    ts->t_flip = p->tex_param.flip_t;
    ts->s_repeat = p->tex_param.repeat_s;
    ts->t_repeat = p->tex_param.repeat_t;
    ts->tex_addr = p->tex_param.vram_offset << 3;
    ts->filled_out = 1;
    switch(p->tex_param.format) {
        case 0: // none!
            ts->sample = NULL;
            ts->tex_ptr = NULL;
            return;
        case 2:
            ts->sample = &sample_texture_palette_2bpp;
        case 3: // 4-bit palette!
            ts->sample = &sample_texture_palette_4bpp;
            break;
        case 7: // direct!
            ts->sample = &sample_texture_direct;
            //get_vram_info(this, ts);
            break;
        default:
            printf("\nUNIMPL. TEX. FMT. %d", p->tex_param.format);
            ts->sample = NULL;
            ts->tex_ptr = NULL;
            break;
    }
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
    struct tex_sampler;
    for (u32 poly_num = 0; poly_num < b->polygon_index; poly_num++) {
        struct NDS_RE_POLY *p = &b->polygon[poly_num];
        u32 tex_enable = global_tex_enable && (p->tex_param.format != 0);
        if (tex_enable && !p->sampler.filled_out) fill_tex_sampler(this, p);
        /*if (p->attr.alpha == 0) {
            printf("\nskip poly %d as hidden alpha", poly_num);
            continue;
        }*/
        //if (poly_num > 0) break;
        // Polygon does not intersect this line
        if ((line_num < p->min_y) || (line_num > p->max_y)) {
            //printf("\nSkip poly %d min:%d max:%d", poly_num, p->min_y, p->max_y);
            continue;
        }

        // Find the correct points to lerp
        find_closest_points_marked(this, b, p, line_num, edges);

        struct NDS_RE_VERTEX lerped[2];

        lerp_edge_to_vtx(&edges[0], &lerped[0], line_num, tex_enable);
        lerp_edge_to_vtx(&edges[1], &lerped[1], line_num, tex_enable);
        u32 xorby = lerped[0].xx > lerped[1].xx ? 1 : 0;
        struct NDS_RE_VERTEX *left = &lerped[0 ^ xorby];
        struct NDS_RE_VERTEX *right = &lerped[1 ^ xorby];

        /*printf("\nTop left   : %d %d    Top right   : %d %d", edges[0 ^ xorby].v[0]->xx, edges[0 ^ xorby].v[0]->yy, edges[1 ^ xorby].v[0]->xx, edges[1 ^ xorby].v[0]->yy);
        printf("\nTLRGB: %04x           TRRGB: %04x", edges[0 ^ xorby].v[0]->color, edges[0 ^ xorby].v[1]->color);
        printf("\nBottom left: %d %d    Bottom right: %d %d ", edges[0 ^ xorby].v[1]->xx, edges[0 ^ xorby].v[1]->yy, edges[1 ^ xorby].v[1]->xx, edges[1 ^ xorby].v[1]->yy);
        printf("\nleft %d, right %d %d %d", left->xx, right->xx, left->lb, right->lb);*/

        float x_recip = 1.0f / (float)(right->xx - left->xx);
        i32 r_steps = right->lr - left->lr;
        i32 g_steps = right->lg - left->lg;
        i32 b_steps = right->lb - left->lb;
        float r_step = (float)r_steps * x_recip;
        float g_step = (float)g_steps * x_recip;
        float b_step = (float)b_steps * x_recip;
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
        depth_step = (float)depth_steps * x_recip;
        depth = depth_l;

        if (tex_enable) {
            s_step = (float)(right->uv[0] - left->uv[0]) * x_recip;
            s = left->uv[0];
            t_step = (float)(right->uv[1] - left->uv[1]) * x_recip;
            t = left->uv[1];
            //printf("\nS,T: %f %f", uv_to_float(s), uv_to_float(t));
        }
        u32 rside = right->xx > 255 ? 255 : right->xx;

        // Factor = (x * W_left) / (((xmax - x) * W_right) + (x * W_left))
        // where x is x position in span, xmax is number of pixels
        // w0 and w1 are
        float xmax = right->xx - left->xx;
        float lfw = (float)left->ww / 4096.0f;
        float rfw = (float)right->ww / 4096.0f;
        for (u32 x = left->xx; x < rside; x++) {
            if (x < 256) {
                float fx = x;
                float fxl = fx * lfw;
                //float factor = fxl / (((xmax - fx) * rfw) + fxl);
                //printf("\nFACTOR: %f", factor);
                // Test Z and early-out
                u32 comparison;
                //printf("\n!%08x %f", (i32)depth, vtx_to_float((i32)depth));
                if (p->attr.depth_test_mode == 0) comparison = (i32)depth < line->depth[x];
                else comparison = (u32)depth == line->depth[x];
                if (comparison) {
                    u32 pix_r5, pix_g5, pix_b5, pix_a5;
                    if (tex_enable && p->sampler.sample) {
                        float tex_rf, tex_gf, tex_bf;
                        float tex_af;
                        i32 final_s = (i32)s;
                        i32 final_t = (i32)t;
                        final_tex_coord(&p->sampler, &final_s, &final_t);
                        u32 tex_r6, tex_g6, tex_b6, tex_a6;
                        p->sampler.sample(this, &p->sampler, p, final_s, final_t, &tex_r6, &tex_g6, &tex_b6, &tex_a6);

                        tex_rf = (float)tex_r6;
                        tex_gf = (float)tex_g6;
                        tex_bf = (float)tex_b6;
                        tex_af = (float)tex_a6;

                        // OK we have texture color now...how do we use it!?
                        switch(p->attr.mode) {
                            case 0: // modulation
                                pix_r5 = (u32)(((tex_rf + 1) * (cr + 1) - 1)) >> 7;
                                pix_g5 = (u32)(((tex_gf + 1) * (cg + 1) - 1)) >> 7;
                                pix_b5 = (u32)(((tex_bf + 1) * (cb + 1) - 1)) >> 7;
                                pix_a5 = (u32)(((tex_rf + 1) * (cr + 1) - 1)) >> 6;
                                break;
                            case 1: // decal
                                switch(tex_a6) {
                                    case 0:
                                        pix_r5 = ((u32)cr) >> 1;
                                        pix_g5 = ((u32)cg) >> 1;
                                        pix_b5 = ((u32)cb) >> 1;
                                        break;
                                    case 31:
                                        pix_r5 = ((u32)tex_r6) >> 1;
                                        pix_g5 = ((u32)tex_g6) >> 1;
                                        pix_b5 = ((u32)tex_b6) >> 1;
                                        break;
                                    default:
                                        // // R = (Rt*At + Rv*(63-At))/64  ;except, when At=0: R=Rv, when At=31: R=Rt
                                        pix_r5 = (u32)(tex_rf * tex_af + cr * (63 - tex_af)) >> 7;
                                        pix_g5 = (u32)(tex_gf * tex_af + cg * (63 - tex_af)) >> 7;
                                        pix_b5 = (u32)(tex_bf * tex_af + cb * (63 - tex_af)) >> 7;
                                        break;
                                }
                                pix_a5 = tex_a6 >> 1;
                                break;
                            case 2: // highlight/toon
                                // TODO: this
                                pix_r5 = 0x1F;
                                pix_g5 = 0;
                                pix_b5 = 0x1F;
                                break;
                            case 3: // shadow
                                // TODO: this
                                pix_r5 = 0x1F;
                                pix_g5 = 0;
                                pix_b5 = 0x1F;
                                break;
                        }
                    }
                    else {
                        pix_r5 = ((u32) cr) >> 1;
                        pix_g5 = ((u32) cg) >> 1;
                        pix_b5 = ((u32) cb) >> 1;
                        pix_a5 = p->attr.alpha;
                    }

                    line->rgb_top[x] = pix_r5 | (pix_g5 << 5) | (pix_b5 << 10);
                    line->depth[x] = (u32)depth;
                }
                depth += depth_step;
                cr += r_step;
                cg += g_step;
                cb += b_step;
                if (tex_enable) {
                    s += s_step;
                    t += t_step;
                }
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

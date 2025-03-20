//
// Created by . on 3/12/25.
//

#include <stdlib.h>
#include <math.h>
#include "nds_re.h"
#include "nds_bus.h"
#include "nds_vram.h"

/**
 * This interpolation code is adapted from MelonDS
 */

struct NDS_RE_interp {
    i32 x0, x1, xdiff, x;

    u32 shift;
    u32 linear;

    i32 xrecip_z;
    i32 w0n, w0d, w1d;

    u32 bit_precision;

    u32 yfactor;
};

static void NDS_RE_interp_setup(struct NDS_RE_interp *this, u32 bit_precision, i32 x0, i32 x1, i32 w0, i32 w1)
{
    this->x0 = x0;
    this->x1 = x1;
    this->xdiff = x1 - x0;
    this->bit_precision = bit_precision;

    if (this->xdiff != 0)
        this->xrecip_z = (1<<22) / this->xdiff;
    else
        this->xrecip_z = 0;

    u32 mask = bit_precision ? 0x7E : 0x7F;
    if ((w0 == w1) && !(w0 & mask) && !(w1 & mask))
        this->linear = 1;
    else
        this->linear = 0;

    if (bit_precision == 9) {
        if ((w0 & 0x1) && !(w1 & 0x1))
        {
            this->w0n = w0 - 1;
            this->w0d = w0 + 1;
            this->w1d = w1;
        }
        else
        {
            this->w0n = w0 & 0xFFFE;
            this->w0d = w0 & 0xFFFE;
            this->w1d = w1 & 0xFFFE;
        }

        this->shift = 9;
    }
    else {
        this->w0n = w0;
        this->w0d = w0;
        this->w1d = w1;

        this->shift = 8;
    }
}

static void clear_line(struct NDS *this, struct NDS_RE_LINEBUFFER *l) {
    for (u32 x = 0; x < 256; x++) {
        l->poly_id[x] = this->re.io.CLEAR.poly_id;
        l->rgb_top[x] = this->re.io.CLEAR.COLOR;
        l->rgb_bottom[x] = this->re.io.CLEAR.COLOR;
        l->alpha[x] = this->re.io.CLEAR.alpha;
        l->depth[x] = INT32_MAX; //this->re.io.clear.depth;
        //l->depth[x] = this->re.io.clear.depth;
    }
}

#define CCW 0
#define CW 1

struct NDS_RE_EDGE {
    struct NDS_RE_interp interp;
    struct NDS_GE_VTX_list_node *v[2];
    i32 num[2];
    i32 dir;
    u32 found;
};

#define DBGL 165

void find_closest_points_marked(struct NDS *this, struct NDS_GE_BUFFERS *b, struct NDS_RE_POLY *p, u32 comp_y, struct NDS_RE_EDGE *edges)
{
    struct NDS_GE_VTX_list_node *first_vertex = p->highest_vertex;
    /*printf("\nHIGHEST VERTEX %d: %d, %d", p->highest_vertex, first_vertex->data.xyzw[0], first_vertex->data.xyzw[1]);
    printf("\nAll vertices of polygon:");
    for (u32 i = 0; i < p->num_vertices; i++) {
        struct NDS_RE_VERTEX *yv = &b->vertex[p->first_vertex_ptr+i];
        printf("\n%d: %d %d", i, yv->data.xyzw[0], yv->data.xyzw[1]);
    }
    printf("\n...");*/

    edges[0].v[1] = first_vertex;
    edges[0].dir = -1;

    edges[1].v[1] = first_vertex;
    edges[1].dir = 1;
    edges[0].found = edges[1].found = 0;
    u32 total_found = 0;

    for (u32 i = 0; i < (p->vertex_list.len + 1); i++) {
        for (i32 j = 0; j < 2; j++) {
            if (edges[j].found) continue;
            edges[j].v[0] = edges[j].v[1];

            struct NDS_GE_VTX_list_node *next = NULL;
            if (edges[j].dir == -1) {
                next = edges[j].v[0]->prev;
                if (!next) next = p->vertex_list.last;
            }
            else {
                next = edges[j].v[0]->next;
                if (!next) next = p->vertex_list.first;
            }
            edges[j].v[1] = next;

            //printf("\nEdge check %d: %d,%d to %d,%d", j, edges[j].v[0]->data.xyzw[0], edges[j].v[0]->data.xyzw[1], edges[j].v[1]->data.xyzw[0], edges[j].v[1]->data.xyzw[1]);
            // TODO: verify vs. winding order if slope is positive or negative properly?
            // original though twas just to verify direction but that doesn't work...

            // Check if edge intersects scanline
            i32 min_y = edges[j].v[0]->data.xyzw[1];
            i32 max_y = edges[j].v[1]->data.xyzw[1];
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
                //printf("\nFound edge %d: %d,%d to %d,%d", j, edges[j].v[0]->data.xyzw[0], edges[j].v[0]->data.xyzw[1], edges[j].v[1]->data.xyzw[0], edges[j].v[1]->data.xyzw[1]);
            total_found++;
            if (min_y > max_y) {
                struct NDS_GE_VTX_list_node *t = edges[j].v[0];
                edges[j].v[0] = edges[j].v[1];
                edges[j].v[1] = t;

            }
            edges[j].found = 1;
        }
        if (total_found == 2) break;
    }
    if (total_found < 2) printf("\nFAILED to find one of the edges!!");

    //for (u32 j = 0; j < 2; j++)
    //    printf("\nEdge %d vtx 0 %d %d RGB %04x", j, edges[j].v[0]->data.xyzw[0], edges[j].v[0]->data.xyzw[1], edges[j].v[0]->color);
}

static void NDS_RE_interp_set_x(struct NDS_RE_interp *this, i32 x)
{
    x -= this->x0;
    this->x = x;
    if (this->xdiff != 0 && !this->linear)
    {
        i64 num = ((s64)x * this->w0n) << this->shift;
        i32 den = (x * this->w0d) + ((this->xdiff - x) * this->w1d);

        if (den == 0) this->yfactor = 0;
        else this->yfactor = (i32)(num / den);
    }
}

static i32 NDS_RE_interpolate(struct NDS_RE_interp *this, i32 y0, i32 y1)
{
    if ((!this->xdiff) || y0 == y1) return y0;

    if (!this->linear) { // PERP? is that the name?
        if (y0 < y1)
            return y0 + (((y1-y0) * this->yfactor) >> this->shift);
        else
            return y1 + (((y0-y1) * ((1<<this->shift)-this->yfactor)) >> this->shift);
    }
    else { // LERP
        if (y0 < y1)
            return y0 + (s64)(y1-y0) * this->x / this->xdiff;
        else
            return y1 + (s64)(y0-y1) * (this->xdiff - this->x) / this->xdiff;
    }
}

static void interpolate_edge_to_vertex(struct NDS_RE_EDGE *e, struct NDS_RE_VERTEX *v, i32 y, u32 do_tex) {
    /*struct NDS_GE_VTX_list_node *t = e->v[0];
    e->v[0] = e->v[1];
    e->v[1] = t;*/
    struct NDS_RE_interp xi;
    NDS_RE_interp_setup(&xi, 9, e->v[0]->data.xyzw[1], e->v[1]->data.xyzw[1], 10, 10);

    NDS_RE_interp_setup(&e->interp, 9, e->v[0]->data.xyzw[1], e->v[1]->data.xyzw[1], e->v[0]->data.w_normalized, e->v[1]->data.w_normalized);

    NDS_RE_interp_set_x(&e->interp, y);
    NDS_RE_interp_set_x(&xi, y);

    v->xx = NDS_RE_interpolate(&xi, e->v[0]->data.xyzw[0], e->v[1]->data.xyzw[0]);
    v->yy = y;
    v->zz = NDS_RE_interpolate(&e->interp, e->v[0]->data.xyzw[2], e->v[1]->data.xyzw[2]);
    v->ww = NDS_RE_interpolate(&e->interp, e->v[0]->data.w_normalized, e->v[1]->data.w_normalized);
    v->color[0] = NDS_RE_interpolate(&e->interp, e->v[0]->data.color[0], e->v[1]->data.color[0]);
    v->color[1] = NDS_RE_interpolate(&e->interp, e->v[0]->data.color[1], e->v[1]->data.color[1]);
    v->color[2] = NDS_RE_interpolate(&e->interp, e->v[0]->data.color[2], e->v[1]->data.color[2]);
    if (do_tex) {
        v->uv[0] = NDS_RE_interpolate(&e->interp, e->v[0]->data.uv[0], e->v[1]->data.uv[0]);
        v->uv[1] = NDS_RE_interpolate(&e->interp, e->v[0]->data.uv[1], e->v[1]->data.uv[1]);
    }
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
    struct NDS_RE_interp interp;

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

        interpolate_edge_to_vertex(&edges[0], &lerped[0], line_num, tex_enable);
        interpolate_edge_to_vertex(&edges[1], &lerped[1], line_num, tex_enable);

        u32 xorby = lerped[0].xx > lerped[1].xx ? 1 : 0;
        struct NDS_RE_VERTEX *left = &lerped[0 ^ xorby];
        struct NDS_RE_VERTEX *right = &lerped[1 ^ xorby];
        NDS_RE_interp_setup(&interp, 8, left->xx, right->xx, left->ww, right->ww);

        u32 rside = right->xx > 255 ? 255 : right->xx;

        for (u32 x = left->xx; x < rside; x++) {
            if (x < 256) {
                NDS_RE_interp_set_x(&interp, x);
                u32 comparison, depth;
                if (b->depth_buffering_w)
                    depth = NDS_RE_interpolate(&interp, left->ww, right->ww);
                else
                    depth = NDS_RE_interpolate(&interp, left->zz, right->zz);

                if (p->attr.depth_test_mode == 0) comparison = (i32)depth < line->depth[x];
                else comparison = (u32)depth == line->depth[x];

                if (comparison) {
                    u32 pix_r5, pix_g5, pix_b5, pix_a5;
                    float cr, cg, cb;
                    cr = NDS_RE_interpolate(&interp, left->color[0], right->color[0]);
                    if (cr < 0) cr = 0;
                    if (cr > 63) cr = 63;
                    cg = NDS_RE_interpolate(&interp, left->color[1], right->color[1]);
                    if (cg < 0) cg = 0;
                    if (cg > 63) cg = 63;
                    cb = NDS_RE_interpolate(&interp, left->color[2], right->color[2]);
                    if (cb < 0) cb = 0;
                    if (cb > 63) cb = 63;
                    if (tex_enable && p->sampler.sample) {
                        i32 final_s = NDS_RE_interpolate(&interp, left->uv[0], right->uv[0]);
                        i32 final_t = NDS_RE_interpolate(&interp, left->uv[1], right->uv[1]);
                        u32 tex_r6, tex_g6, tex_b6, tex_a6;
                        final_tex_coord(&p->sampler, &final_s, &final_t);
                        p->sampler.sample(this, &p->sampler, p, final_s, final_t, &tex_r6, &tex_g6, &tex_b6,
                                          &tex_a6);

                        float tex_rf = (float)tex_r6;
                        float tex_gf = (float)tex_g6;
                        float tex_bf = (float)tex_b6;
                        float tex_af = (float)tex_a6;

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

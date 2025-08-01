//
// Created by . on 3/12/25.
//

#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "nds_re.h"
#include "system/nds/nds_bus.h"
#include "system/nds/nds_vram.h"

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

static void clear_stencil(struct NDS *this, struct NDS_RE_LINEBUFFER *l) {
    memset(&l->stencil, 0, sizeof(l->stencil));
}

static void clear_line(struct NDS *this, struct NDS_RE_LINEBUFFER *l) {
    for (u32 x = 0; x < 256; x++) {
        l->poly_id[x] = this->re.io.CLEAR.poly_id;
        l->rgb[x] = this->re.io.CLEAR.COLOR;
        //l->rgb_bottom[x] = this->re.io.CLEAR.COLOR;
        l->alpha[x] = this->re.io.CLEAR.alpha;
        l->has[x] = 0;
        l->extra_attr[x].u = 0;
        //l->depth[x] = INT32_MAX; //this->re.io.clear.depth;
        l->tex_param[x].u = 0;
        l->stencil[x] = 0;
        l->depth[x] = this->re.io.CLEAR.depth;
    }

    if (this->re.io.DISP3DCNT.rear_plane_is_bitmap) {
        u32 x_offset = this->re.io.CLRIMAGE_OFFSET & 0xFF;
        u32 y_offset = (this->re.io.CLRIMAGE_OFFSET >> 8) & 0xFF;
        for (u32 x = 0; x < 256; x++) {
            u16 val2 =  NDS_VRAM_tex_read(this, 0x40000 + (y_offset << 9) + (x_offset << 1), 2);
            u16 val3 = NDS_VRAM_tex_read(this, 0x60000 + (y_offset << 9) + (x_offset << 1), 2);

            u32 r = (val2 << 1) & 0x3E;
            u32 g = (val2 >> 4) & 0x3E;
            u32 b = (val2 >> 9) & 0x3E;
            if (r) r++;
            if (g) g++;
            if (b) b++;
            u32 a = (val2 & 0x8000) ? 0x1F000000 : 0;
            u32 color = r | (g << 8) | (b << 16) | a;

            u32 z = ((val3 & 0x7FFF) * 0x200) + 0x1FF;

            l->rgb[x] = color;
            l->depth[x] = z;
            l->poly_id[x] = this->re.io.CLEAR.poly_id | 0x8000;

            x_offset++;
        }
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
    edges[0].v[1] = first_vertex;
    edges[0].dir = -1;

    edges[1].v[1] = first_vertex;
    edges[1].dir = 1;
    edges[0].found = edges[1].found = 0;
    u32 total_found = 0;

    for (u32 i = 0; i < p->vertex_list.len; i++) {
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
            u32 swapped = 0;
            if (max_y < min_y) {
                i32 t = min_y;
                min_y = max_y;
                max_y = t;
                swapped = 1;
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
            if (swapped) {
                struct NDS_GE_VTX_list_node *t = edges[j].v[0];
                edges[j].v[0] = edges[j].v[1];
                edges[j].v[1] = t;
            }
            edges[j].found = 1;
        }
        if (total_found == 2) break;
    }
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
    v->original_w = NDS_RE_interpolate(&e->interp, e->v[0]->data.xyzw[3], e->v[1]->data.xyzw[3]);
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

static const u32 ctabl[32] = {
        0, 3, 5, 7, 9, 11, 13, 15,
        17, 19, 21, 23, 25, 27, 29, 31,
        33, 35, 37, 39, 41, 43, 45, 47,
        49, 51, 53, 55, 57, 59, 61, 63
};

static inline u32 tex(u32 colo)
{
    return colo & 0x1F;
}

static inline u32 texc(u32 colo)
{
    //return ctabl[colo & 31];
    u32 v = ((colo & 0x1F) << 1);
    if (v) v++;//| ((colo & 0x10) >> 4);
    return v;
}


static float vtx_to_float(i32 v)
{
    return ((float)v) / 4096.0f;
}

static void ct_basic_read(u32 color, u32 *r, u32 *g, u32 *b, u32 *a)
{
    *r = texc(color);
    *g = texc(color >> 5);
    *b = texc(color >> 10);
    *a = 31;
}

static void ct_transparent(u32 *r, u32 *g, u32 *b, u32 *a)
{
    *r = *b = *g = *a = 0;
}

static void ct_blend1(u32 color, u32 *r, u32 *g, u32 *b, u32 *a)
{
    u32 color0 = color & 0xFFFF;
    u32 color1 = color >> 16;
    *r = (texc(color0) + texc(color1)) >> 1;
    *g = (texc(color0 >> 5) + texc(color1 >> 5)) >> 1;
    *b = (texc(color0 >> 10) + texc(color1 >> 10)) >> 1;
    *a = 31;
}

static void ct_blend5(u32 color, u32 balance, u32 *r, u32 *g, u32 *b, u32 *a)
{
    u32 color0 = color & 0xFFFF;
    u32 color1 = color >> 16;
    u32 mul0, mul1;
    if (balance) mul0=3;
    else mul0=5;
    // (Color0*5+Color1*3)/8
    mul1 = 8 - mul0;
    *r = ((texc(color0) * mul0) + (texc(color1) * mul1)) >> 3;
    *g = ((texc(color0 >> 5) * mul0) + (texc(color1 >> 5) * mul1)) >> 3;
    *b = ((texc(color0 >> 10) * mul0) + (texc(color1 >> 10) * mul1)) >> 3;
    *a = 31;
}

static void sample_texture_a3i5(struct NDS *this, struct NDS_RE_TEX_SAMPLER *ts, struct NDS_RE_POLY *p, u32 s, u32 t, u32 *r, u32 *g, u32 *b, u32 *a)
{
    //Alpha=(Alpha*4)+(Alpha/2).
    u8 sample = NDS_VRAM_tex_read(this, ts->tex_addr+((t*ts->s_size)+s), 1);
    u32 color = NDS_VRAM_pal_read(this, ts->pltt_base+((sample & 31) * 2), 2);
    *r = texc(color);
    *g = texc(color >> 5);
    *b = texc(color >> 10);
    *a =  ((sample >> 3) & 0x1C) + (sample >> 6);
}

static void sample_texture_a5i3(struct NDS *this, struct NDS_RE_TEX_SAMPLER *ts, struct NDS_RE_POLY *p, u32 s, u32 t, u32 *r, u32 *g, u32 *b, u32 *a)
{
    //Each Texel occupies 8bit, the 1st Texel is located in 1st byte.
    //  Bit0-2: Color Index (0..7) of a 8-color Palette
    //  Bit3-7: Alpha       (0..31; 0=Transparent, 31=Solid)
    // y*width bytes + x
    u8 sample = NDS_VRAM_tex_read(this, ts->tex_addr+((t*ts->s_size)+s), 1);
    u32 color = NDS_VRAM_pal_read(this, ts->pltt_base+((sample & 7) * 2), 2);
    *r = texc(color);
    *g = texc(color >> 5);
    *b = texc(color >> 10);
    *a = sample >> 3;
}

static void sample_texture_compressed(struct NDS *this, struct NDS_RE_TEX_SAMPLER *ts, struct NDS_RE_POLY *p, u32 s, u32 t, u32 *r, u32 *g, u32 *b, u32 *a)
{
    u32 vramaddr = ts->tex_addr;
    vramaddr += ((t & 0x3FC) * (ts->s_size>>2)) + (s & 0x3FC);
    vramaddr += (t & 0x3);
    vramaddr &= 0x7FFFF;

    u32 slot1addr = 0x20000 + ((vramaddr & 0x1FFFC) >> 1);
    if (vramaddr >= 0x40000)
        slot1addr += 0x10000;

    u8 val;
    if (vramaddr >= 0x20000 && vramaddr < 0x40000) // reading slot 1 for texels should always read 0
        val = 0;
    else
    {
        val = NDS_VRAM_tex_read(this, vramaddr, 1);
        val >>= (2 * (s & 0x3));
    }

    u16 palinfo = NDS_VRAM_tex_read(this, slot1addr, 2);
    u32 paloffset = (palinfo & 0x3FFF) << 2;
    u32 texpal = ts->pltt_base;

    u32 color;
    switch (val & 3)
    {
        case 0:
            color = NDS_VRAM_pal_read(this, texpal + paloffset, 2);
            *r = texc(color);
            *g = texc(color >> 5);
            *b = texc(color >> 10);
            *a = 31;
            break;

        case 1:
            color = NDS_VRAM_pal_read(this, texpal + paloffset + 2, 2);
            *r = texc(color);
            *g = texc(color >> 5);
            *b = texc(color >> 10);
            *a = 31;
            break;

        case 2:
            if ((palinfo >> 14) == 1) {
                u32 color0 = NDS_VRAM_pal_read(this, texpal + paloffset, 2);
                u32 color1 = NDS_VRAM_pal_read(this, texpal + paloffset + 2, 2);

                u32 r0 = color0 & 0x1F;
                u32 g0 = (color0 >> 5) & 0x1F;
                u32 b0 = (color0 >> 10) & 0x1F;
                u32 r1 = color1 & 0x1F;
                u32 g1 = (color1 >> 5) & 0x1F;
                u32 b1 = (color1 >> 10) & 0x1F;

                u32 mr = (r0 + r1) >> 1;
                u32 mg = (g0 + g1) >> 1;
                u32 mb = (b0 + b1) >> 1;

                *r = texc(mr);
                *g = texc(mg);
                *b = texc(mb);
            }
            else if ((palinfo >> 14) == 3) {
                u32 color0 = NDS_VRAM_pal_read(this, texpal + paloffset, 2);
                u32 color1 = NDS_VRAM_pal_read(this, texpal + paloffset + 2, 2);

                u32 r0 = color0 & 0x1F;
                u32 g0 = (color0 >> 5) & 0x1F;
                u32 b0 = (color0 >> 10) & 0x1F;
                u32 r1 = color1 & 0x1F;
                u32 g1 = (color1 >> 5) & 0x1F;
                u32 b1 = (color1 >> 10) & 0x1F;

                u32 mr = (r0*5 + r1*3) >> 3;
                u32 mg = (g0*5 + g1*3) >> 3;
                u32 mb = (b0*5 + b1*3) >> 3;

                *r = texc(mr);
                *g = texc(mg);
                *b = texc(mb);
            }
            else {
                color = NDS_VRAM_pal_read(this, texpal + paloffset + 4, 2);
                *r = texc(color);
                *g = texc(color >> 5);
                *b = texc(color >> 10);
            }
            *a = 31;
            break;

        case 3:
            if ((palinfo >> 14) == 2) {
                color = NDS_VRAM_pal_read(this, texpal + paloffset + 6, 2);
                *r = texc(color);
                *g = texc(color >> 5);
                *b = texc(color >> 10);
                *a = 31;
            }
            else if ((palinfo >> 14) == 3)
            {
                u32 color0 = NDS_VRAM_pal_read(this, texpal + paloffset, 2);
                u32 color1 = NDS_VRAM_pal_read(this, texpal + paloffset + 2, 2);

                u32 r0 = color0 & 0x1F;
                u32 g0 = (color0 >> 5) & 0x1F;
                u32 b0 = (color0 >> 10) & 0x1F;
                u32 r1 = color1 & 0x1F;
                u32 g1 = (color1 >> 5) & 0x1F;
                u32 b1 = (color1 >> 10) & 0x1F;

                u32 mr = (r0*3 + r1*5) >> 3;
                u32 mg = (g0*3 + g1*5) >> 3;
                u32 mb = (b0*3 + b1*5) >> 3;

                *r = texc(mr);
                *g = texc(mg);
                *b = texc(mb);
                *a = 31;
            }
            else {
                *r = *g = *b = *a = 0;
            }
            break;
    }
}


static void sample_texture_palette_4bpp(struct NDS *this, struct NDS_RE_TEX_SAMPLER *ts, struct NDS_RE_POLY *p, u32 s, u32 t, u32 *r, u32 *g, u32 *b, u32 *a)
{
    u32 addr = ((t * ts->s_size) + s);
    u32 c = NDS_VRAM_tex_read(this, ts->tex_addr + (addr >> 1), 1) & 0xFF;
    c >>= ((addr & 1) << 2);
    c &= 0x0F;
    *a = c ? 31 : ts->alpha0;

    c = NDS_VRAM_pal_read(this, ts->pltt_base + (c*2), 2);

    *r = texc(c);
    *g = texc(c >> 5);
    *b = texc(c >> 10);
}

static void sample_texture_palette_2bpp(struct NDS *this, struct NDS_RE_TEX_SAMPLER *ts, struct NDS_RE_POLY *p, u32 s, u32 t, u32 *r, u32 *g, u32 *b, u32 *a)
{
    u32 addr = ((t * ts->s_size) + s);
    u32 c = NDS_VRAM_tex_read(this, ts->tex_addr + (addr >> 2), 1) & 0xFF;
    c = (c >> (2 * (addr & 3))) & 3;

    *a = c ? 31 : ts->alpha0;
    c = NDS_VRAM_pal_read(this, ts->pltt_base + (c*2), 2);

    *r = texc(c);
    *g = texc(c >> 5);
    *b = texc(c >> 10);
}

static void sample_texture_palette_8bpp(struct NDS *this, struct NDS_RE_TEX_SAMPLER *ts, struct NDS_RE_POLY *p, u32 s, u32 t, u32 *r, u32 *g, u32 *b, u32 *a)
{
    u32 addr = ((t * ts->s_size) + s);
    u32 c = NDS_VRAM_tex_read(this, ts->tex_addr+addr, 1) & 0xFF;

    *a = c ? 31 : ts->alpha0;
    c = NDS_VRAM_pal_read(this, ts->pltt_base + (c*2), 2);

    *r = texc(c);
    *g = texc(c >> 5);
    *b = texc(c >> 10);
}


static void sample_texture_direct(struct NDS *this, struct NDS_RE_TEX_SAMPLER *ts, struct NDS_RE_POLY *p, u32 s, u32 t, u32 *r, u32 *g, u32 *b, u32 *a)
{
    // 16-bit read from VRAM @ ptr ((t * size) + s) * 2
    u32 c = NDS_VRAM_tex_read(this, ts->tex_addr+(((t * ts->s_size) + s) << 1), 2) & 0xFFFF;
    *r = texc(c);
    *g = texc(c >> 5);
    *b = texc(c >> 10);
    *a = ((c >> 15) & 1) * 31;
}

static void final_tex_coord(struct NDS_RE_TEX_SAMPLER *ts, i32 *s, i32 *t)
{
    //printf("\nINPUT S,T: %f, %f", uv_to_float(*s), uv_to_float(*t));
    i32 ms = ((i32)*s) >> 4;
    i32 mt = ((i32)*t) >> 4;

    if (ts->s_repeat) {
        if (ts->s_flip)
        {
            if (ms & ts->s_size) ms = (ts->s_size-1) - (ms & ts->s_mask);
            else ms = ms & ts->s_mask;
        }
        else
            ms &= ts->s_mask;
    }
    else
    {
        if (ms < 0) ms = 0;
        else if (ms >= ts->s_size) ms = ts->s_mask;
    }


    if (ts->t_repeat) {
        if (ts->t_flip)
        {
            if (mt & ts->t_size) mt = (ts->t_size-1) - (mt & ts->t_mask);
            else mt = mt & ts->t_mask;
        }
        else
            mt &= ts->t_mask;
    }
    else
    {
        if (mt < 0) mt = 0;
        else if (mt >= ts->t_size) mt = ts->t_mask;
    }

    *s = ms;
    *t = mt;

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
    ts->alpha0 = p->tex_param.color0_is_transparent ? 0 : 31;
    ts->pltt_base = p->tex_param.format == 2 ? p->pltt_base << 3 : p->pltt_base << 4;
    ts->s_size_fp = (ts->s_size << 4);
    ts->t_size_fp = (ts->t_size << 4);
    ts->s_mask = ts->s_size - 1;
    ts->t_mask = ts->t_size - 1;
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
        case 1:
            ts->sample = &sample_texture_a3i5;
            break;
        case 2:
            ts->sample = &sample_texture_palette_2bpp;
            break;
        case 3: // 4-bit palette!
            ts->sample = &sample_texture_palette_4bpp;
            break;
        case 5:
            ts->tex_slot = (ts->tex_addr >> 17);
            if (ts->tex_slot == 0) {
                ts->tex_slot1_addr = (ts->tex_addr >> 1) + (128 * 1024);
            }
            else {
                ts->tex_slot1_addr = ((ts->tex_addr & 0x1FFFF) >> 1) + (128 * 1024) + 0x10000;
            }
            ts->sample = &sample_texture_compressed;
            break;
        case 4:
            ts->sample = &sample_texture_palette_8bpp;
            break;
        case 6:
            ts->sample = &sample_texture_a5i3;
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
    u32 last_was_smask = 0;
    struct NDS_RE_interp interp;

    u32 global_tex_enable = this->re.io.DISP3DCNT.texture_mapping;

    struct NDS_RE_EDGE edges[2];
    struct tex_sampler;
    for (u32 poly_num = 0; poly_num < b->polygon_index; poly_num++) {
        struct NDS_RE_POLY *p = this->re.render_list.items[poly_num];
        u32 tex_enable = global_tex_enable && (p->tex_param.format != 0);

        // Polygon does not intersect this line
        if ((line_num < p->min_y) || (line_num > p->max_y)) {
            continue;
        }

        if (p->attr.mode == 3) {
            if (!last_was_smask) clear_stencil(this, line);
            last_was_smask = 1;
        } else {
            last_was_smask = 0;
        }

        if (tex_enable && !p->sampler.filled_out) fill_tex_sampler(this, p);
        u32 poly_a5 = p->attr.alpha;

        // Find the correct points to lerp
        find_closest_points_marked(this, b, p, line_num, edges);

        struct NDS_RE_VERTEX lerped[2];

        interpolate_edge_to_vertex(&edges[0], &lerped[0], line_num, tex_enable);
        interpolate_edge_to_vertex(&edges[1], &lerped[1], line_num, tex_enable);

        u32 xorby = lerped[0].xx > lerped[1].xx ? 1 : 0;
        struct NDS_RE_VERTEX *left = &lerped[0 ^ xorby];
        struct NDS_RE_VERTEX *right = &lerped[1 ^ xorby];
        NDS_RE_interp_setup(&interp, 8, left->xx, right->xx, left->ww, right->ww);

        i32 depth_l, depth_r;
        if (b->depth_buffering_w) {
            depth_l = left->original_w;
            depth_r = right->original_w;
        } else {
            depth_l = left->zz;
            depth_r = right->zz;
        }

        u32 rside = right->xx > 255 ? 255 : right->xx;

        for (u32 x = left->xx; x < rside; x++) {
            NDS_RE_interp_set_x(&interp, x);
            u32 comparison, depth;
            depth = NDS_RE_interpolate(&interp, depth_l, depth_r);

            if (p->attr.depth_test_mode == 0) comparison = depth < line->depth[x];
            else comparison = (u32) depth == line->depth[x];
            u32 shading_mode = 1;
            if (comparison) {
                u32 pix_r6, pix_g6, pix_b6, pix_a5;
                u32 vr6, vg6, vb6;
                vr6 = NDS_RE_interpolate(&interp, left->color[0], right->color[0]) >> 3;
                vg6 = NDS_RE_interpolate(&interp, left->color[1], right->color[1]) >> 3;
                vb6 = NDS_RE_interpolate(&interp, left->color[2], right->color[2]) >> 3;
                if (tex_enable && p->sampler.sample) {
                    i32 final_s = NDS_RE_interpolate(&interp, left->uv[0], right->uv[0]);
                    i32 final_t = NDS_RE_interpolate(&interp, left->uv[1], right->uv[1]);
                    u32 tex_r6, tex_g6, tex_b6, tex_a5;
                    final_tex_coord(&p->sampler, &final_s, &final_t);
                    p->sampler.sample(this, &p->sampler, p, final_s, final_t, &tex_r6, &tex_g6, &tex_b6,
                                      &tex_a5);

                    // OK we have texture color now...how do we use it!?
                    switch (p->attr.mode) {
                        case 0: // modulation
                            shading_mode = 2;
                            pix_r6 = ((tex_r6 + 1) * (vr6 + 1) - 1) >> 6;
                            pix_g6 = ((tex_g6 + 1) * (vg6 + 1) - 1) >> 6;
                            pix_b6 = ((tex_b6 + 1) * (vb6 + 1) - 1) >> 6;

                            pix_a5 = ((tex_a5 + 1) * (poly_a5 + 1) - 1) >> 5;
                            break;
                        case 3: // shadow
                            printf("\nGOT HERE...");
                            if ((p->attr.poly_id == 0) || (p->attr.poly_id == line->poly_id[x]))
                                continue; // this pixel should be forgotten about
                            if (!line->stencil[x]) continue;
                            shading_mode = 6;
                            pix_r6 = vr6;
                            pix_g6 = vg6;
                            pix_b6 = vb6;
                            pix_a5 = poly_a5;
                            break;
                        case 1: // decal
                            shading_mode = 3;
                            switch (tex_a5) {
                                case 0:
                                    pix_r6 = vr6;
                                    pix_g6 = vg6;
                                    pix_b6 = vb6;
                                    break;
                                case 31:
                                    pix_r6 = tex_r6;
                                    pix_g6 = tex_g6;
                                    pix_b6 = tex_b6;
                                    break;
                                default:
                                    // // R = (Rt*At + Rv*(63-At))/64  ;except, when At=0: R=Rv, when At=31: R=Rt
                                    pix_r6 = (tex_r6 * tex_a5 + vr6 * (31 - tex_a5)) >> 5;
                                    pix_g6 = (tex_g6 * tex_a5 + vg6 * (31 - tex_a5)) >> 5;
                                    pix_b6 = (tex_b6 * tex_a5 + vb6 * (31 - tex_a5)) >> 5;
                                    break;
                            }
                            pix_a5 = poly_a5;
                            break;
                        case 2: {// highlight/toon
                            u32 idx = vg6 >> 1;
                            u32 rs = this->re.io.TOON_TABLE_r[idx];
                            u32 gs = this->re.io.TOON_TABLE_g[idx];
                            u32 bs = this->re.io.TOON_TABLE_b[idx];
                            switch (this->re.io.DISP3DCNT.polygon_attr_shading) {
                                case 0: { // toon
                                    shading_mode = 4;
                                    // R = ((Rt+1)*(Rs+1)-1)/64
                                    pix_r6 = ((tex_r6 + 1) * (rs + 1) - 1) >> 6;
                                    pix_g6 = ((tex_g6 + 1) * (gs + 1) - 1) >> 6;
                                    pix_b6 = ((tex_b6 + 1) * (bs + 1) - 1) >> 6;
                                    break;
                                }
                                case 1: { // highlight
                                    shading_mode = 5;
                                    pix_r6 = ((tex_r6 + 1) * (vr6 + 1) - 1) >> 6;
                                    pix_g6 = ((tex_g6 + 1) * (vg6 + 1) - 1) >> 6;
                                    pix_b6 = ((tex_b6 + 1) * (vb6 + 1) - 1) >> 6;
                                    break;
                                }
                            }
                            // TODO: uhhhhh....?
                            pix_a5 = (tex_a5 * p->attr.alpha) >> 5;

                            //pix_a5 = poly_a5;
                            break;
                        }
                    }
                } else {
                    if (p->attr.mode == 3) {
                        if ((p->attr.poly_id == 0) || (p->attr.poly_id == line->poly_id[x]))
                            continue; // this pixel should be forgotten about
                        if (!line->stencil[x]) continue;
                    }
                    pix_r6 = vr6;
                    pix_g6 = vg6;
                    pix_b6 = vb6;
                    pix_a5 = p->attr.alpha;
                }
                if (pix_a5) {
                    if (pix_r6 > 63) pix_r6 = 63;
                    if (pix_g6 > 63) pix_g6 = 63;
                    if (pix_b6 > 63) pix_b6 = 63;
                    if (pix_a5 > 31) pix_a5 = 31;
                    if (pix_a5 < this->re.io.ALPHA_TEST_REF) continue;
                    if ((pix_a5 < 31) && (this->re.io.DISP3DCNT.alpha_blending) && (line->alpha[x] != 0)) {
                        // BLEND ALPHA!
                        if (line->poly_id[x] == p->attr.poly_id) {
                            continue;
                        }
                        u32 fb_r6 = line->rgb[x] & 0x3F;
                        u32 fb_g6 = (line->rgb[x] >> 6) & 0x3F;
                        u32 fb_b6 = (line->rgb[x] >> 12) & 0x3F;
                        u32 fb_a5 = line->alpha[x];

                        pix_a5++;
                        pix_r6 = ((pix_r6 * pix_a5) + (fb_r6 * (32 - pix_a5))) >> 5;
                        pix_g6 = ((pix_g6 * pix_a5) + (fb_g6 * (32 - pix_a5))) >> 5;
                        pix_b6 = ((pix_b6 * pix_a5) + (fb_b6 * (32 - pix_a5))) >> 5;
                        pix_a5--;

                        if (p->attr.translucent_set_new_depth) pix_a5 = pix_a5 > fb_a5 ? pix_a5 : fb_a5;
                        else pix_a5 = fb_a5;
                    }

                    u32 alpha_out = 0x8000 * (pix_a5 > 0);
                    line->rgb[x] = pix_r6 | (pix_g6 << 6) | (pix_b6 << 12) | alpha_out;
                    line->tex_param[x] = p->tex_param;
                    line->poly_id[x] = p->attr.poly_id;
                    line->extra_attr[x].vertex_mode = p->vertex_mode + 1;
                    line->extra_attr[x].has_px = 1;
                    line->has[x] = 1;
                    line->extra_attr[x].shading_mode = shading_mode;
                    line->depth[x] = (u32) depth;
                }
            } else if ((p->attr.mode == 3) && (p->attr.poly_id == 0)) {
                line->stencil[x] = 1;
            }
        }
    }
}

int poly_comparator(const void *a, const void *b)
{
    return ((struct NDS_RE_POLY *)a)->sorting_key - ((struct NDS_RE_POLY *)b)->sorting_key;
}

static void copy_and_sort_list(struct NDS *this, struct NDS_GE_BUFFERS *b)
{
    this->re.render_list.len = 0;
    this->re.render_list.num_opaque = 0;
    this->re.render_list.num_translucent = 0;
    for (u32 i = 0; i < b->polygon_index; i++) {
        struct NDS_RE_POLY *p = &b->polygon[i];
        this->re.render_list.num_opaque += p->is_translucent ^ 1;
        this->re.render_list.num_translucent += p->is_translucent;
    }

    u32 opaque_index = 0;
    u32 transparent_index = this->re.render_list.num_opaque;
    for (u32 i = 0; i < b->polygon_index; i++) {
        struct NDS_RE_POLY *p = &b->polygon[i];
        if (p->is_translucent)
            this->re.render_list.items[transparent_index++] = p;
        else
            this->re.render_list.items[opaque_index++] = p;
    }
    u32 num_to_sort = b->translucent_y_sorting_manual ? this->re.render_list.num_opaque : b->polygon_index;

    qsort(this->re.render_list.items, num_to_sort, sizeof(this->re.render_list.items[0]), &poly_comparator);
}

void NDS_RE_render_frame(struct NDS *this)
{
    struct NDS_GE_BUFFERS *b = &this->ge.buffers[this->ge.ge_has_buffer ^ 1];
    copy_and_sort_list(this, b);
    for (u32 i = 0; i < 192; i++)
        render_line(this, b, i);
}

void NDS_RE_init(struct NDS *this)
{

}

void NDS_RE_reset(struct NDS *this)
{

}

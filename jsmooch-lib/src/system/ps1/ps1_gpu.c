//
// Created by . on 2/15/25.
//
#include <string.h>
#include <math.h>

#include "ps1_bus.h"
#include "ps1_gpu.h"

#include "helpers/multisize_memaccess.c"

static void gp0_cmd(struct PS1_GPU *this, u32 cmd);

typedef void (*setpix_func)(struct PS1_GPU *this, i32 y, i32 x, u32 color, u32 transparent);

static void default_setpix(struct PS1_GPU *this, i32 y, i32 x, u32 color, u32 transparent)
{
    // VRAM is 512 1024-wide 16-bit words. so 2048 bytes per line
    i32 ry = (y & 511) + this->draw_y_offset;
    i32 rx = (x & 1023) + this->draw_x_offset;
    if ((ry < this->draw_area_top) || (ry > this->draw_area_bottom)) return;
    if ((rx < this->draw_area_left) || (rx > this->draw_area_right)) return;
    u32 addr = (2048*ry)+(rx*2);
    cW[2](this->VRAM, addr, color);
}



void PS1_GPU_texture_sampler_new(struct PS1_GPU_TEXTURE_SAMPLER *this, u32 page_x, u32 page_y, u32 clut, struct PS1_GPU *ctrl)
{
    this->page_x = (page_x & 0x0F) << 6;
    this->page_y = (page_y & 1) << 8;
    this->base_addr = (page_y << 11) + (page_x << 1);
    u32 clx = (clut & 0x3F) << 4;
    u32 cly = (clut >> 6) & 0x1FF;
    this->clut_addr = (cly << 11) + (clx << 2);
    this->VRAM = ctrl->VRAM;
}

#define R_GPUSTAT 0
#define R_GPUPLAYING 1
#define R_GPUQUIT 2
#define R_GPUGP1 3
#define R_GPUREAD 4
#define R_LASTUSED 23

void PS1_GPU_init(struct PS1 *this)
{
    memset(this->gpu.VRAM, 0x77, 1024*1024);

    this->gpu.display_horiz_start = 0x200;
    this->gpu.display_horiz_end = 0xC00;
    this->gpu.display_line_start = 0x10;
    this->gpu.display_line_end = 0x100;
    this->gpu.handle_gp0 = &gp0_cmd;
}

static void ins_null(struct PS1_GPU *this)
{

}

static inline void unready_cmd(struct PS1_GPU *this)
{
    this->io.GPUSTAT &= 0xFBFFFFFF;
}

static inline void unready_recv_dma(struct PS1_GPU *this)
{
    this->io.GPUSTAT &= 0xEFFFFFFF;
}

static inline void unready_vram_to_CPU(struct PS1_GPU *this)
{
    this->io.GPUSTAT &= 0xF7FFFFFF;
}

static inline void unready_all(struct PS1_GPU *this)
{
    unready_cmd(this);
    unready_recv_dma(this);
    unready_vram_to_CPU(this);
}

static inline void ready_cmd(struct PS1_GPU *this)
{
    this->io.GPUSTAT |= 0x4000000;
}

static inline void ready_recv_dma(struct PS1_GPU *this)
{
    this->io.GPUSTAT |= 0x10000000;
}

static inline void ready_vram_to_CPU(struct PS1_GPU *this)
{
    this->io.GPUSTAT |= 0x8000000;
}

static inline void ready_all(struct PS1_GPU *this)
{
    ready_cmd(this);
    ready_recv_dma(this);
    ready_vram_to_CPU(this);
}


static inline u32 BGR24to15(u32 c)
{
    return (((c >> 19) & 0x1F) << 10) |
           (((c >> 11) & 0x1F) << 5) |
           ((c >> 3) & 0x1F);
}

static void cmd02_quick_rect(struct PS1_GPU *this)
{
    unready_all(this);
    u32 ysize = (this->cmd[2] >> 16) & 0xFFFF;
    u32 xsize = (this->cmd[2]) & 0xFFFF;
    u32 BGR = BGR24to15(this->cmd[0] & 0xFFFFFF);
    u32 start_y = (this->cmd[1] >> 16) & 0xFFFF;
    u32 start_x = (this->cmd[1]) & 0xFFFF;
    //if (LOG_DRAW_QUADS) console.log('QUICKRECT! COLOR', hex4(BGR), 'X Y', start_x, start_y, 'SZ X SZ Y', xsize, ysize);
    for (u32 y = start_y; y < (start_y+ysize); y++) {
        for (u32 x = start_x; x < (start_x + xsize); x++) {
            //this->setpix(y, x, BGR);
            u32 addr = (2048*y)+(x*2);
            cW[2](this->VRAM, addr, BGR);
        }
    }

    ready_all(this);
}

static inline i32 mksigned16(u32 v)
{
    return SIGNe16to32(v);
}

static inline i32 mksigned11(u32 v)
{
    return SIGNe11to32(v);
}

static void draw_flat_triangle(struct PS1_GPU *this, i32 x0, i32 y0, i32 x1, i32 y1, i32 x2, i32 y2, u32 color)
{
    // sort points vertically
    i32 a, b;
    if (y1 > y2) {
        a = x1;
        b = y1;
        x1 = x2;
        y1 = y2;
        x2 = a;
        y2 = b;
    }

    if (y0 > y1) {
        a = x0;
        b = y0;
        x0 = x1;
        y0 = y1;
        x1 = a;
        y1 = b;
    }

    if (y1 > y2) {
        a = x1;
        b = y1;
        x1 = x2;
        y1 = y2;
        x2 = a;
        y2 = b;
    }

    float dx_far = (float)(x2 - x0) / (float)(y2 - y0 + 1);
    float dx_upper = (float)(x1 - x0) / (float)(y1 - y0 + 1);
    float dx_low = (float)(x2 - x1) / (float)(y2 - y1 + 1);
    float xf = (float)x0;
    float xt = (float)x0 + dx_upper;
    for (i32 y = y0; y <= y2; y++) {
        if (y >= 0) {
            for (i32 x = (i32)(xf > 0 ? xf : 0); x <= (i32)xt; x++)
                default_setpix(this, y, x, color, 0);
            for (i32 x = (i32)xf; x > (i32)(xt > 0 ? xt : 0); x--)
                default_setpix(this, y, x, color, 0);
        }
        xf += dx_far;
        if (y < y1)
            xt += dx_upper;
        else
            xt += dx_low;
    }
}

static void cmd28_draw_flat4untex(struct PS1_GPU *this)
{
    // Flat 4-vertex untextured poly
    i32 x0 = mksigned16(this->cmd[1] & 0xFFFF);
    i32 y0 = mksigned16(this->cmd[1] >> 16);
    i32 x1 = mksigned16(this->cmd[2] & 0xFFFF);
    i32 y1 = mksigned16(this->cmd[2] >> 16);
    i32 x2 = mksigned16(this->cmd[3] & 0xFFFF);
    i32 y2 = mksigned16(this->cmd[3] >> 16);
    i32 x3 = mksigned16(this->cmd[4] & 0xFFFF);
    i32 y3 = mksigned16(this->cmd[4] >> 16);

    /*
    For quads, I do v1, v2, v3 for one triangle and then v2, v3, v4 for the other
     */
    u32 color = BGR24to15(this->cmd[0] & 0xFFFFFF);
#ifdef LOG_DRAW_QUADS
    printf("\nflat4untex %d %d %d %d %d %d %06x", x0, y0, x1, y1, x2, y2, color);
#endif
    draw_flat_triangle(this, x0, y0, x1, y1, x2, y2, color);
    draw_flat_triangle(this, x1, y1, x2, y2, x3, y3, color);
}

static inline void v3_from_cmd2(struct PS1_GPU_VERTEX3 *this, u32 cmd){
    this->x = ((i32)(cmd & 0xFFFF) << 16) >> 16;
    this->y = ((i32)cmd) >> 16;
}

static inline void uv3_from_cmd(struct PS1_GPU_VERTEX3 *this, u32 cmd)
{
    this->u = cmd & 0xFF;
    this->v = (cmd >> 8) & 0xFF;
}

static void fst_draw_line(struct PS1_GPU *this, i32 y, float fx1, float fx2, float r1, float r2, float g1, float g2, float b1, float b2, setpix_func setpix) {
    i32 x1 = (i32)fx1;
    i32 x2 = (i32)fx2;
    if (x1 > x2) {
        i32 a = x1;
        float b = r1;
        float c = g1;
        float d = b1;
        x1 = x2;
        r1 = r2;
        g1 = g2;
        b1 = b2;
        x2 = a;
        r2 = b;
        g2 = c;
        b2 = d;
    }
    float r = 1.0f / (float)(x2 - x1);
    float rd = (r2 - r1) * r;
    float gd = (g2 - g1) * r;
    float bd = (b2 - b1) * r;

    for (i32 x = x1; x < x2; x++) {
        setpix(this, y, x, (((u32)r1 & 31)) | (((u32)g1 & 31) << 5) | (((u32)b1 & 31) << 10), 0);
        r1 += rd;
        g1 += gd;
        b1 += bd;
    }
}

static void ftt_draw_line(struct PS1_GPU *this, i32 y, float fx1, float fx2, float u1, float u2, float v1, float v2, struct PS1_GPU_TEXTURE_SAMPLER *ts, setpix_func setpix) {
    float x1 = roundf(fx1);
    float x2 = roundf(fx2);
    if (x1 > x2) {
        float a = x1;
        float b = u1;
        float  c = v1;
        x1 = x2;
        u1 = u2;
        v1 = v2;
        x2 = a;
        u2 = b;
        v2 = c;
    }
    float r = 1 / (x2 - x1);
    float ud = (u2 - u1) * r;
    float vd = (v2 - v1) * r;

    for (i32 x = (i32)x1; x < (i32)x2; x++) {
        u16 c =  ts->sample(ts, (i32)u1, (i32)v1);
        u16 hbit = c & 0x8000;
        u16 lbit = c & 0x7FFF;
        //if ((lbit != 0) || ((lbit == 0) && hbit)) setpix(this, (i32)y, (i32)x, 0, 0);
        if ((lbit != 0) || ((lbit == 0) && hbit)) default_setpix(this, y, x, lbit, 0);
        else setpix(this, y, x, c, 0);
        u1 += ud;
        v1 += vd;
    }
}

static void ftt_fill_bottom(struct PS1_GPU *this, struct PS1_GPU_VERTEX3 *v1, struct PS1_GPU_VERTEX3 *v2, struct PS1_GPU_VERTEX3 *v3, struct PS1_GPU_TEXTURE_SAMPLER *ts, setpix_func setpix)
{
    // fill flat-bottom triangle
    //console.log('YS!', v1->y, v2->y, v3->y)
    float r1 = 1.0f / (v2->y - v1->y);
    float r2 = 1.0f / (v3->y - v1->y);
    float islop1 = (v2->x - v1->x) * r1;
    float uslop1 = (v2->u - v1->u) * r1;
    float vslop1 = (v2->v - v1->v) * r1;
    
    float islop2 = (v3->x - v1->x) * r2;
    float uslop2 = (v3->u - v1->u) * r2;
    float vslop2 = (v3->v - v1->v) * r2;
    
    float cx1 = v1->x;
    float cx2 = v1->x;
    float cu1 = v1->u;
    float cu2 = v1->u;
    float cv1 = v1->v;
    float cv2 = v1->v;
    
    for (i32 y = (i32)v1->y; y <= (i32)v2->y; y++) {
        ftt_draw_line(this, y, cx1, cx2, cu1, cu2, cv1, cv2, ts, setpix);
        cx1 += islop1;
        cx2 += islop2;
        cu1 += uslop1;
        cu2 += uslop2;
        cv1 += vslop1;
        cv2 += vslop2;
    }
}

static void ftt_fill_top(struct PS1_GPU *this, struct PS1_GPU_VERTEX3 *v1, struct PS1_GPU_VERTEX3 *v2, struct PS1_GPU_VERTEX3 *v3, struct PS1_GPU_TEXTURE_SAMPLER *ts, setpix_func setpix)
{
    // fill flat-top triangle
    //     v2->     ->v1
    //    /  _____/
    //  /_____/
    // v3
    //
    float r1 = 1.0f / (v3->y - v1->y);
    float r2 = 1.0f / (v3->y - v2->y);

    float islop1 = (v3->x - v1->x) * r1;
    float uslop1 = (v3->u - v1->u) * r1;
    float vslop1 = (v3->v - v1->v) * r1;

    float islop2 = (v3->x - v2->x) * r2;
    float uslop2 = (v3->u - v2->u) * r2;
    float vslop2 = (v3->v - v2->v) * r2;

    float cu1 = v3->u;
    float cu2 = v3->u;
    float cv1 = v3->v;
    float cv2 = v3->v;

    float cx1 = v3->x;
    float cx2 = v3->x;
    for (i32 y = (i32)v3->y; y > (i32)v1->y; y--) {
        ftt_draw_line(this, y, cx1, cx2, cu1, cu2, cv1, cv2, ts, setpix);

        cx1 -= islop1;
        cx2 -= islop2;
        cu1 -= uslop1;
        cu2 -= uslop2;
        cv1 -= vslop1;
        cv2 -= vslop2;
    }
}

static void fst_fill_top(struct PS1_GPU *this, struct PS1_GPU_VERTEX3 *v1, struct PS1_GPU_VERTEX3 *v2, struct PS1_GPU_VERTEX3 *v3, setpix_func setpix)
{
    // fill flat-top triangle
    //     v2->     .v1
    //    /  _____/
    //  /_____/
    // v3
    //
    float r1 = 1.0f / (v3->y - v1->y);
    float r2 = 1.0f / (v3->y - v2->y);

    float islop1 = (v3->x - v1->x) * r1;
    float rslop1 = (float)(v3->r - v1->r) * r1;
    float gslop1 = (float)(v3->g - v1->g) * r1;
    float bslop1 = (float)(v3->b - v1->b) * r1;

    float islop2 = (v3->x - v2->x) * r2;
    float rslop2 = (float)(v3->r - v2->r) * r2;
    float gslop2 = (float)(v3->g - v2->g) * r2;
    float bslop2 = (float)(v3->b - v2->b) * r2;

    float cr1 = (float)v3->r;
    float cr2 = (float)v3->r;
    float cg1 = (float)v3->g;
    float cg2 = (float)v3->g;
    float cb1 = (float)v3->b;
    float cb2 = (float)v3->b;

    float cx1 = v3->x;
    float cx2 = v3->x;
    for (i32 y = (i32)v3->y; y > (i32)v1->y; y--) {
        fst_draw_line(this, y, cx1, cx2, (float)((u32)cr1 >> 3), (float)((u32)cr2 >> 3), (float)((u32)cg1 >> 3), (float)((u32)cg2 >> 3), (float)((u32)cb1 >> 3), (float)((u32)cb2 >> 3), setpix);

        cx1 -= islop1;
        cx2 -= islop2;
        cr1 -= rslop1;
        cr2 -= rslop2;
        cg1 -= gslop1;
        cg2 -= gslop2;
        cb1 -= bslop1;
        cb2 -= bslop2;
    }
}

static void fst_fill_bottom(struct PS1_GPU *this, struct PS1_GPU_VERTEX3 *v1, struct PS1_GPU_VERTEX3 *v2, struct PS1_GPU_VERTEX3 *v3, setpix_func setpix)
{
    float recip = 1.0f / (v2->y - v1->y);
    float islop1 = (v2->y - v1->y) * recip;
    float rslop1 = (v2->r - v1->r) * recip;
    float gslop1 = (v2->g - v1->g) * recip;
    float bslop1 = (v2->b - v1->b) * recip;

    recip = 1.0f / (v3->y - v1->y);
    float islop2 = (v3->y - v1->y) * recip;
    float rslop2 = (v3->r - v1->r) * recip;
    float gslop2 = (v3->g - v1->g) * recip;
    float bslop2 = (v3->b - v1->b) * recip;

    float cx1 = v1->y;
    float cx2 = v1->y;
    float cr1 = v1->r;
    float cr2 = v1->r;
    float cg1 = v1->g;
    float cg2 = v1->g;
    float cb1 = v1->b;
    float cb2 = v1->b;

    for (i32 y = (i32)v1->y; y <= (i32)v2->y; y++) {
        fst_draw_line(this, y, cx1, cx2, (float)((u32)cr1 >> 3), (float)((u32)cr2 >> 3), (float)((u32)cg1 >> 3), (float)((u32)cg2 >> 3), (float)((u32)cb1 >> 3), (float)((u32)cb2 >> 3), setpix);
        cx1 += islop1;
        cx2 += islop2;
        cr1 += rslop1;
        cr2 += rslop2;
        cg1 += gslop1;
        cg2 += gslop2;
        cb1 += bslop1;
        cb2 += bslop2;
    }
}

static u16 sample_tex_4bit(struct PS1_GPU_TEXTURE_SAMPLER *ts, i32 u, i32 v)
{
    u32 addr = ts->base_addr + ((v&0xFF)<<11) + ((u&0xFF) >> 1);
    u32 d = ts->VRAM[addr & 0xFFFFF];
    if ((u & 1) == 0) d &= 0x0F;
    else d = (d & 0xF0) >> 4;
    return cR16(ts->VRAM, (ts->clut_addr + (d*2)) & 0xFFFFF);
}

static u16 sample_tex_8bit(struct PS1_GPU_TEXTURE_SAMPLER *ts, i32 u, i32 v)
{
    u32 d = ts->VRAM[(ts->base_addr + ((v&0xFF)<<11) + (u&0x7F)) & 0xFFFFF];
    return cR16(ts->VRAM, (ts->clut_addr + (d*2)) & 0xFFFFF);
}

static u16 sample_tex_15bit(struct PS1_GPU_TEXTURE_SAMPLER *ts, i32 u, i32 v)
{
    u32 addr = ts->base_addr + ((v&0xFF)<<11) + ((u&0x3F)>>1);
    return cR16(ts->VRAM, addr & 0xFFFFF);
}

static void get_texture_sampler(struct PS1_GPU *this, u32 tex_depth, u32 page_x, u32 page_y, u32 clut, struct PS1_GPU_TEXTURE_SAMPLER *ts)
{
    PS1_GPU_texture_sampler_new(ts, page_x, page_y, clut, this);
    switch(tex_depth) {
        case PS1e_t4bit:
            ts->sample = &sample_tex_4bit;
            return;
        case PS1e_t8bit:
            ts->sample = &sample_tex_8bit;
            return;
        case PS1e_t15bit:
            ts->sample = &sample_tex_15bit;
            return;
    }
}

static void draw_flat_shaded_triangle(struct PS1_GPU *this, struct PS1_GPU_VERTEX3 *v1, struct PS1_GPU_VERTEX3 *v2, struct PS1_GPU_VERTEX3 *v3)
{
#ifdef LOG_DRAW_TRIS
    printf("shaded v1:%f,%f v2:%f,%d v3:%f,%f", v1->x, v1->y, 'v2', v2->x, v2->y, 'v3', v3->x, v3->y);
#endif
    // I think? the PS1 uses render top/bottom separately algorithm. so....let's do that
    // first sort points
    struct PS1_GPU_VERTEX3 *a;
    if (v2->y > v3->y) {
        a = v2;
        v2 = v3;
        v3 = a;
    }

    if (v1->y > v2->y) {
        a = v1;
        v1 = v2;
        v2 = a;
    }

    if (v2->y > v3->y) {
        a = v2;
        v2 = v3;
        v3 = a;
    }

    // Trivial case 1
    if (v1->y == v3->y) {
        fst_fill_bottom(this, v1, v2, v3, &default_setpix);
    }
    else if (v1->y == v2->y) { // trivial case 2
        fst_fill_top(this, v1, v2, v3, &default_setpix);
    } // other cases
    else {
        struct PS1_GPU_VERTEX3 *v4 = &this->v4;
        v4->y = v2->y;
        // Now calculate G and B
        float c = ((v2->y - v1->y) / (v3->y - v1->y));

        v4->x = v1->x + c * (v3->x - v1->x);
        v4->r = (float)v1->r + c * (float)(v3->r - v1->r);
        v4->g = (float)v1->g + c * (float)(v3->g - v1->g);
        v4->b = (float)v1->b + c * (float)(v3->b - v1->b);

        // v3->b - v1->b is the gradient between v1 and 3
        // v1->b + sets us up to

        fst_fill_bottom(this, v1, v2, v4, &default_setpix);
        fst_fill_top(this, v2, v4, v3, &default_setpix);
    }
}

static void draw_flat_tex_triangle(struct PS1_GPU *this, struct PS1_GPU_VERTEX3 *v1, struct PS1_GPU_VERTEX3 *v2, struct PS1_GPU_VERTEX3 *v3, u32 palette, u32 tx_page)
{
    u32 tx_x = tx_page & 0x1FF;
    u32 tx_y = (tx_page >> 1) & 1;
    struct PS1_GPU_TEXTURE_SAMPLER tsa;
    get_texture_sampler(this, this->texture_depth, tx_x, tx_y, palette, &tsa);
    
    // first sort points
    struct PS1_GPU_VERTEX3 *a;
    if (v2->y > v3->y) {
        a = v2;
        v2 = v3;
        v3 = a;
    }

    if (v1->y > v2->y) {
        a = v1;
        v1 = v2;
        v2 = a;
    }

    if (v2->y > v3->y) {
        a = v2;
        v2 = v3;
        v3 = a;
    }

    // Trivial case 1
    if (v1->y == v3->y) {
        ftt_fill_bottom(this, v1, v2, v3, &tsa, &default_setpix);
    }
    else if (v1->y == v2->y) { // trivial case 2
        ftt_fill_top(this, v1, v2, v3, &tsa, &default_setpix);
    } // other cases
    else {
        struct PS1_GPU_VERTEX3 *v4 = &this->v4;
        v4->y = v2->y;
        // Now calculate G and B
        float c = ((v2->y - v1->y) / (v3->y - v1->y));

        v4->x = v1->x + c * (v3->x - v1->x);
        v4->u = v1->u + c * (v3->u - v1->u);
        v4->v = v1->v + c * (v3->v - v1->v);

        ftt_fill_bottom(this, v1, v2, v4, &tsa, &default_setpix);
        ftt_fill_top(this, v2, v4, v3, &tsa, &default_setpix);
    }
}

static void cmd2c_quad_opaque_flat_textured(struct PS1_GPU *this) {
    // 0 WRIOW GP0,(0x2C<<24)+(COLOR&0xFFFFFF)        ; Write GP0 Command Word (Color+Command)
    u32 col = this->cmd[0] & 0xFFFFFF;
    // 1 WRIOW GP0,(Y1<<16)+(X1&0xFFFF)               ; Write GP0  Packet Word (Vertex1)
    v3_from_cmd2(&this->t0, this->cmd[1]);
    // 2 WRIOW GP0,(PAL<<16)+((V1&0xFF)<<8)+(U1&0xFF) ; Write GP0  Packet Word (Texcoord1+Palette)
    u32 palette = this->cmd[2] >> 16;
    uv3_from_cmd(&this->t1, this->cmd[2]);
    // 3 WRIOW GP0,(Y2<<16)+(X2&0xFFFF)               ; Write GP0  Packet Word (Vertex2)
    v3_from_cmd2(&this->t2, this->cmd[3]);
    // 4 WRIOW GP0,(TEX<<16)+((V2&0xFF)<<8)+(U2&0xFF) ; Write GP0  Packet Word (Texcoord2+Texpage)
    u32 tx_page = this->cmd[4] >> 16;
    uv3_from_cmd(&this->t2, this->cmd[4]);
    // 5 WRIOW GP0,(Y3<<16)+(X3&0xFFFF)               ; Write GP0  Packet Word (Vertex3)
    v3_from_cmd2(&this->t3, this->cmd[5]);
    // 6 WRIOW GP0,((V3&0xFF)<<8)+(U3&0xFF)           ; Write GP0  Packet Word (Texcoord3)
    uv3_from_cmd(&this->t3, this->cmd[6]);
    // 7 WRIOW GP0,(Y4<<16)+(X4&0xFFFF)               ; Write GP0  Packet Word (Vertex4)
    v3_from_cmd2(&this->t4, this->cmd[7]);
    // 8 WRIOW GP0,((V4&0xFF)<<8)+(U4&0xFF)           ; Write GP0  Packet Word (Texcoord4)
    uv3_from_cmd(&this->t4, this->cmd[8]);

    draw_flat_tex_triangle(this, &this->t1, &this->t2, &this->t3, palette, tx_page);
    draw_flat_tex_triangle(this, &this->t2, &this->t3, &this->t4, palette, tx_page);
}

static inline void c24_from_cmd(struct PS1_GPU_VERTEX3 *this, u32 cmd)
{
    this->r = cmd & 0xFF;
    this->g = (cmd >> 8) & 0xFF;
    this->b = (cmd >> 16) & 0xFF;
}

static void cmd30_tri_shaded_opaque(struct PS1_GPU *this)
{
    // 0 WRIOW GP0,(0x30<<24)+(COLOR1&0xFFFFFF)       ; Write GP0 Command Word (Color1+Command)
    // 1 WRIOW GP0,(Y1<<16)+(X1&0xFFFF)               ; Write GP0  Packet Word (Vertex1)
    // 2 WRIOW GP0,(COLOR2&0xFFFFFF)                  ; Write GP0  Packet Word (Color2)
    // 3 WRIOW GP0,(Y2<<16)+(X2&0xFFFF)               ; Write GP0  Packet Word (Vertex2)
    // 4 WRIOW GP0,(COLOR3&0xFFFFFF)                  ; Write GP0  Packet Word (Color3)
    // 5 WRIOW GP0,(Y3<<16)+(X3&0xFFFF)               ; Write GP0  Packet Word (Vertex3)
    struct PS1_GPU_COLOR_SAMPLER *c1 = &this->color1;
    struct PS1_GPU_COLOR_SAMPLER *c2 = &this->color2;
    struct PS1_GPU_COLOR_SAMPLER *c3 = &this->color3;
    v3_from_cmd2(&this->v1, this->cmd[1]);
    v3_from_cmd2(&this->v2, this->cmd[3]);
    v3_from_cmd2(&this->v3, this->cmd[5]);
    c24_from_cmd(&this->v1, this->cmd[0]);
    c24_from_cmd(&this->v2, this->cmd[2]);
    c24_from_cmd(&this->v3, this->cmd[4]);
#ifdef LOG_GP0
    printf("\ntri_shaded_opaque ");
    print_v(&this->v1);
    print_v(&this->v2);
    print_v(&this->v3);
#endif
    draw_flat_shaded_triangle(this, &this->v1, &this->v2, &this->v3);
}

static void cmd38_quad_shaded_opaque(struct PS1_GPU *this)
{
    // WRIOW GP0,(0x38<<24)+(COLOR1&0xFFFFFF)       ; Write GP0 Command Word (Color1+Command)
    // WRIOW GP0,(Y1<<16)+(X1&0xFFFF)               ; Write GP0  Packet Word (Vertex1)
    // WRIOW GP0,(COLOR2&0xFFFFFF)                  ; Write GP0  Packet Word (Color2)
    // WRIOW GP0,(Y2<<16)+(X2&0xFFFF)               ; Write GP0  Packet Word (Vertex2)
    // WRIOW GP0,(COLOR3&0xFFFFFF)                  ; Write GP0  Packet Word (Color3)
    // WRIOW GP0,(Y3<<16)+(X3&0xFFFF)               ; Write GP0  Packet Word (Vertex3)
    // WRIOW GP0,(COLOR4&0xFFFFFF)                  ; Write GP0  Packet Word (Color4)
    // WRIOW GP0,(Y4<<16)+(X4&0xFFFF)               ; Write GP0  Packet Word (Vertex4)
    c24_from_cmd(&this->t1, this->cmd[0]);
    v3_from_cmd2(&this->t1, this->cmd[1]);
    c24_from_cmd(&this->t2, this->cmd[2]);
    v3_from_cmd2(&this->t2, this->cmd[3]);
    c24_from_cmd(&this->t3, this->cmd[4]);
    v3_from_cmd2(&this->t3, this->cmd[5]);
    c24_from_cmd(&this->t4, this->cmd[6]);
    v3_from_cmd2(&this->t4, this->cmd[7]);

#ifdef LOG_DRAW_QUADS
    printf("\nquad_shaded ");
    draw_vd(&this->t1);
    draw_vd(&this->t2);
    draw_vd(&this->t3);
    draw_vd(&this->t4);
#endif
    
    draw_flat_shaded_triangle(this, &this->t1, &this->t2, &this->t3);
    draw_flat_shaded_triangle(this, &this->t2, &this->t3, &this->t4);
}

static void cmd60_rect_opaque_flat(struct PS1_GPU *this)
{
    u32 color = BGR24to15(this->cmd[0] & 0xFFFFFF);
    u32 xstart = this->cmd[1] & 0xFFFF;
    u32 ystart = this->cmd[1] >> 16;
    u32 xsize = this->cmd[2] & 0xFFFF;
    u32 ysize = this->cmd[2] >> 16;

#ifdef LOG_GP0
    printf("\n60_rect_opaque_flat %d %d %d %d %06x", xstart, ystart, xsize, ysize, this->cmd[0] & 0xFFFFFF);
#endif

    u32 xend = (xstart + xsize);
    xend = xend > 1024 ? 1024 : xend;

    u32 yend = (ystart + ysize);
    yend = yend > 512 ? 512 : yend;

    for (u32 y = ystart; y < yend; y++) {
        for (u32 x = xstart; x < xend; x++) {
            default_setpix(this, y, x, color, 0);
        }
    }
}

static void cmd64_rect_opaque_flat_textured(struct PS1_GPU *this)
{
    // WRIOW GP0,(0x64<<24)+(COLOR&0xFFFFFF)      ; Write GP0 Command Word (Color+Command)

    // WRIOW GP0,(Y<<16)+(X&0xFFFF)               ; Write GP0  Packet Word (Vertex)
    u32 ystart = (this->cmd[1] & 0xFFFF0000) >> 16;
    u32 xstart = ((this->cmd[1] & 0xFFFF) << 16) >> 16;

    // WRIOW GP0,(PAL<<16)+((V&0xFF)<<8)+(U&0xFF) ; Write GP0  Packet Word (Texcoord+Palette)
    u32 clut = (this->cmd[2] >> 16) & 0xFFFF;
    u32 v = (this->cmd[2] >> 8) & 0xFF;
    u32 ustart = this->cmd[2] & 0xFF;

    // WRIOW GP0,(HEIGHT<<16)+(WIDTH&0xFFFF)      ; Write GP0  Packet Word (Width+Height)
    u32 height = (this->cmd[3] >> 16) & 0xFFFF;
    u32 width = this->cmd[3] & 0xFFFF;

    u32 xend = (xstart + width);
    u32 yend = (ystart + height);
    xend = xend > 1024 ? 1024 : xend;
    yend = yend > 512 ? 512 : yend;
#ifdef LOG_GP0
    printf("\nrect_opaque_flat %d %d %d %d", xstart, ystart, width, height);
#endif

    struct PS1_GPU_TEXTURE_SAMPLER ts;
    get_texture_sampler(this, this->texture_depth, this->page_base_x, this->page_base_y, clut, &ts);
    for (u32 y = ystart; y < yend; y++) {
        u32 u = ustart;
        for (u32 x = xstart; x < xend; x++) {
            u16 color = ts.sample(&ts, u, v);
            u32 hbit = color & 0x8000;
            u32 lbit = color & 0x7FFF;
            if ((lbit != 0) || ((lbit == 0) && hbit)) default_setpix(this, y, x, lbit, 0);
            u++;
        }
        v++;
    }
}

static void cmd68_rect_1x1(struct PS1_GPU *this)
{
    u32 y = (this->cmd[1] & 0xFFFF0000) >> 16;
    u32 x = ((this->cmd[1] & 0xFFFF) << 16) >> 16;
    u32 color = BGR24to15(this->cmd[0] & 0xFFFFFF);
    default_setpix(this, y, x, color, 0);
}

static void cmd75_rect_opaque_flat_textured(struct PS1_GPU *this)
{
    // WRIOW GP0,(0x64<<24)+(COLOR&0xFFFFFF)      ; Write GP0 Command Word (Color+Command)
    //let color24 = this->cmd[0] & 0xFFFFFF;

    // WRIOW GP0,(Y<<16)+(X&0xFFFF)               ; Write GP0  Packet Word (Vertex)
    u32 ystart = (this->cmd[1] & 0xFFFF0000) >> 16;
    u32 xstart = ((this->cmd[1] & 0xFFFF) << 16) >> 16;

    // WRIOW GP0,(PAL<<16)+((V&0xFF)<<8)+(U&0xFF) ; Write GP0  Packet Word (Texcoord+Palette)
    u32 clut = (this->cmd[2] >> 16) & 0xFFFF;
    u32 v = (this->cmd[2] >> 8) & 0xFF;
    u32 ustart = this->cmd[2] & 0xFF;

    // WRIOW GP0,(HEIGHT<<16)+(WIDTH&0xFFFF)      ; Write GP0  Packet Word (Width+Height)
    u32 height = (this->cmd[3] >> 16) & 0xFFFF;
    u32 width = this->cmd[3] & 0xFFFF;

    u32 xend = (xstart + width);
    u32 yend = (ystart + height);
    xend = xend > 1024 ? 1024 : xend;
    yend = yend > 512 ? 512 : yend;
#ifdef LOG_GP0
    printf("\nrect_opaque_flat %d %d %d %d", xstart, ystart, width, height);
#endif


    struct PS1_GPU_TEXTURE_SAMPLER ts;
    get_texture_sampler(this, this->texture_depth, this->page_base_x, this->page_base_y, clut, &ts);
    for (u32 y = ystart; y < yend; y++) {
        u32 u = ustart;
        for (u32 x = xstart; x < xend; x++) {
            u16 color = ts.sample(&ts, u, v);
            u32 hbit = color & 0x8000;
            u32 lbit = color & 0x7FFF;
            if ((lbit != 0) || ((lbit == 0) && hbit)) default_setpix(this, y, x, lbit, 0);
            u++;
        }
        v++;
    }
}

static void load_buffer_reset(struct PS1_GPU *this, u32 x, u32 y, u32 width, u32 height)
{
    this->load_buffer.x = x;
    this->load_buffer.y = y;
    this->load_buffer.width = width;
    this->load_buffer.height = height;
    this->load_buffer.line_ptr = (y * 2048) + x;
    this->load_buffer.img_x = this->load_buffer.img_y = 0;
}

static void gp0_image_load_continue(struct PS1_GPU *this, u32 cmd)
{
    this->recv_gp0[this->recv_gp0_len] = cmd>>0;
    this->recv_gp0_len++;
    if (this->recv_gp0_len >= (1024*1024)) {
        printf("\nWARNING GP0 TRANSFER OVERFLOW!");
        this->recv_gp0_len--;
    }
    // Put in 2 16-bit pixels
    //console.log('TRANSFERRING!', this->gp0_transfer_remaining);
    for (u32 i = 0; i < 2; i++) {
        u32 px = cmd & 0xFFFF;
        cmd >>= 16;
        u32 y = this->load_buffer.y+this->load_buffer.img_y;
        u32 x = this->load_buffer.x+this->load_buffer.img_x;
        u32 addr = (2048*y)+(x*2) & 0xFFFFF;
        cW16(this->VRAM, addr, px);
        this->load_buffer.img_x++;
        if ((x+1) >= (this->load_buffer.width+this->load_buffer.x)) {
            this->load_buffer.img_x = 0;
            this->load_buffer.img_y++;
        }
    }
    this->gp0_transfer_remaining--;
    if (this->gp0_transfer_remaining == 0) {
        //console.log('TRANSFER COMPLETE!');
        this->current_ins = NULL;
        this->handle_gp0 = &gp0_cmd;
        ready_cmd(this);
        unready_recv_dma(this);
    }
}

static void gp0_image_load_start(struct PS1_GPU *this)
{
    unready_cmd(this);
    ready_recv_dma(this);
    // Top-left corner in VRAM
    u32 x = this->cmd[1] & 1023;
    u32 y = (this->cmd[1] >> 16) & 511;

    // Resolution
    u32 width = this->cmd[2] & 0xFFFF;
    u32 height = (this->cmd[2] >> 16) & 0xFFFF;

    // Get imgsize, round it
    u32 imgsize = ((width * height) + 1) & 0xFFFFFFFE;

    this->gp0_transfer_remaining = imgsize/2;
#ifdef LOG_GP0
    printf("\nTRANSFER IMGSIZE %d X:%d Y:%d WIDTH:%d HEIGHT:%d CMD:%08x', imgsize, x, y, width, height, this->cmd[0]);
#endif
    if (this->gp0_transfer_remaining > 0) {
        load_buffer_reset(this, x, y, width, height);
        this->handle_gp0 = &gp0_image_load_continue;
    } else {
        printf("\nBad size image load: 0?");
        this->current_ins = NULL;
    }
}

static void gp0_cmd_unhandled(struct PS1_GPU *this)
{

}

static void GPUSTAT_update(struct PS1_GPU *this)
{
    u32 o = this->page_base_x;
    o |= (this->page_base_y) << 4;
    o |= (this->semi_transparency) << 5;
    o |= (this->texture_depth) << 7;
    o |= (this->dithering) << 9;
    o |= (this->draw_to_display) << 10;
    o |= (this->force_set_mask_bit) << 11;
    o |= (this->preserve_masked_pixels) << 12;
    o |= (this->field) << 14;
    o |= (this->texture_disable) << 15;
    o |= this->hres << 16;
    o |= this->vres << 19;
    o |= this->vmode << 20;
    o |= this->display_depth << 21;
    o |= this->interlaced << 22;
    o |= this->display_disabled << 23;
    // interrupt is set at other end where they are scheduled

    o |= this->dma_direction << 29;

    u32 dmar;
    switch(this->dma_direction) {
        // 0 if FIFO full, 1 otherwise
        case PS1e_dma_off:
            dmar = 0;
            break;
        case PS1e_dma_fifo:
            dmar = 0; // set during read on other side
            break;
        case PS1e_dma_cpu_to_gp0:
            dmar = (o >> 28) & 1;
            break;
        case PS1e_dma_vram_to_cpu:
            dmar = (o >> 27) & 1;
            break;
    }

    o |= (dmar << 25);

    // Preserve GPU ready or not bits
    this->io.GPUSTAT = o | (this->io.GPUSTAT & 0x1C000000);
}

void PS1_GPU_write_gp0(struct PS1_GPU *this, u32 cmd) {
    this->handle_gp0(this, cmd);
}


static void gp0_cmd(struct PS1_GPU *this, u32 cmd) {
    this->gp0_buffer[this->recv_gp0_len] = cmd;
    this->recv_gp0_len++;

    // Check if we have an instruction..
    if (this->current_ins) {
        this->cmd[this->cmd_arg_index++] = cmd;
        if (this->cmd_arg_index == this->cmd_arg_num) {
            this->current_ins(this);
            this->current_ins = NULL;
        }
    } else {
        this->cmd[0] = cmd;
        this->cmd_arg_index = 1;
        this->ins_special = 0;
        this->cmd_arg_num = 1;
        u32 cmdr = cmd >> 24;
        switch(cmdr) {
            case 0: // NOP
                if (cmd != 0) printf("\nINTERPRETED AS NOP:%08x", cmd);
                break;
            case 0x01: // Clear cache (not implemented)
                break;
            case 0x02: // Quick Rectangle
                //console.log('Quick rectangle!');
                this->current_ins = &cmd02_quick_rect;
                this->cmd_arg_num = 3;
                break;
                //case 0x21: // ??
                //    console.log('NOT IMPLEMENT 0x21');
                //    break;
            case 0x28: // flat-shaded rectangle
                this->current_ins = &cmd28_draw_flat4untex;
                this->cmd_arg_num = 5;
                break;
            case 0x2C: // polygon, 4 points, textured, flat
                this->current_ins = &cmd2c_quad_opaque_flat_textured;
                this->cmd_arg_num = 9;
                break;
            case 0x30: // opaque shaded trinalge
                this->current_ins = &cmd30_tri_shaded_opaque;
                this->cmd_arg_num = 6;
                break;
            case 0x38: // polygon, 4 points, gouraud-shaded
                this->current_ins = &cmd38_quad_shaded_opaque;
                this->cmd_arg_num = 8;
                break;
            case 0x60: // Rectangle, variable size, opaque
                this->current_ins = &cmd60_rect_opaque_flat;
                this->cmd_arg_num = 3;
                break;
            case 0x64: // Rectangle, variable size, textured, flat, opaque
                this->current_ins = &cmd64_rect_opaque_flat_textured;
                this->cmd_arg_num = 4;
                break;
            case 0x6A: // Solid 1x1 rect
            case 0x68:
                this->current_ins = &cmd68_rect_1x1;
                this->cmd_arg_num = 2;
                break;
            case 0x75: // Rectangle, 8x8, opaque, textured
                this->current_ins = &cmd75_rect_opaque_flat_textured;
                this->cmd_arg_num = 3;
                break;
            case 0xBC:
            case 0xB8:
            case 0xA0: // Image stream to GPU
                this->current_ins = &gp0_image_load_start;
                this->cmd_arg_num = 3;
                break;
            case 0xC0:
                printf("\nWARNING unhandled GP0 command 0xC0");
                this->cmd_arg_num = 2;
                this->current_ins = &gp0_cmd_unhandled;
                break;
            case 0xE1: // GP0 Draw Mode
#ifdef DBG_GP0
                console.log('GP0 E1 set draw mode');
#endif
                printf("\nSET DRAW MODE %08x", cmd);
                this->page_base_x = cmd & 15;
                this->page_base_y = (cmd >> 4) & 1;
                switch ((cmd >> 7) & 3) {
                    case 0:
                        this->texture_depth = PS1e_t4bit;
                        break;
                    case 1:
                        this->texture_depth = PS1e_t8bit;
                        break;
                    case 2:
                        this->texture_depth = PS1e_t15bit;
                        break;
                    case 3:
                        printf("\nUNHANDLED TEXTAR DEPTH 3!!!");
                        break;
                }
                this->dithering = (cmd >> 9) & 1;
                this->draw_to_display = (cmd >> 10) & 1;
                this->texture_disable = (cmd >> 1) & 1;
                GPUSTAT_update(this);
                this->rect.texture_x_flip = (cmd >> 12) & 1;
                this->rect.texture_y_flip = (cmd >> 12) & 1;
                break;
            case 0xE2: // Texture window
#ifdef DBG_GP0
                printf("\nGP0 E2 set draw mode");
#endif
                this->tx_win_x_mask = cmd & 0x1F;
                this->tx_win_y_mask = (cmd >> 5) & 0x1F;
                this->tx_win_x_offset = (cmd >> 10) & 0x1F;
                this->tx_win_y_offset = (cmd >> 15) & 0x1F;
                break;
            case 0xE3: // Set draw area upper-left corner
#ifdef DBG_GP0
                printf("\nGP0 E3 set draw area UL corner");
#endif
                this->draw_area_top = (cmd >> 10) & 0x3FF;
                this->draw_area_left = cmd & 0x3FF;
                break;
            case 0xE4: // Draw area lower-right corner
#ifdef DBG_GP0
                printf("\nGP0 E4 set draw area LR corner");
#endif
                this->draw_area_bottom = (cmd >> 10) & 0x3FF;
                this->draw_area_right = cmd & 0x3FF;
                break;
            case 0xE5: // Drawing offset
#ifdef DBG_GP0
                printf("\nGP0 E5 set drawing offset");
#endif
                this->draw_x_offset = mksigned11(cmd & 0x7FF);
                this->draw_y_offset = mksigned11((cmd >> 11) & 0x7FF);
                break;
            case 0xE6: // Set Mask Bit setting
#ifdef DBG_GP0
                printf("\nGP0 E6 set bit mask");
#endif
                this->force_set_mask_bit = cmd & 1;
                this->preserve_masked_pixels = (cmd >> 1) & 1;
                break;
            default:
                printf("Unknown GP0 command %08x", cmd);
                break;
        }
    }
}

void PS1_GPU_write_gp1(struct PS1_GPU *this, u32 cmd)
{
    switch(cmd >> 24) {
        case 0:
            printf("\nGP1 soft reset");
            // Soft reset
            unready_cmd(this);
            unready_recv_dma(this);
            unready_vram_to_CPU(this);
            this->page_base_x = 0;
            this->page_base_y = 0;
            this->semi_transparency = 0;
            this->texture_depth = PS1e_t4bit;
            this->tx_win_x_mask = this->tx_win_y_mask = 0;
            this->tx_win_x_offset = this->tx_win_y_offset = 0;
            this->dithering = 0;
            this->draw_to_display = 0;
            this->texture_disable = 0;
            this->rect.texture_x_flip = this->rect.texture_y_flip = 0;
            this->draw_area_bottom = this->draw_area_right = this->draw_area_left = this->draw_area_top = 0;
            this->draw_x_offset = this->draw_y_offset = 0;
            this->force_set_mask_bit = 0;
            this->preserve_masked_pixels = 0;
            this->dma_direction = PS1e_dma_off;
            this->display_disabled = 1;
            this->display_vram_x_start = this->display_vram_y_start = 0;
            this->hres = 0;
            this->vres = PS1e_y240lines;
            this->vmode = PS1e_ntsc;
            this->interlaced = 1;
            this->display_horiz_start = 0x200;
            this->display_horiz_end = 0xC00;
            this->display_line_start = 0x10;
            this->display_line_end = 0x100;
            this->display_depth = PS1e_d15bits;
            //this->clear_FIFO();
            GPUSTAT_update(this);
            ready_cmd(this);
            ready_recv_dma(this);
            ready_vram_to_CPU(this);

            // TODO: remember to flush GPU texture cache
            break;
        case 0x01: // reset CMD FIFO
            //console.log('RESET CMD FIFO NOT IMPLEMENT');
            break;
        case 0x02:
            printf("\nGPU IRQ RESET; WHAT...");
            break;
        case 0x03: // DISPLAY DISABLE
            //TODO: do this
            break;
        case 0x04: // DMA direction
            switch(cmd & 3) {
                case 0:
                    this->dma_direction = PS1e_dma_off;
                    break;
                case 1:
                    this->dma_direction = PS1e_dma_fifo;
                    break;
                case 2:
                    this->dma_direction = PS1e_dma_cpu_to_gp0;
                    break;
                case 3:
                    this->dma_direction = PS1e_dma_vram_to_cpu;
                    break;
            }
            GPUSTAT_update(this);
            break;
        case 0x05: // VRAM start
            //console.log('GP1 VRAM start');
            this->display_vram_x_start = cmd & 0x3FE;
            this->display_vram_y_start = (cmd >> 10) & 0x1FF;
            break;
        case 0x06: // Display horizontal range, in output coordinates
            this->display_horiz_start = cmd & 0xFFF;
            this->display_horiz_end = (cmd >> 12) & 0xFFF;
            break;
        case 0x07: // Display vertical range, in output coordinates
            this->display_line_start = cmd & 0x3FF;
            this->display_line_end = (cmd >> 10) & 0x3FF;
            break;
        case 0x08: {// Display mode
            //console.log('GP1 display mode');
            u32 hr1 = cmd & 3;
            u32 hr2 = ((cmd >> 6) & 1);

            this->hres = (hr2 & 1) | ((hr1 & 3) << 1);
            this->vres = (cmd & 4) ? PS1e_y480lines : PS1e_y240lines;
            this->vmode = (cmd & 8) ? PS1e_pal : PS1e_ntsc;
            this->display_depth = (cmd & 16) ? PS1e_d15bits : PS1e_d24bits;
            this->interlaced = (cmd >> 5) & 1;
            if ((cmd & 0x80) != 0) {
                printf("\nUnsupported display mode!");
            }
            break; }
        default:
            printf("\nUnknown GP1 command %08x", cmd);
            break;
    }
}

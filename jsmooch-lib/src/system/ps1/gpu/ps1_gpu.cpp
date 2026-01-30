//
// Created by . on 2/15/25.
//
#include <cstdlib>
#include <cstring>
#include <math.h>

#include "pixel_helpers.h"
#include "../ps1_bus.h"
#include "../timers/ps1_timers.h"
#include "ps1_gpu.h"
#include "rasterize_line.h"

#include "helpers/multisize_memaccess.c"

//#define LOG_GP0
//#define DBG_GP0

static void gp0_cmd(PS1_GPU *this, u32 cmd);

static inline void xy_from_cmd(RT_POINT2D *this, u32 cmd){
    this->x = (i32)(cmd & 0xFFFF);
    this->y = (i32)(cmd >> 16);
}

static inline void color24_from_cmd(RT_POINT2D *this, u32 cmd)
{
    this->r = cmd & 0xFF;
    this->g = (cmd >> 8) & 0xFF;
    this->b = (cmd >> 16) & 0xFF;
}

static inline void uv_from_cmd(RT_POINT2D *this, u32 cmd)
{
    this->u = cmd & 0xFF;
    this->v = (cmd >> 8) & 0xFF;
}

void PS1_GPU_texture_sampler_new(PS1_GPU_TEXTURE_SAMPLER *this, u32 page_x, u32 page_y, u32 clut_addr, PS1_GPU *ctrl)
{
    this->page_x = (page_x & 0x0F) << 6; // * 64
    this->page_y = (page_y & 1) * 256;
    this->base_addr = (this->page_y * 2048) + (this->page_x * 2);
    this->clut_addr = clut_addr;
    this->VRAM = ctrl->VRAM;
}

#define R_GPUSTAT 0
#define R_GPUPLAYING 1
#define R_GPUQUIT 2
#define R_GPUGP1 3
#define R_GPUREAD 4
#define R_LASTUSED 23

void PS1_GPU_init(PS1 *this)
{
    memset(this->gpu.VRAM, 0, 1024*1024);
    this->gpu.scheduler = &this->scheduler;

    this->gpu.display_horiz_start = 0x200;
    this->gpu.display_horiz_end = 0xC00;
    this->gpu.display_line_start = 0x10;
    this->gpu.display_line_end = 0x100;
    this->gpu.handle_gp0 = &gp0_cmd;
}

static void ins_null(PS1_GPU *this)
{

}

static inline void unready_cmd(PS1_GPU *this)
{
    //static u32 e = 0;
    //e++;
    //printf("\nUNREADY CMD %d", e);
    this->io.GPUSTAT &= 0xFBFFFFFF;
}

static inline void unready_recv_dma(PS1_GPU *this)
{
    //dbg_printf("\nUNREADY DMA");
    this->io.GPUSTAT &= 0xEFFFFFFF;
}

static inline void unready_vram_to_CPU(PS1_GPU *this)
{
    //printf("\nUNREADY VRAM_TO_CPU");
    this->io.GPUSTAT &= 0xF7FFFFFF;
}

static inline void unready_all(PS1_GPU *this)
{
    unready_cmd(this);
    unready_recv_dma(this);
    unready_vram_to_CPU(this);
}

static inline void ready_cmd(PS1_GPU *this)
{
    //printf("\nREADY CMD");
    this->io.GPUSTAT |= 0x4000000;
}

static inline void ready_recv_dma(PS1_GPU *this)
{
    //dbg_printf("\nREADY DMA");
    this->io.GPUSTAT |= 0x10000000;
}

static inline void ready_vram_to_CPU(PS1_GPU *this)
{
    //printf("\nREADY VRAM_TO_CPU");
    this->io.GPUSTAT |= 0x8000000;
}

static inline void ready_all(PS1_GPU *this)
{
    ready_cmd(this);
    ready_recv_dma(this);
    ready_vram_to_CPU(this);
}

static void cmd02_quick_rect(PS1_GPU *this)
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

static void cmd28_draw_flat4untex(PS1_GPU *this)
{
    // Flat 4-vertex untextured poly
    xy_from_cmd(&this->v0, this->cmd[1]);
    xy_from_cmd(&this->v1, this->cmd[2]);
    xy_from_cmd(&this->v2, this->cmd[3]);
    xy_from_cmd(&this->v3, this->cmd[4]);

    u32 color = BGR24to15(this->cmd[0] & 0xFFFFFF);
#ifdef LOG_DRAW_QUADS
    printf("\nflat4untex %d %d %d %d %d %d %06x", x0, y0, x1, y1, x2, y2, color);
#endif
    RT_draw_flat_triangle(this, &this->v0, &this->v1, &this->v2, color);
    RT_draw_flat_triangle(this, &this->v1, &this->v2, &this->v3, color);
}


static u16 sample_tex_4bit(PS1_GPU_TEXTURE_SAMPLER *ts, i32 u, i32 v)
{
    u32 addr = ts->base_addr;
    addr += ((v&0xFF)*2048); // 2048 bytes per line
    addr += ((u&0xFF) >> 1); // half of x, since we are 4-bit
    u32 d = ts->VRAM[addr & 0xFFFFF];
    //ts->VRAM[addr & 0xFFFFF] = 0xFF;
    if ((u & 1) == 0) d &= 0x0F;
    else d = (d & 0xF0) >> 4;
    addr = (ts->clut_addr + (d*2)) & 0xFFFFF;
    //cW16(ts->VRAM, addr, 0xFFF0);
    u16 r = cR16(ts->VRAM, addr);
    return r;
}

static u16 sample_tex_8bit(PS1_GPU_TEXTURE_SAMPLER *ts, i32 u, i32 v)
{
    u32 d = ts->VRAM[(ts->base_addr + ((v&0xFF)<<11) + (u&0x7F)) & 0xFFFFF];
    return cR16(ts->VRAM, (ts->clut_addr + (d*2)) & 0xFFFFF);
}

static u16 sample_tex_15bit(PS1_GPU_TEXTURE_SAMPLER *ts, i32 u, i32 v)
{
    u32 addr = ts->base_addr + ((v&0xFF)<<11) + ((u&0x7F)<<1);
    return cR16(ts->VRAM, addr & 0xFFFFF);
}

static void get_texture_sampler_from_texpage_and_palette(PS1_GPU *this, u32 texpage, u32 palette, PS1_GPU_TEXTURE_SAMPLER *ts)
{
    u32 clx = (palette & 0x3F) << 4;
    u32 cly = (palette >> 6) & 0x1FF;

    u32 clut_addr = (cly * 2048) + (clx * 2);

    u32 tx_x = texpage & 15;
    u32 tx_y = (texpage >> 4) & 1;
    PS1_GPU_texture_sampler_new(ts, tx_x, tx_y, clut_addr, this);
    ts->semi_mode = (texpage >> 5) & 3;
    u32 tdepth = (texpage >> 7) & 3;
    switch(tdepth) {
        case 0:
            ts->sample = &sample_tex_4bit;
            break;
        case 1:
            ts->sample = &sample_tex_8bit;
            break;
        case 2:
            ts->sample = &sample_tex_15bit;
            break;
        case 3:
            NOGOHERE;
    }
}

static void cmd20_tri_flat(PS1_GPU *this)
{
    xy_from_cmd(&this->v0, this->cmd[1]);
    xy_from_cmd(&this->v1, this->cmd[2]);
    xy_from_cmd(&this->v2, this->cmd[3]);
    u32 color = BGR24to15(this->cmd[0] & 0xFFFFFF);
#ifdef LOG_GP0
    printf("\nGP0_20 flat tri %f,%f %f,%f %f,%f color:%04x", x0, y0, x1, y1, x2, y2, color);
#endif
    RT_draw_flat_triangle(this, &this->v0, &this->v1, &this->v2, color);
}

static void cmd22_tri_flat_semi_transparent(PS1_GPU *this)
{
    xy_from_cmd(&this->v0, this->cmd[1]);
    xy_from_cmd(&this->v1, this->cmd[2]);
    xy_from_cmd(&this->v2, this->cmd[3]);
    color24_from_cmd(&this->v0, this->cmd[0]);
    u32 color = BGR24to15(this->cmd[0] & 0xFFFFFF);
#ifdef LOG_GP0
    printf("\nGP0_20 flat tri %f,%f %f,%f %f,%f color:%04x", x0, y0, x1, y1, x2, y2, color);
#endif
    RT_draw_flat_triangle_semi(this, &this->v0, &this->v1, &this->v2, this->v0.r, this->v0.g, this->v0.b);
}

static void cmd24_tri_raw_modulated(PS1_GPU *this)
{
    /*
  0 WRIOW GP0,(0x24<<24)+(COLOR&0xFFFFFF)        ; Write GP0 Command Word (Color+Command)
  1 WRIOW GP0,(Y1<<16)+(X1&0xFFFF)               ; Write GP0  Packet Word (Vertex1)
  2 WRIOW GP0,(PAL<<16)+((V1&0xFF)<<8)+(U1&0xFF) ; Write GP0  Packet Word (Texcoord1+Palette)
  3 WRIOW GP0,(Y2<<16)+(X2&0xFFFF)               ; Write GP0  Packet Word (Vertex2)
  4 WRIOW GP0,(TEX<<16)+((V2&0xFF)<<8)+(U2&0xFF) ; Write GP0  Packet Word (Texcoord2+Texpage)
  5 WRIOW GP0,(Y3<<16)+(X3&0xFFFF)               ; Write GP0  Packet Word (Vertex3)
  6 WRIOW GP0,((V3&0xFF)<<8)+(U3&0xFF)           ; Write GP0  Packet Word (Texcoord3)     */
    xy_from_cmd(&this->v0, this->cmd[1]);
    xy_from_cmd(&this->v1, this->cmd[3]);
    xy_from_cmd(&this->v2, this->cmd[5]);
    uv_from_cmd(&this->v0, this->cmd[2]);
    uv_from_cmd(&this->v1, this->cmd[4]);
    uv_from_cmd(&this->v2, this->cmd[6]);
    u16 texpage = this->cmd[4] >> 16;
    u16 palette = this->cmd[2] >> 16;
    u32 color = this->cmd[0] & 0xFFFFFF;
    struct PS1_GPU_TEXTURE_SAMPLER ts;
    get_texture_sampler_from_texpage_and_palette(this, texpage, palette, &ts);
    RT_draw_flat_tex_triangle_modulated(this, &this->v0, &this->v1, &this->v2, color, &ts);
}

static void cmd25_tri_raw(PS1_GPU *this)
{
    /*
  0 WRIOW GP0,(0x25<<24)                         ; Write GP0 Command Word (Command)
  1 WRIOW GP0,(Y1<<16)+(X1&0xFFFF)               ; Write GP0  Packet Word (Vertex1)
  2 WRIOW GP0,(PAL<<16)+((V1&0xFF)<<8)+(U1&0xFF) ; Write GP0  Packet Word (Texcoord1+Palette)
  3 WRIOW GP0,(Y2<<16)+(X2&0xFFFF)               ; Write GP0  Packet Word (Vertex2)
  4 WRIOW GP0,(TEX<<16)+((V2&0xFF)<<8)+(U2&0xFF) ; Write GP0  Packet Word (Texcoord2+Texpage)
  5 WRIOW GP0,(Y3<<16)+(X3&0xFFFF)               ; Write GP0  Packet Word (Vertex3)
  6 WRIOW GP0,((V3&0xFF)<<8)+(U3&0xFF)           ; Write GP0  Packet Word (Texcoord3)
     */
    xy_from_cmd(&this->v0, this->cmd[1]);
    xy_from_cmd(&this->v1, this->cmd[3]);
    xy_from_cmd(&this->v2, this->cmd[5]);
    uv_from_cmd(&this->v0, this->cmd[2]);
    uv_from_cmd(&this->v1, this->cmd[4]);
    uv_from_cmd(&this->v2, this->cmd[6]);
    u16 texpage = this->cmd[4] >> 16;
    u16 palette = this->cmd[2] >> 16;
    struct PS1_GPU_TEXTURE_SAMPLER ts;
    get_texture_sampler_from_texpage_and_palette(this, texpage, palette, &ts);
    RT_draw_flat_tex_triangle(this, &this->v0, &this->v1, &this->v2, &ts);
}

static void cmd26_tri_modulated_semi_transparent(PS1_GPU *this)
{
    /*
  0 WRIOW GP0,(0x26<<24)+(COLOR&0xFFFFFF)        ; Write GP0 Command Word (Color+Command)
  1 WRIOW GP0,(Y1<<16)+(X1&0xFFFF)               ; Write GP0  Packet Word (Vertex1)
  2 WRIOW GP0,(PAL<<16)+((V1&0xFF)<<8)+(U1&0xFF) ; Write GP0  Packet Word (Texcoord1+Palette)
  3 WRIOW GP0,(Y2<<16)+(X2&0xFFFF)               ; Write GP0  Packet Word (Vertex2)
  4 WRIOW GP0,(TEX<<16)+((V2&0xFF)<<8)+(U2&0xFF) ; Write GP0  Packet Word (Texcoord2+Texpage)
  5 WRIOW GP0,(Y3<<16)+(X3&0xFFFF)               ; Write GP0  Packet Word (Vertex3)
  6 WRIOW GP0,((V3&0xFF)<<8)+(U3&0xFF)           ; Write GP0  Packet Word (Texcoord3)
     */
    xy_from_cmd(&this->v0, this->cmd[1]);
    xy_from_cmd(&this->v1, this->cmd[3]);
    xy_from_cmd(&this->v2, this->cmd[5]);
    uv_from_cmd(&this->v0, this->cmd[2]);
    uv_from_cmd(&this->v1, this->cmd[4]);
    uv_from_cmd(&this->v2, this->cmd[6]);
    u16 texpage = this->cmd[4] >> 16;
    u16 palette = this->cmd[2] >> 16;
    u32 color = this->cmd[0] & 0xFFFFFF;
    struct PS1_GPU_TEXTURE_SAMPLER ts;
    get_texture_sampler_from_texpage_and_palette(this, texpage, palette, &ts);
    RT_draw_flat_tex_triangle_modulated_semi(this, &this->v0, &this->v1, &this->v2, color, &ts);
}

static void cmd27_tri_raw_semi_transparent(PS1_GPU *this)
{
    /*
  0 WRIOW GP0,(0x27<<24)                         ; Write GP0 Command Word (Command)
  1 WRIOW GP0,(Y1<<16)+(X1&0xFFFF)               ; Write GP0  Packet Word (Vertex1)
  2 WRIOW GP0,(PAL<<16)+((V1&0xFF)<<8)+(U1&0xFF) ; Write GP0  Packet Word (Texcoord1+Palette)
  3 WRIOW GP0,(Y2<<16)+(X2&0xFFFF)               ; Write GP0  Packet Word (Vertex2)
  4 WRIOW GP0,(TEX<<16)+((V2&0xFF)<<8)+(U2&0xFF) ; Write GP0  Packet Word (Texcoord2+Texpage)
  5 WRIOW GP0,(Y3<<16)+(X3&0xFFFF)               ; Write GP0  Packet Word (Vertex3)
  6 WRIOW GP0,((V3&0xFF)<<8)+(U3&0xFF)           ; Write GP0  Packet Word (Texcoord3)     */
    xy_from_cmd(&this->v0, this->cmd[1]);
    xy_from_cmd(&this->v1, this->cmd[3]);
    xy_from_cmd(&this->v2, this->cmd[5]);
    uv_from_cmd(&this->v0, this->cmd[2]);
    uv_from_cmd(&this->v1, this->cmd[4]);
    uv_from_cmd(&this->v2, this->cmd[6]);
    u16 texpage = this->cmd[4] >> 16;
    u16 palette = this->cmd[2] >> 16;
    struct PS1_GPU_TEXTURE_SAMPLER ts;
    get_texture_sampler_from_texpage_and_palette(this, texpage, palette, &ts);
    RT_draw_flat_tex_triangle_semi(this, &this->v0, &this->v1, &this->v2, &ts);
}


static void cmd2a_quad_flat_semi_transparent(PS1_GPU *this)
{
    xy_from_cmd(&this->v0, this->cmd[1]);
    xy_from_cmd(&this->v1, this->cmd[2]);
    xy_from_cmd(&this->v2, this->cmd[3]);
    xy_from_cmd(&this->v3, this->cmd[4]);
    color24_from_cmd(&this->v0, this->cmd[0]);

#ifdef LOG_GP0
    printf("\ncmd2a quad flat semi v0:%d,%d v1:%d,%d v2:%d,%d v3:%d,%d color:%04x", v0.x, v0.y, v1.x, v1.y, v2.x, v2.y, v3.x, v3.y, color);
#endif
    RT_draw_flat_triangle_semi(this, &this->v0, &this->v1, &this->v2, this->v0.r, this->v0.g, this->v0.b);
    RT_draw_flat_triangle_semi(this, &this->v1, &this->v2, &this->v3, this->v0.r, this->v0.g, this->v0.b);
}


static void cmd2c_quad_opaque_flat_textured_modulated(PS1_GPU *this) {
    // 0 WRIOW GP0,(0x2C<<24)+(COLOR&0xFFFFFF)        ; Write GP0 Command Word (Color+Command)
    // 1 WRIOW GP0,(Y1<<16)+(X1&0xFFFF)               ; Write GP0  Packet Word (Vertex1)
    // 2 WRIOW GP0,(PAL<<16)+((V1&0xFF)<<8)+(U1&0xFF) ; Write GP0  Packet Word (Texcoord1+Palette)
    // 3 WRIOW GP0,(Y2<<16)+(X2&0xFFFF)               ; Write GP0  Packet Word (Vertex2)
    // 4 WRIOW GP0,(TEX<<16)+((V2&0xFF)<<8)+(U2&0xFF) ; Write GP0  Packet Word (Texcoord2+Texpage)
    // 5 WRIOW GP0,(Y3<<16)+(X3&0xFFFF)               ; Write GP0  Packet Word (Vertex3)
    // 6 WRIOW GP0,((V3&0xFF)<<8)+(U3&0xFF)           ; Write GP0  Packet Word (Texcoord3)
    // 7 WRIOW GP0,(Y4<<16)+(X4&0xFFFF)               ; Write GP0  Packet Word (Vertex4)
    // 8 WRIOW GP0,((V4&0xFF)<<8)+(U4&0xFF)           ; Write GP0  Packet Word (Texcoord4)
    xy_from_cmd(&this->t1, this->cmd[1]);
    xy_from_cmd(&this->t2, this->cmd[3]);
    xy_from_cmd(&this->t3, this->cmd[5]);
    xy_from_cmd(&this->t4, this->cmd[7]);
    uv_from_cmd(&this->t1, this->cmd[2]);
    uv_from_cmd(&this->t2, this->cmd[4]);
    uv_from_cmd(&this->t3, this->cmd[6]);
    uv_from_cmd(&this->t4, this->cmd[8]);

    u32 palette = this->cmd[2] >> 16;
    u32 tx_page = this->cmd[4] >> 16;
    u32 col = this->cmd[0] & 0xFFFFFF;

    struct PS1_GPU_TEXTURE_SAMPLER ts;
    get_texture_sampler_from_texpage_and_palette(this, tx_page, palette, &ts);

    RT_draw_flat_tex_triangle_modulated(this, &this->t1, &this->t2, &this->t3, col, &ts);
    RT_draw_flat_tex_triangle_modulated(this, &this->t2, &this->t3, &this->t4, col, &ts);
}

static void cmd2d_quad_opaque_flat_textured(PS1_GPU *this) {
    /*
WRIOW GP0,(0x2D<<24)                         ; Write GP0 Command Word (Command)
  WRIOW GP0,(Y1<<16)+(X1&0xFFFF)               ; Write GP0  Packet Word (Vertex1)
  WRIOW GP0,(PAL<<16)+((V1&0xFF)<<8)+(U1&0xFF) ; Write GP0  Packet Word (Texcoord1+Palette)
  WRIOW GP0,(Y2<<16)+(X2&0xFFFF)               ; Write GP0  Packet Word (Vertex2)
  WRIOW GP0,(TEX<<16)+((V2&0xFF)<<8)+(U2&0xFF) ; Write GP0  Packet Word (Texcoord2+Texpage)
  WRIOW GP0,(Y3<<16)+(X3&0xFFFF)               ; Write GP0  Packet Word (Vertex3)
  WRIOW GP0,((V3&0xFF)<<8)+(U3&0xFF)           ; Write GP0  Packet Word (Texcoord3)
  WRIOW GP0,(Y4<<16)+(X4&0xFFFF)               ; Write GP0  Packet Word (Vertex4)
  WRIOW GP0,((V4&0xFF)<<8)+(U4&0xFF)           ; Write GP0  Packet Word (Texcoord4)
     */
    xy_from_cmd(&this->t1, this->cmd[1]);
    xy_from_cmd(&this->t2, this->cmd[3]);
    xy_from_cmd(&this->t3, this->cmd[5]);
    xy_from_cmd(&this->t4, this->cmd[7]);

    uv_from_cmd(&this->t1, this->cmd[2]);
    uv_from_cmd(&this->t2, this->cmd[4]);
    uv_from_cmd(&this->t3, this->cmd[6]);
    uv_from_cmd(&this->t4, this->cmd[8]);

    u32 palette = this->cmd[2] >> 16;
    u32 tx_page = this->cmd[4] >> 16;

    struct PS1_GPU_TEXTURE_SAMPLER ts;
    get_texture_sampler_from_texpage_and_palette(this, tx_page, palette, &ts);

    RT_draw_flat_tex_triangle(this, &this->t1, &this->t2, &this->t3, &ts);
    RT_draw_flat_tex_triangle(this, &this->t2, &this->t3, &this->t4, &ts);
}

static void cmd2e_quad_flat_textured_modulated(PS1_GPU *this) {
/*
  WRIOW GP0,(0x2E<<24)+(COLOR&0xFFFFFF)        ; Write GP0 Command Word (Color+Command)
  WRIOW GP0,(Y1<<16)+(X1&0xFFFF)               ; Write GP0  Packet Word (Vertex1)
  WRIOW GP0,(PAL<<16)+((V1&0xFF)<<8)+(U1&0xFF) ; Write GP0  Packet Word (Texcoord1+Palette)
  WRIOW GP0,(Y2<<16)+(X2&0xFFFF)               ; Write GP0  Packet Word (Vertex2)
  WRIOW GP0,(TEX<<16)+((V2&0xFF)<<8)+(U2&0xFF) ; Write GP0  Packet Word (Texcoord2+Texpage)
  WRIOW GP0,(Y3<<16)+(X3&0xFFFF)               ; Write GP0  Packet Word (Vertex3)
  WRIOW GP0,((V3&0xFF)<<8)+(U3&0xFF)           ; Write GP0  Packet Word (Texcoord3)
  WRIOW GP0,(Y4<<16)+(X4&0xFFFF)               ; Write GP0  Packet Word (Vertex4)
  WRIOW GP0,((V4&0xFF)<<8)+(U4&0xFF)           ; Write GP0  Packet Word (Texcoord4)
 */
    xy_from_cmd(&this->t1, this->cmd[1]);
    xy_from_cmd(&this->t2, this->cmd[3]);
    xy_from_cmd(&this->t3, this->cmd[5]);
    xy_from_cmd(&this->t4, this->cmd[7]);
    uv_from_cmd(&this->t1, this->cmd[2]);
    uv_from_cmd(&this->t2, this->cmd[4]);
    uv_from_cmd(&this->t3, this->cmd[6]);
    uv_from_cmd(&this->t4, this->cmd[8]);

    u32 col = this->cmd[0] & 0xFFFFFF;
    u32 palette = this->cmd[2] >> 16;
    u32 tx_page = this->cmd[4] >> 16;

    struct PS1_GPU_TEXTURE_SAMPLER ts;
    get_texture_sampler_from_texpage_and_palette(this, tx_page, palette, &ts);

    RT_draw_flat_tex_triangle_modulated_semi(this, &this->t1, &this->t2, &this->t3, col, &ts);
    RT_draw_flat_tex_triangle_modulated_semi(this, &this->t2, &this->t3, &this->t4, col, &ts);
}

static void cmd2f_quad_flat_textured_semi(PS1_GPU *this) {
/*
  WRIOW GP0,(0x2F<<24)                         ; Write GP0 Command Word (Command)
  WRIOW GP0,(Y1<<16)+(X1&0xFFFF)               ; Write GP0  Packet Word (Vertex1)
  WRIOW GP0,(PAL<<16)+((V1&0xFF)<<8)+(U1&0xFF) ; Write GP0  Packet Word (Texcoord1+Palette)
  WRIOW GP0,(Y2<<16)+(X2&0xFFFF)               ; Write GP0  Packet Word (Vertex2)
  WRIOW GP0,(TEX<<16)+((V2&0xFF)<<8)+(U2&0xFF) ; Write GP0  Packet Word (Texcoord2+Texpage)
  WRIOW GP0,(Y3<<16)+(X3&0xFFFF)               ; Write GP0  Packet Word (Vertex3)
  WRIOW GP0,((V3&0xFF)<<8)+(U3&0xFF)           ; Write GP0  Packet Word (Texcoord3)
  WRIOW GP0,(Y4<<16)+(X4&0xFFFF)               ; Write GP0  Packet Word (Vertex4)
  WRIOW GP0,((V4&0xFF)<<8)+(U4&0xFF)           ; Write GP0  Packet Word (Texcoord4)
  */
    xy_from_cmd(&this->t1, this->cmd[1]);
    xy_from_cmd(&this->t2, this->cmd[3]);
    xy_from_cmd(&this->t3, this->cmd[5]);
    xy_from_cmd(&this->t4, this->cmd[7]);
    uv_from_cmd(&this->t1, this->cmd[2]);
    uv_from_cmd(&this->t2, this->cmd[4]);
    uv_from_cmd(&this->t3, this->cmd[6]);
    uv_from_cmd(&this->t4, this->cmd[8]);

    u32 palette = this->cmd[2] >> 16;
    u32 tx_page = this->cmd[4] >> 16;

    struct PS1_GPU_TEXTURE_SAMPLER ts;
    get_texture_sampler_from_texpage_and_palette(this, tx_page, palette, &ts);

    RT_draw_flat_tex_triangle_semi(this, &this->t1, &this->t2, &this->t3, &ts);
    RT_draw_flat_tex_triangle_semi(this, &this->t2, &this->t3, &this->t4, &ts);
}


static void cmd30_tri_shaded_opaque(PS1_GPU *this)
{
    // 0 WRIOW GP0,(0x30<<24)+(COLOR1&0xFFFFFF)       ; Write GP0 Command Word (Color1+Command)
    // 1 WRIOW GP0,(Y1<<16)+(X1&0xFFFF)               ; Write GP0  Packet Word (Vertex1)
    // 2 WRIOW GP0,(COLOR2&0xFFFFFF)                  ; Write GP0  Packet Word (Color2)
    // 3 WRIOW GP0,(Y2<<16)+(X2&0xFFFF)               ; Write GP0  Packet Word (Vertex2)
    // 4 WRIOW GP0,(COLOR3&0xFFFFFF)                  ; Write GP0  Packet Word (Color3)
    // 5 WRIOW GP0,(Y3<<16)+(X3&0xFFFF)               ; Write GP0  Packet Word (Vertex3)
    xy_from_cmd(&this->v1, this->cmd[1]);
    xy_from_cmd(&this->v2, this->cmd[3]);
    xy_from_cmd(&this->v3, this->cmd[5]);
    color24_from_cmd(&this->v1, this->cmd[0]);
    color24_from_cmd(&this->v2, this->cmd[2]);
    color24_from_cmd(&this->v3, this->cmd[4]);
#ifdef LOG_GP0
    printf("\ntri_shaded_opaque ");
    print_v(&this->v1);
    print_v(&this->v2);
    print_v(&this->v3);
#endif
    RT_draw_shaded_triangle(this, &this->v1, &this->v2, &this->v3);
}

static void cmd34_tri_shaded_opaque_tex_modulated(PS1_GPU *this)
{
    // 0 WRIOW GP0,(0x34<<24)+(COLOR1&0xFFFFFF)       ; Write GP0 Command Word (Color1+Command)
    // 1 WRIOW GP0,(Y1<<16)+(X1&0xFFFF)               ; Write GP0  Packet Word (Vertex1)
    // 2 WRIOW GP0,(PAL<<16)+((V1&0xFF)<<8)+(U1&0xFF) ; Write GP0  Packet Word (Texcoord1+Palette)
    // 3 WRIOW GP0,(COLOR2&0xFFFFFF)                  ; Write GP0  Packet Word (Color2)
    // 4 WRIOW GP0,(Y2<<16)+(X2&0xFFFF)               ; Write GP0  Packet Word (Vertex2)
    // 5 WRIOW GP0,(TEX<<16)+((V2&0xFF)<<8)+(U2&0xFF) ; Write GP0  Packet Word (Texcoord2+Texpage)
    // 6 WRIOW GP0,(COLOR3&0xFFFFFF)                  ; Write GP0  Packet Word (Color3)
    // 7 WRIOW GP0,(Y3<<16)+(X3&0xFFFF)               ; Write GP0  Packet Word (Vertex3)
    // 8 WRIOW GP0,((V3&0xFF)<<8)+(U3&0xFF)           ; Write GP0  Packet Word (Texcoord    xy_from_cmd(&this->v1, this->cmd[1]);
    xy_from_cmd(&this->v1, this->cmd[1]);
    xy_from_cmd(&this->v2, this->cmd[4]);
    xy_from_cmd(&this->v3, this->cmd[7]);
    color24_from_cmd(&this->v1, this->cmd[0]);
    color24_from_cmd(&this->v2, this->cmd[3]);
    color24_from_cmd(&this->v3, this->cmd[6]);
    uv_from_cmd(&this->v1, this->cmd[2]);
    uv_from_cmd(&this->v2, this->cmd[5]);
    uv_from_cmd(&this->v3, this->cmd[8]);
    u16 texpage = this->cmd[5] >> 16;
    u16 palette = this->cmd[2] >> 16;
    struct PS1_GPU_TEXTURE_SAMPLER ts;
    get_texture_sampler_from_texpage_and_palette(this, texpage, palette, &ts);
    RT_draw_shaded_tex_triangle_modulated(this, &this->v1, &this->v2, &this->v3, &ts);
}

static void cmd36_tri_shaded_opaque_tex_modulated_semi(PS1_GPU *this)
{
    // 0 WRIOW GP0,(0x34<<24)+(COLOR1&0xFFFFFF)       ; Write GP0 Command Word (Color1+Command)
    // 1 WRIOW GP0,(Y1<<16)+(X1&0xFFFF)               ; Write GP0  Packet Word (Vertex1)
    // 2 WRIOW GP0,(PAL<<16)+((V1&0xFF)<<8)+(U1&0xFF) ; Write GP0  Packet Word (Texcoord1+Palette)
    // 3 WRIOW GP0,(COLOR2&0xFFFFFF)                  ; Write GP0  Packet Word (Color2)
    // 4 WRIOW GP0,(Y2<<16)+(X2&0xFFFF)               ; Write GP0  Packet Word (Vertex2)
    // 5 WRIOW GP0,(TEX<<16)+((V2&0xFF)<<8)+(U2&0xFF) ; Write GP0  Packet Word (Texcoord2+Texpage)
    // 6 WRIOW GP0,(COLOR3&0xFFFFFF)                  ; Write GP0  Packet Word (Color3)
    // 7 WRIOW GP0,(Y3<<16)+(X3&0xFFFF)               ; Write GP0  Packet Word (Vertex3)
    // 8 WRIOW GP0,((V3&0xFF)<<8)+(U3&0xFF)           ; Write GP0  Packet Word (Texcoord    xy_from_cmd(&this->v1, this->cmd[1]);
    xy_from_cmd(&this->v1, this->cmd[1]);
    xy_from_cmd(&this->v2, this->cmd[4]);
    xy_from_cmd(&this->v3, this->cmd[7]);
    color24_from_cmd(&this->v1, this->cmd[0]);
    color24_from_cmd(&this->v2, this->cmd[3]);
    color24_from_cmd(&this->v3, this->cmd[6]);
    uv_from_cmd(&this->v1, this->cmd[2]);
    uv_from_cmd(&this->v2, this->cmd[5]);
    uv_from_cmd(&this->v3, this->cmd[8]);
    u16 texpage = this->cmd[5] >> 16;
    u16 palette = this->cmd[2] >> 16;
    struct PS1_GPU_TEXTURE_SAMPLER ts;
    get_texture_sampler_from_texpage_and_palette(this, texpage, palette, &ts);

    RT_draw_shaded_tex_triangle_modulated_semi(this, &this->v1, &this->v2, &this->v3, &ts);
}

static void cmd32_tri_shaded_semi_transparent(PS1_GPU *this)
{
    // 0 WRIOW GP0,(0x30<<24)+(COLOR1&0xFFFFFF)       ; Write GP0 Command Word (Color1+Command)
    // 1 WRIOW GP0,(Y1<<16)+(X1&0xFFFF)               ; Write GP0  Packet Word (Vertex1)
    // 2 WRIOW GP0,(COLOR2&0xFFFFFF)                  ; Write GP0  Packet Word (Color2)
    // 3 WRIOW GP0,(Y2<<16)+(X2&0xFFFF)               ; Write GP0  Packet Word (Vertex2)
    // 4 WRIOW GP0,(COLOR3&0xFFFFFF)                  ; Write GP0  Packet Word (Color3)
    // 5 WRIOW GP0,(Y3<<16)+(X3&0xFFFF)               ; Write GP0  Packet Word (Vertex3)
    /*struct PS1_GPU_COLOR_SAMPLER *c1 = &this->color1;
    struct PS1_GPU_COLOR_SAMPLER *c2 = &this->color2;
    struct PS1_GPU_COLOR_SAMPLER *c3 = &this->color3;*/
    xy_from_cmd(&this->v1, this->cmd[1]);
    xy_from_cmd(&this->v2, this->cmd[3]);
    xy_from_cmd(&this->v3, this->cmd[5]);
    color24_from_cmd(&this->v1, this->cmd[0]);
    color24_from_cmd(&this->v2, this->cmd[2]);
    color24_from_cmd(&this->v3, this->cmd[4]);
#ifdef LOG_GP0
    printf("\ntri_shaded_opaque ");
    print_v(&this->v1);
    print_v(&this->v2);
    print_v(&this->v3);
#endif
    RT_draw_shaded_triangle_semi(this, &this->v1, &this->v2, &this->v3);
}


static void cmd38_quad_shaded_opaque(PS1_GPU *this)
{
    // WRIOW GP0,(0x38<<24)+(COLOR1&0xFFFFFF)       ; Write GP0 Command Word (Color1+Command)
    // WRIOW GP0,(Y1<<16)+(X1&0xFFFF)               ; Write GP0  Packet Word (Vertex1)
    // WRIOW GP0,(COLOR2&0xFFFFFF)                  ; Write GP0  Packet Word (Color2)
    // WRIOW GP0,(Y2<<16)+(X2&0xFFFF)               ; Write GP0  Packet Word (Vertex2)
    // WRIOW GP0,(COLOR3&0xFFFFFF)                  ; Write GP0  Packet Word (Color3)
    // WRIOW GP0,(Y3<<16)+(X3&0xFFFF)               ; Write GP0  Packet Word (Vertex3)
    // WRIOW GP0,(COLOR4&0xFFFFFF)                  ; Write GP0  Packet Word (Color4)
    // WRIOW GP0,(Y4<<16)+(X4&0xFFFF)               ; Write GP0  Packet Word (Vertex4)
    color24_from_cmd(&this->t1, this->cmd[0]);
    xy_from_cmd(&this->t1, this->cmd[1]);
    color24_from_cmd(&this->t2, this->cmd[2]);
    xy_from_cmd(&this->t2, this->cmd[3]);
    color24_from_cmd(&this->t3, this->cmd[4]);
    xy_from_cmd(&this->t3, this->cmd[5]);
    color24_from_cmd(&this->t4, this->cmd[6]);
    xy_from_cmd(&this->t4, this->cmd[7]);

#ifdef LOG_DRAW_QUADS
    printf("\nquad_shaded ");
    draw_vd(&this->t1);
    draw_vd(&this->t2);
    draw_vd(&this->t3);
    draw_vd(&this->t4);
#endif
    
    RT_draw_shaded_triangle(this, &this->t1, &this->t2, &this->t3);
    RT_draw_shaded_triangle(this, &this->t2, &this->t3, &this->t4);
}

static void cmd3a_quad_shaded_semi_transparent(PS1_GPU *this)
{
    // WRIOW GP0,(0x38<<24)+(COLOR1&0xFFFFFF)       ; Write GP0 Command Word (Color1+Command)
    // WRIOW GP0,(Y1<<16)+(X1&0xFFFF)               ; Write GP0  Packet Word (Vertex1)
    // WRIOW GP0,(COLOR2&0xFFFFFF)                  ; Write GP0  Packet Word (Color2)
    // WRIOW GP0,(Y2<<16)+(X2&0xFFFF)               ; Write GP0  Packet Word (Vertex2)
    // WRIOW GP0,(COLOR3&0xFFFFFF)                  ; Write GP0  Packet Word (Color3)
    // WRIOW GP0,(Y3<<16)+(X3&0xFFFF)               ; Write GP0  Packet Word (Vertex3)
    // WRIOW GP0,(COLOR4&0xFFFFFF)                  ; Write GP0  Packet Word (Color4)
    // WRIOW GP0,(Y4<<16)+(X4&0xFFFF)               ; Write GP0  Packet Word (Vertex4)
    color24_from_cmd(&this->t1, this->cmd[0]);
    xy_from_cmd(&this->t1, this->cmd[1]);
    color24_from_cmd(&this->t2, this->cmd[2]);
    xy_from_cmd(&this->t2, this->cmd[3]);
    color24_from_cmd(&this->t3, this->cmd[4]);
    xy_from_cmd(&this->t3, this->cmd[5]);
    color24_from_cmd(&this->t4, this->cmd[6]);
    xy_from_cmd(&this->t4, this->cmd[7]);

#ifdef LOG_DRAW_QUADS
    printf("\nquad_shaded ");
    draw_vd(&this->t1);
    draw_vd(&this->t2);
    draw_vd(&this->t3);
    draw_vd(&this->t4);
#endif

    RT_draw_shaded_triangle_semi(this, &this->t1, &this->t2, &this->t3);
    RT_draw_shaded_triangle_semi(this, &this->t2, &this->t3, &this->t4);
}

static void cmd3c_quad_opaque_shaded_textured_modulated(PS1_GPU *this) {
    //  0 WRIOW GP0,(0x3C<<24)+(COLOR1&0xFFFFFF)       ; Write GP0 Command Word (Color1+Command)
    //  1 WRIOW GP0,(Y1<<16)+(X1&0xFFFF)               ; Write GP0  Packet Word (Vertex1)
    //  2 WRIOW GP0,(PAL<<16)+((V1&0xFF)<<8)+(U1&0xFF) ; Write GP0  Packet Word (Texcoord1+Palette)
    //  3 WRIOW GP0,(COLOR2&0xFFFFFF)                  ; Write GP0  Packet Word (Color2)
    //  4 WRIOW GP0,(Y2<<16)+(X2&0xFFFF)               ; Write GP0  Packet Word (Vertex2)
    //  5 WRIOW GP0,(TEX<<16)+((V2&0xFF)<<8)+(U2&0xFF) ; Write GP0  Packet Word (Texcoord2+Texpage)
    //  6 WRIOW GP0,(COLOR3&0xFFFFFF)                  ; Write GP0  Packet Word (Color3)
    //  7 WRIOW GP0,(Y3<<16)+(X3&0xFFFF)               ; Write GP0  Packet Word (Vertex3)
    //  8 WRIOW GP0,((V3&0xFF)<<8)+(U3&0xFF)           ; Write GP0  Packet Word (Texcoord3)
    //  9 WRIOW GP0,(COLOR4&0xFFFFFF)                  ; Write GP0  Packet Word (Color4)
    //  10 WRIOW GP0,(Y4<<16)+(X4&0xFFFF)               ; Write GP0  Packet Word (Vertex4)
    //  11 WRIOW GP0,((V4&0xFF)<<8)+(U4&0xFF)           ; Write GP0  Packet Word (Texcoord4)
    xy_from_cmd(&this->t1, this->cmd[1]);
    xy_from_cmd(&this->t2, this->cmd[4]);
    xy_from_cmd(&this->t3, this->cmd[7]);
    xy_from_cmd(&this->t4, this->cmd[10]);

    uv_from_cmd(&this->t1, this->cmd[2]);
    uv_from_cmd(&this->t2, this->cmd[5]);
    uv_from_cmd(&this->t3, this->cmd[8]);
    uv_from_cmd(&this->t4, this->cmd[11]);

    color24_from_cmd(&this->t1, this->cmd[0]);
    color24_from_cmd(&this->t2, this->cmd[3]);
    color24_from_cmd(&this->t3, this->cmd[6]);
    color24_from_cmd(&this->t4, this->cmd[9]);

    u16 texpage = this->cmd[5] >> 16;
    u16 palette = this->cmd[2] >> 16;
    struct PS1_GPU_TEXTURE_SAMPLER ts;
    get_texture_sampler_from_texpage_and_palette(this, texpage, palette, &ts);

    RT_draw_shaded_tex_triangle_modulated(this, &this->t1, &this->t2, &this->t3, &ts);
    RT_draw_shaded_tex_triangle_modulated(this, &this->t2, &this->t3, &this->t4, &ts);
}

static void cmd3e_quad_opaque_shaded_textured_modulated_semi(PS1_GPU *this) {
    //  0 WRIOW GP0,(0x3C<<24)+(COLOR1&0xFFFFFF)       ; Write GP0 Command Word (Color1+Command)
    //  1 WRIOW GP0,(Y1<<16)+(X1&0xFFFF)               ; Write GP0  Packet Word (Vertex1)
    //  2 WRIOW GP0,(PAL<<16)+((V1&0xFF)<<8)+(U1&0xFF) ; Write GP0  Packet Word (Texcoord1+Palette)
    //  3 WRIOW GP0,(COLOR2&0xFFFFFF)                  ; Write GP0  Packet Word (Color2)
    //  4 WRIOW GP0,(Y2<<16)+(X2&0xFFFF)               ; Write GP0  Packet Word (Vertex2)
    //  5 WRIOW GP0,(TEX<<16)+((V2&0xFF)<<8)+(U2&0xFF) ; Write GP0  Packet Word (Texcoord2+Texpage)
    //  6 WRIOW GP0,(COLOR3&0xFFFFFF)                  ; Write GP0  Packet Word (Color3)
    //  7 WRIOW GP0,(Y3<<16)+(X3&0xFFFF)               ; Write GP0  Packet Word (Vertex3)
    //  8 WRIOW GP0,((V3&0xFF)<<8)+(U3&0xFF)           ; Write GP0  Packet Word (Texcoord3)
    //  9 WRIOW GP0,(COLOR4&0xFFFFFF)                  ; Write GP0  Packet Word (Color4)
    //  10 WRIOW GP0,(Y4<<16)+(X4&0xFFFF)               ; Write GP0  Packet Word (Vertex4)
    //  11 WRIOW GP0,((V4&0xFF)<<8)+(U4&0xFF)           ; Write GP0  Packet Word (Texcoord4)
    xy_from_cmd(&this->t1, this->cmd[1]);
    xy_from_cmd(&this->t2, this->cmd[4]);
    xy_from_cmd(&this->t3, this->cmd[7]);
    xy_from_cmd(&this->t4, this->cmd[10]);

    uv_from_cmd(&this->t1, this->cmd[2]);
    uv_from_cmd(&this->t2, this->cmd[5]);
    uv_from_cmd(&this->t3, this->cmd[8]);
    uv_from_cmd(&this->t4, this->cmd[11]);

    color24_from_cmd(&this->t1, this->cmd[0]);
    color24_from_cmd(&this->t2, this->cmd[3]);
    color24_from_cmd(&this->t3, this->cmd[6]);
    color24_from_cmd(&this->t4, this->cmd[9]);

    u16 texpage = this->cmd[5] >> 16;
    u16 palette = this->cmd[2] >> 16;
    struct PS1_GPU_TEXTURE_SAMPLER ts;
    get_texture_sampler_from_texpage_and_palette(this, texpage, palette, &ts);

    RT_draw_shaded_tex_triangle_modulated_semi(this, &this->t1, &this->t2, &this->t3, &ts);
    RT_draw_shaded_tex_triangle_modulated_semi(this, &this->t2, &this->t3, &this->t4, &ts);
}

static void cmd40_line_opaque(PS1_GPU *this) {
    // WRIOW GP0,(0x40<<24)+(COLOR&0xFFFFFF)  ; Write GP0 Command Word (Color+Command)
    // WRIOW GP0,(Y1<<16)+(X1&0xFFFF)         ; Write GP0  Packet Word (Vertex1)
    // WRIOW GP0,(Y2<<16)+(X2&0xFFFF)         ; Write GP0  Packet Word (Vertex2)
    u32 color = BGR24to15(this->cmd[0] & 0xFFFFFF);
    xy_from_cmd(&this->v0, this->cmd[1]);
    xy_from_cmd(&this->v1, this->cmd[2]);
    bresenham_opaque(this, &this->v0, &this->v1, color);
}

static void cmd60_rect_opaque_flat(PS1_GPU *this)
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
            setpix(this, y, x, color, 0, 0);
        }
    }
}

static void cmd64_rect_opaque_flat_textured_modulated(PS1_GPU *this)
{
    // WRIOW GP0,(0x64<<24)+(COLOR&0xFFFFFF)      ; Write GP0 Command Word (Color+Command)

    // WRIOW GP0,(Y<<16)+(X&0xFFFF)               ; Write GP0  Packet Word (Vertex)
    u32 ystart = (this->cmd[1] & 0xFFFF0000) >> 16;
    u32 xstart = ((this->cmd[1] & 0xFFFF) << 16) >> 16;

    u32 col = this->cmd[0] & 0xFFFFFF;
    const static float mm = 1.0f / 128.0f;
    float mr = ((float)(col & 0xFF)) * mm;
    float mg = ((float)((col >> 8) & 0xFF)) * mm;
    float mb = ((float)((col >> 16) & 0xFF)) * mm;

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
    get_texture_sampler_from_texpage_and_palette(this, this->io.GPUSTAT, clut, &ts);
    for (u32 y = ystart; y < yend; y++) {
        u32 u = ustart;
        for (u32 x = xstart; x < xend; x++) {
            u16 color = ts.sample(&ts, u, v);
            u32 hbit = color & 0x8000;

            float r = (float)(color & 0x1F) * mr;
            float g = (float)((color >> 5) & 0x1F) * mg;
            float b = (float)((color >> 10) & 0x1F) * mb;
            if (r > 31.0f) r = 31.0f;
            if (g > 31.0f) g = 31.0f;
            if (b > 31.0f) b = 31.0f;
            u32 lbit = ((u32)r) | ((u32)g << 5) | ((u32)b << 10);

            setpix(this, y, x, lbit, 1, hbit);
            u++;
        }
        v++;
    }
}

static void cmd65_rect_opaque_flat_textured(PS1_GPU *this)
{
    // 0 WRIOW GP0,(0x64<<24)+(COLOR&0xFFFFFF)      ; Write GP0 Command Word (Color+Command)
    // 1 WRIOW GP0,(Y<<16)+(X&0xFFFF)               ; Write GP0  Packet Word (Vertex)
    // 2 WRIOW GP0,(PAL<<16)+((V&0xFF)<<8)+(U&0xFF) ; Write GP0  Packet Word (Texcoord+Palette)
    // 3 WRIOW GP0,(HEIGHT<<16)+(WIDTH&0xFFFF)      ; Write GP0  Packet Word (Width+Height)

    u32 ystart = (this->cmd[1] & 0xFFFF0000) >> 16;
    u32 xstart = ((this->cmd[1] & 0xFFFF) << 16) >> 16;

    u32 clut = (this->cmd[2] >> 16) & 0xFFFF;
    u32 v = (this->cmd[2] >> 8) & 0xFF;
    u32 ustart = this->cmd[2] & 0xFF;

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
    //get_texture_sampler_clut(this, this->texture_depth, this->page_base_x, this->page_base_y, clut, &ts);
    get_texture_sampler_from_texpage_and_palette(this, this->io.GPUSTAT, clut, &ts);
    for (u32 y = ystart; y < yend; y++) {
        u32 u = ustart;
        for (u32 x = xstart; x < xend; x++) {
            u16 color = ts.sample(&ts, u, v);
            u32 hbit = color & 0x8000;
            u32 lbit = color & 0x7FFF;
            setpix(this, y, x, lbit, 1, hbit);
            u++;
        }
        v++;
    }
}

static void cmd66_rect_semi_flat_textured_modulated(PS1_GPU *this)
{
    // WRIOW GP0,(0x64<<24)+(COLOR&0xFFFFFF)      ; Write GP0 Command Word (Color+Command)

    // WRIOW GP0,(Y<<16)+(X&0xFFFF)               ; Write GP0  Packet Word (Vertex)
    u32 ystart = (this->cmd[1] & 0xFFFF0000) >> 16;
    u32 xstart = ((this->cmd[1] & 0xFFFF) << 16) >> 16;

    u32 col = this->cmd[0] & 0xFFFFFF;
    const static float mm = 1.0f / 128.0f;
    float mr = ((float)(col & 0xFF)) * mm;
    float mg = ((float)((col >> 8) & 0xFF)) * mm;
    float mb = ((float)((col >> 16) & 0xFF)) * mm;

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
    get_texture_sampler_from_texpage_and_palette(this, this->io.GPUSTAT, clut, &ts);
    for (u32 y = ystart; y < yend; y++) {
        u32 u = ustart;
        for (u32 x = xstart; x < xend; x++) {
            u16 color = ts.sample(&ts, u, v);
            u32 hbit = color & 0x8000;
            u32 lbit = color & 0x7FFF;

            float r = (float)(lbit & 0x1F) * mr;
            float g = (float)((lbit >> 5) & 0x1F) * mg;
            float b = (float)((lbit >> 10) & 0x1F) * mb;
            if (r > 31.0f) r = 31.0f;
            if (g > 31.0f) g = 31.0f;
            if (b > 31.0f) b = 31.0f;
            lbit = ((u32)r) | ((u32)g << 5) | ((u32)b << 10);

            semipix(this, y, x, lbit, 1, hbit);
            u++;
        }
        v++;
    }
}


static void cmd67_rect_semi_flat_textured(PS1_GPU *this)
{
    // 0 WRIOW GP0,(0x64<<24)+(COLOR&0xFFFFFF)      ; Write GP0 Command Word (Color+Command)
    // 1 WRIOW GP0,(Y<<16)+(X&0xFFFF)               ; Write GP0  Packet Word (Vertex)
    // 2 WRIOW GP0,(PAL<<16)+((V&0xFF)<<8)+(U&0xFF) ; Write GP0  Packet Word (Texcoord+Palette)
    // 3 WRIOW GP0,(HEIGHT<<16)+(WIDTH&0xFFFF)      ; Write GP0  Packet Word (Width+Height)

    u32 ystart = (this->cmd[1] & 0xFFFF0000) >> 16;
    u32 xstart = ((this->cmd[1] & 0xFFFF) << 16) >> 16;

    u32 clut = (this->cmd[2] >> 16) & 0xFFFF;
    u32 v = (this->cmd[2] >> 8) & 0xFF;
    u32 ustart = this->cmd[2] & 0xFF;

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
    //get_texture_sampler_clut(this, this->texture_depth, this->page_base_x, this->page_base_y, clut, &ts);
    get_texture_sampler_from_texpage_and_palette(this, this->io.GPUSTAT, clut, &ts);
    for (u32 y = ystart; y < yend; y++) {
        u32 u = ustart;
        for (u32 x = xstart; x < xend; x++) {
            u16 color = ts.sample(&ts, u, v);
            u32 hbit = color & 0x8000;
            u32 lbit = color & 0x7FFF;
            semipix(this, y, x, lbit, 1, hbit);
            u++;
        }
        v++;
    }
}

static void cmd68_rect_1x1(PS1_GPU *this)
{
    u32 y = (this->cmd[1] & 0xFFFF0000) >> 16;
    u32 x = ((this->cmd[1] & 0xFFFF) << 16) >> 16;
    u32 color = BGR24to15(this->cmd[0] & 0xFFFFFF);
    setpix(this, y, x, color, 0, 0);
}

static void cmd6d_rect_1x1_tex(PS1_GPU *this)
{
    u32 y = (this->cmd[1] & 0xFFFF0000) >> 16;
    u32 x = ((this->cmd[1] & 0xFFFF) << 16) >> 16;
    u32 v = (this->cmd[2] & 0xFF00) >> 8;
    u32 u = (this->cmd[2] & 0xFF);
    u32 palette = (this->cmd[2] >> 16);
    struct PS1_GPU_TEXTURE_SAMPLER ts;
    get_texture_sampler_from_texpage_and_palette(this, this->io.GPUSTAT, palette, &ts);
    u32 color = ts.sample(&ts, u, v);
    setpix(this, y, x, color & 0x7FFF, 1, color & 0x8000);
}

static void cmd6c_rect_1x1_tex_modulated(PS1_GPU *this)
{
    u32 y = (this->cmd[1] & 0xFFFF0000) >> 16;
    u32 x = ((this->cmd[1] & 0xFFFF) << 16) >> 16;
    u32 v = (this->cmd[2] & 0xFF00) >> 8;
    u32 u = (this->cmd[2] & 0xFF);
    u32 palette = (this->cmd[2] >> 16);
    struct PS1_GPU_TEXTURE_SAMPLER ts;
    get_texture_sampler_from_texpage_and_palette(this, this->io.GPUSTAT, palette, &ts);
    u32 color = ts.sample(&ts, u, v);
    u32 hbit = color & 0x8000;

    u32 tc = this->cmd[0] & 0xFFFFFF;
    static const float mm = 1.0f / 128.0f;
    float rm = (float)(tc & 0xFF) * mm;
    float gm = (float)((tc >> 8) & 0xFF) * mm;
    float bm = (float)((tc >> 16) & 0xFF) * mm;

    float r = (float)(color & 0x1F) * rm;
    float g = (float)((color >> 5) & 0x1F) * gm;
    float b = (float)((color >> 10) & 0x1F) * bm;
    if (r > 31.0f) r = 31.0f;
    if (g > 31.0f) g = 31.0f;
    if (b > 31.0f) b = 31.0f;

    color = ((u32)r) | ((u32)g << 5) | ((u32)b << 10);

    setpix(this, y, x, color & 0x7FFF, 1, hbit);
}

static void cmd6e_rect_1x1_tex_semi_modulated(PS1_GPU *this)
{
    u32 y = (this->cmd[1] & 0xFFFF0000) >> 16;
    u32 x = ((this->cmd[1] & 0xFFFF) << 16) >> 16;
    u32 v = (this->cmd[2] & 0xFF00) >> 8;
    u32 u = (this->cmd[2] & 0xFF);
    u32 palette = (this->cmd[2] >> 16);
    struct PS1_GPU_TEXTURE_SAMPLER ts;
    get_texture_sampler_from_texpage_and_palette(this, this->io.GPUSTAT, palette, &ts);
    u32 color = ts.sample(&ts, u, v);
    u32 hbit = color & 0x8000;

    u32 tc = this->cmd[0] & 0xFFFFFF;
    static const float mm = 1.0f / 128.0f;
    float rm = (float)(tc & 0xFF) * mm;
    float gm = (float)((tc >> 8) & 0xFF) * mm;
    float bm = (float)((tc >> 16) & 0xFF) * mm;

    float r = (float)(color & 0x1F) * rm;
    float g = (float)((color >> 5) & 0x1F) * gm;
    float b = (float)((color >> 10) & 0x1F) * bm;

    color = ((u32)r) | ((u32)g << 5) | ((u32)b << 10);

    semipix(this, y, x, color & 0x7FFF, 1, hbit);
}


static void cmd6f_rect_1x1_tex_semi(PS1_GPU *this)
{
    u32 y = (this->cmd[1] & 0xFFFF0000) >> 16;
    u32 x = ((this->cmd[1] & 0xFFFF) << 16) >> 16;
    u32 v = (this->cmd[2] & 0xFF00) >> 8;
    u32 u = (this->cmd[2] & 0xFF);
    u32 palette = (this->cmd[2] >> 16);
    struct PS1_GPU_TEXTURE_SAMPLER ts;
    get_texture_sampler_from_texpage_and_palette(this, this->io.GPUSTAT, palette, &ts);
    u32 color = ts.sample(&ts, u, v);
    semipix(this, y, x, color & 0x7FFF, 1, color & 0x8000);
}


static void cmd80_vram_copy(PS1_GPU *this)
{
    u32 y1 = this->cmd[1] >> 16;
    u32 x1 = this->cmd[1] & 0xFFFF;
    u32 y2 = this->cmd[2] >> 16;
    u32 x2 = this->cmd[2] & 0xFFFF;
    u32 height = this->cmd[3] >> 16;
    u32 width = this->cmd[3] & 0xFFFF;
#ifdef LOG_GP0
    printf("\nGP0 cmd80 copy from %d,%d to %d,%d size:%d,%d", x1, y1, x2, y2, width, height);
#endif

    for (u32 iy = 0; iy < height; iy++) {
        for (u32 ix = 0; ix < width; ix++) {
            u32 get_addr = ((ix+x1) & 1023) * 2;
            get_addr += ((iy+y1) & 511) * 2048; // 2048 bytes per 1024-pixel row
            if (this->preserve_masked_pixels) {
                u16 t = cR16(this->VRAM, (((iy + y2) & 511) * 2048) + (((ix + x2) & 1023) * 2));
                if (t & 0x8000) continue;
            }

            u16 v = cR16(this->VRAM, get_addr & 0xFFFFF);
            setpix(this, iy+y2, ix+x2, v & 0x7FFF, 1, v & 0x8000);
        }
    }

    //printf("\nCOPY VRAM %d,%d to %d,%d size:%d,%d")
}

static void rect_opaque_flat_textured_modulated_xx(PS1_GPU *this, u32 wh)
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
    u32 height = wh;
    u32 width = wh;

    u32 xend = (xstart + width);
    u32 yend = (ystart + height);
    xend = xend > 1024 ? 1024 : xend;
    yend = yend > 512 ? 512 : yend;
#ifdef LOG_GP0
    printf("\nrect_opaque_flat %d %d %d %d", xstart, ystart, width, height);
#endif

    u32 tc = this->cmd[0] & 0xFFFFFF;
    static const float mm = 1.0f / 128.0f;
    float rm = (float)(tc & 0xFF) * mm;
    float gm = (float)((tc >> 8) & 0xFF) * mm;
    float bm = (float)((tc >> 16) & 0xFF) * mm;

    struct PS1_GPU_TEXTURE_SAMPLER ts;
    get_texture_sampler_from_texpage_and_palette(this, this->io.GPUSTAT, clut, &ts);
    for (u32 y = ystart; y < yend; y++) {
        u32 u = ustart;
        for (u32 x = xstart; x < xend; x++) {
            u16 color = ts.sample(&ts, u, v);
            u32 hbit = color & 0x8000;

            float r = (float)(color & 0x1F) * rm;
            float g = (float)((color >> 5) & 0x1F) * gm;
            float b = (float)((color >> 10) & 0x1F) * bm;
            if (r > 31.0f) r = 31.0f;
            if (g > 31.0f) g = 31.0f;
            if (b > 31.0f) b = 31.0f;

            setpix_split(this, y, x, (u32)r, (u32)g, (u32)b, 1, hbit);
            u++;
        }
        v++;
    }
}

static void cmd7c_rect_opaque_flat_textured_modulated_16x16(PS1_GPU *this)
{
    rect_opaque_flat_textured_modulated_xx(this, 16);
}

static void cmd74_rect_opaque_flat_textured_modulated_8x8(PS1_GPU *this)
{
    rect_opaque_flat_textured_modulated_xx(this, 8);
}

static void rect_opaque_flat_textured_xx(PS1_GPU *this, u32 wh)
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
    u32 height = wh;
    u32 width = wh;

    u32 xend = (xstart + width);
    u32 yend = (ystart + height);
    xend = xend > 1024 ? 1024 : xend;
    yend = yend > 512 ? 512 : yend;
#ifdef LOG_GP0
    printf("\nrect_opaque_flat %d %d %d %d", xstart, ystart, width, height);
#endif

    struct PS1_GPU_TEXTURE_SAMPLER ts;
    get_texture_sampler_from_texpage_and_palette(this, this->io.GPUSTAT, clut, &ts);
    for (u32 y = ystart; y < yend; y++) {
        u32 u = ustart;
        for (u32 x = xstart; x < xend; x++) {
            u16 color = ts.sample(&ts, u, v);
            u32 hbit = color & 0x8000;
            u32 lbit = color & 0x7FFF;
            setpix(this, y, x, lbit, 1, hbit);
            u++;
        }
        v++;
    }
}

static void cmd75_rect_opaque_flat_textured_8x8(PS1_GPU *this)
{
    rect_opaque_flat_textured_xx(this, 8);
}

static void cmd7d_rect_opaque_flat_textured_16x16(PS1_GPU *this)
{
    rect_opaque_flat_textured_xx(this, 16);
}


static void rect_semi_flat_textured_modulated_xx(PS1_GPU *this, u32 wh)
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
    u32 height = wh;
    u32 width = wh;

    u32 xend = (xstart + width);
    u32 yend = (ystart + height);
    xend = xend > 1024 ? 1024 : xend;
    yend = yend > 512 ? 512 : yend;
#ifdef LOG_GP0
    printf("\nrect_opaque_flat %d %d %d %d", xstart, ystart, width, height);
#endif

    u32 tc = this->cmd[0] & 0xFFFFFF;
    static const float mm = 1.0f / 128.0f;
    float rm = (float)(tc & 0xFF) * mm;
    float gm = (float)((tc >> 8) & 0xFF) * mm;
    float bm = (float)((tc >> 16) & 0xFF) * mm;

    struct PS1_GPU_TEXTURE_SAMPLER ts;
    get_texture_sampler_from_texpage_and_palette(this, this->io.GPUSTAT, clut, &ts);
    for (u32 y = ystart; y < yend; y++) {
        u32 u = ustart;
        for (u32 x = xstart; x < xend; x++) {
            u16 color = ts.sample(&ts, u, v);
            u32 hbit = color & 0x8000;

            float r = (float)(color & 0x1F) * rm;
            float g = (float)((color >> 5) & 0x1F) * gm;
            float b = (float)((color >> 10) & 0x1F) * bm;
            if (r > 31.0f) r = 31.0f;
            if (g > 31.0f) g = 31.0f;
            if (b > 31.0f) b = 31.0f;

            semipix_split(this, y, x, (u32)r, (u32)g, (u32)b, 1, hbit);
            u++;
        }
        v++;
    }
}

static void cmd76_rect_semi_flat_textured_modulated_8x8(PS1_GPU *this)
{
    rect_semi_flat_textured_modulated_xx(this, 8);
}

static void cmd7e_rect_semi_flat_textured_modulated_16x16(PS1_GPU *this)
{
    rect_semi_flat_textured_modulated_xx(this, 16);
}

static void rect_semi_flat_textured_xx(PS1_GPU *this, u32 wh)
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
    u32 height = wh;
    u32 width = wh;

    u32 xend = (xstart + width);
    u32 yend = (ystart + height);
    xend = xend > 1024 ? 1024 : xend;
    yend = yend > 512 ? 512 : yend;
#ifdef LOG_GP0
    printf("\nrect_opaque_flat %d %d %d %d", xstart, ystart, width, height);
#endif

    struct PS1_GPU_TEXTURE_SAMPLER ts;
    get_texture_sampler_from_texpage_and_palette(this, this->io.GPUSTAT, clut, &ts);
    for (u32 y = ystart; y < yend; y++) {
        u32 u = ustart;
        for (u32 x = xstart; x < xend; x++) {
            u16 color = ts.sample(&ts, u, v);
            u32 hbit = color & 0x8000;
            u32 lbit = color & 0x7FFF;
            semipix(this, y, x, lbit, 1, hbit);
            u++;
        }
        v++;
    }
}

static void cmd77_rect_semi_flat_textured_8x8(PS1_GPU *this)
{
    rect_semi_flat_textured_xx(this, 8);
}

static void cmd7f_rect_semi_flat_textured_16x16(PS1_GPU *this)
{
    rect_semi_flat_textured_xx(this, 16);
}

static void load_buffer_reset(PS1_GPU *this, u32 x, u32 y, u32 width, u32 height)
{
    this->load_buffer.x = x;
    this->load_buffer.y = y;
    this->load_buffer.width = width;
    this->load_buffer.height = height;
    this->load_buffer.line_ptr = (y * 2048) + x;
    this->load_buffer.img_x = this->load_buffer.img_y = 0;
}

static void gp0_image_load_continue(PS1_GPU *this, u32 cmd)
{
    /*this->recv_gp0[this->recv_gp0_len] = cmd;
    this->recv_gp0_len++;
    if (this->recv_gp0_len >= (1024*1024)) {
        printf("\nWARNING GP0 TRANSFER OVERFLOW!");
        this->recv_gp0_len--;
    }*/
    // Put in 2 16-bit pixels
    //console.log('TRANSFERRING!', this->gp0_transfer_remaining);
    for (u32 i = 0; i < 2; i++) {
        u32 px = cmd & 0xFFFF;
        cmd >>= 16;
        u32 y = this->load_buffer.y+this->load_buffer.img_y;
        u32 x = this->load_buffer.x+this->load_buffer.img_x;
        u32 addr = (2048*y)+(x*2) & 0xFFFFF;
        u32 draw_it = 1;
        if (this->preserve_masked_pixels) {
            u16 v = cR16(this->VRAM, addr);
            if (v & 0x8000) draw_it = 0;
        }
        //if (!((px & 0x8000) || ((px & 0x7FFF) != 0))) draw_it = 0;
        if (draw_it) {
            cW16(this->VRAM, addr, px);
        }

        this->load_buffer.img_x++;
        if ((x+1) >= (this->load_buffer.width+this->load_buffer.x)) {
            this->load_buffer.img_x = 0;
            this->load_buffer.img_y++;
        }
    }
    this->gp0_transfer_remaining--;
    if (this->gp0_transfer_remaining == 0) {
        //printf("\nTRANSFER COMPLETE!");
        this->current_ins = NULL;
        this->handle_gp0 = &gp0_cmd;
        ready_cmd(this);
        unready_recv_dma(this);
    }
}

static void gp0_image_load_start(PS1_GPU *this)
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
    //printf("\nNEW TRANSFER x,y:%d,%d width,height:%d,%d", x, y, width, height);

    this->gp0_transfer_remaining = imgsize/2;
#ifdef LOG_GP0
    printf("\nTRANSFER IMGSIZE %d X:%d Y:%d WIDTH:%d HEIGHT:%d CMD:%08x", imgsize, x, y, width, height, this->cmd[0]);
#endif
    if (this->gp0_transfer_remaining > 0) {
        load_buffer_reset(this, x, y, width, height);
        this->handle_gp0 = &gp0_image_load_continue;
    } else {
        printf("\nBad size image load: 0?");
        this->current_ins = NULL;
    }
}

static void gp0_cmd_unhandled(PS1_GPU *this)
{

}

static void GPUSTAT_update(PS1_GPU *this)
{
    u32 o = this->page_base_x;
    o |= (this->page_base_y) << 4;
    o |= (this->semi_transparency) << 5;
    o |= (this->texture_depth) << 7;
    o |= (this->dithering) << 9;
    o |= (this->draw_to_display) << 10;
    o |= (this->force_set_mask_bit) >> 4;
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

void PS1_GPU_write_gp0(PS1_GPU *this, u32 cmd) {
    this->handle_gp0(this, cmd);
}


static void gp0_cmd(PS1_GPU *this, u32 cmd) {
    this->gp0_buffer[this->recv_gp0_len] = cmd;
    this->recv_gp0_len++;
    assert(this->recv_gp0_len < 256);

    // Check if we have an instruction..
    if (this->current_ins) {
        this->cmd[this->cmd_arg_index++] = cmd;
        assert(this->cmd_arg_index<32);
        if (this->cmd_arg_index == this->cmd_arg_num) {
            // Execute instruction!
            this->current_ins(this);
            this->current_ins = NULL;
            this->recv_gp0_len = 0;
            this->cmd_arg_index = 0;
        }
    } else {
        this->cmd[0] = cmd;
        this->cmd_arg_index = 1;
        this->ins_special = 0;
        this->cmd_arg_num = 1;
        u32 cmdr = cmd >> 24;
#ifdef LOG_GP0
        printf("\nCMD %02x", cmdr);
#endif
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
            case 0x20: // flat-shaded opaque triangle
                this->current_ins = &cmd20_tri_flat;
                this->cmd_arg_num = 4;
                break;
            case 0x22: // flat-shaded opaque triangle, semi-transparent
                this->current_ins = &cmd22_tri_flat_semi_transparent;
                this->cmd_arg_num = 4;
                break;
            case 0x24: // flat-shaded opaque triangle, semi-transparent, textured, modulated
                this->current_ins = &cmd24_tri_raw_modulated;
                this->cmd_arg_num = 7;
                break;
            case 0x25: // flat-shaded triangle, opaque, "raw"
                this->current_ins = &cmd25_tri_raw;
                this->cmd_arg_num = 7;
                break;
            case 0x26: // flat-shaded triangle, opaque, modulated, semi-transparent
                this->current_ins = &cmd26_tri_modulated_semi_transparent;
                this->cmd_arg_num = 7;
                break;
            case 0x27: // Textured Triangle, Semi-Transparent, Raw-Texture
                this->current_ins = &cmd27_tri_raw_semi_transparent;
                this->cmd_arg_num = 7;
                break;
            case 0x28: // flat-shaded rectangle
                this->current_ins = &cmd28_draw_flat4untex;
                this->cmd_arg_num = 5;
                break;
            case 0x2A: // semi-transparent monochrome quad
                this->current_ins = &cmd2a_quad_flat_semi_transparent;
                this->cmd_arg_num = 5;
                break;
            case 0x2C: // polygon, 4 points, textured, flat, modulated
                this->current_ins = &cmd2c_quad_opaque_flat_textured_modulated;
                this->cmd_arg_num = 9;
                break;
            case 0x2D: // textured quad opaque raw
                this->current_ins = &cmd2d_quad_opaque_flat_textured;
                this->cmd_arg_num = 9;
                break;
            case 0x2E: // textured quad, semi-transparent, modulated
                this->current_ins = &cmd2e_quad_flat_textured_modulated;
                this->cmd_arg_num = 9;
                break;
            case 0x2F: // textured quad, semi-transparent, raw tex
                this->current_ins = &cmd2f_quad_flat_textured_semi;
                this->cmd_arg_num = 9;
                break;
            case 0x30: // opaque shaded trinalge
                this->current_ins = &cmd30_tri_shaded_opaque;
                this->cmd_arg_num = 6;
                break;
            case 0x32: // shaded triangle semi-transparent
                this->current_ins = &cmd32_tri_shaded_semi_transparent;
                this->cmd_arg_num = 6;
                break;
            case 0x34: // opaque shaded textured triangle modulated
                this->current_ins = &cmd34_tri_shaded_opaque_tex_modulated;
                this->cmd_arg_num = 9;
                break;
            case 0x36: // opaque shaded textured semi-transparent triangle modulated
                this->current_ins = &cmd36_tri_shaded_opaque_tex_modulated_semi;
                this->cmd_arg_num = 9;
                break;
            case 0x38: // polygon, 4 points, gouraud-shaded
                this->current_ins = &cmd38_quad_shaded_opaque;
                this->cmd_arg_num = 8;
                break;
            case 0x3A: // polygon, 4 points, goraud-shaded, semi-transparent
                this->current_ins = &cmd3a_quad_shaded_semi_transparent;
                this->cmd_arg_num = 8;
                break;
            case 0x3C: // polygon, 4 points, goraud-shaded, textured & modulated
                this->current_ins = &cmd3c_quad_opaque_shaded_textured_modulated;
                this->cmd_arg_num = 12;
                break;
            case 0x3E: // polygon, 4 points, goraud-shaded, textured & modulated & semi-transparent
                this->current_ins = &cmd3e_quad_opaque_shaded_textured_modulated_semi;
                this->cmd_arg_num = 12;
                break;
            case 0x40: // monochrome line, opaque
                this->current_ins = &cmd40_line_opaque;
                this->cmd_arg_num = 3;
                break;
            case 0x60: // Rectangle, variable size, opaque
                this->current_ins = &cmd60_rect_opaque_flat;
                this->cmd_arg_num = 3;
                break;
            case 0x64: // Rectangle, variable size, textured, flat, opaque, modulated
                this->current_ins = &cmd64_rect_opaque_flat_textured_modulated;
                this->cmd_arg_num = 4;
                break;
            case 0x65: // rectangle, variable size, textured, flat, opaque
                this->current_ins = &cmd65_rect_opaque_flat_textured;
                this->cmd_arg_num = 4;
                break;
            case 0x66: // Rectangle, variable size, textured, flat, opaque, modulated
                this->current_ins = &cmd66_rect_semi_flat_textured_modulated;
                this->cmd_arg_num = 4;
                break;
            case 0x67: // rectangle, variable size, textured, flat, semi-transparent
                this->current_ins = &cmd67_rect_semi_flat_textured;
                this->cmd_arg_num = 4;
                break;
            case 0x6A: // Solid 1x1 rect
            case 0x68:
                this->current_ins = &cmd68_rect_1x1;
                this->cmd_arg_num = 2;
                break;
            case 0x6C: // 1x1 rect texuted semi-transparent
                this->current_ins = &cmd6c_rect_1x1_tex_modulated;
                this->cmd_arg_num = 3;
                break;
            case 0x6D: // 1x1 rect texuted opaque
                this->current_ins = &cmd6d_rect_1x1_tex;
                this->cmd_arg_num = 3;
                break;
            case 0x6e: // 1x1 rect texuted semi modulated
                this->current_ins = &cmd6e_rect_1x1_tex_semi_modulated;
                this->cmd_arg_num = 3;
                break;
            case 0x6F: // 1x1 rect texuted semi-transparent
                this->current_ins = &cmd6f_rect_1x1_tex_semi;
                this->cmd_arg_num = 3;
                break;
            case 0x74: // Rectangle, 8x8, opaque, textured, modulated
                this->current_ins = &cmd74_rect_opaque_flat_textured_modulated_8x8;
                this->cmd_arg_num = 3;
                break;
            case 0x75: // Rectangle, 8x8, opaque, textured
                this->current_ins = &cmd75_rect_opaque_flat_textured_8x8;
                this->cmd_arg_num = 3;
                break;
            case 0x76: // Rectangle, 8x8, semi-transparent, textured, modulated
                this->current_ins = &cmd76_rect_semi_flat_textured_modulated_8x8;
                this->cmd_arg_num = 3;
                break;
            case 0x77: // Rectangle, 8x8, semi, textured
                this->current_ins = &cmd77_rect_semi_flat_textured_8x8;
                this->cmd_arg_num = 3;
                break;
            case 0x7C:// Rectangle, 16x16, opaque, textured, modulated
                this->current_ins = &cmd7c_rect_opaque_flat_textured_modulated_16x16;
                this->cmd_arg_num = 3;
                break;

            case 0x7D:// Rectangle, 16x16, opaque, textured
                this->current_ins = &cmd7d_rect_opaque_flat_textured_16x16;
                this->cmd_arg_num = 3;
                break;
            case 0x7E: // Rectangle, 8x8, semi-transparent, textured, modulated
                this->current_ins = &cmd7e_rect_semi_flat_textured_modulated_16x16;
                this->cmd_arg_num = 3;
                break;
            case 0x7F: // Rectangle, 8x8, semi, textured
                this->current_ins = &cmd7f_rect_semi_flat_textured_16x16;
                this->cmd_arg_num = 3;
                break;
            case 0x80: // Copy from place to place
                this->current_ins = &cmd80_vram_copy;
                this->cmd_arg_num = 4;
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
                printf("GP0 E1 set draw mode");
#endif
                //printf("\nSET DRAW MODE %08x", cmd);
                this->page_base_x = cmd & 15;
                this->page_base_y = (cmd >> 4) & 1;
                this->semi_transparency = (cmd >> 5) & 3;
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
                this->rect.texture_y_flip = (cmd >> 13) & 1;
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
                this->draw_area_top = (cmd >> 10) & 0x3FF;
                this->draw_area_left = cmd & 0x3FF;
#ifdef DBG_GP0
                printf("\nGP0 E3 set draw area UL corner %d, %d", this->draw_area_top, this->draw_area_left);
#endif
                break;
            case 0xE4: // Draw area lower-right corner
                this->draw_area_bottom = (cmd >> 10) & 0x3FF;
                this->draw_area_right = cmd & 0x3FF;
#ifdef DBG_GP0
                printf("\nGP0 E4 set draw area LR corner %d, %d", this->draw_area_right, this->draw_area_bottom);
#endif
                break;
            case 0xE5: // Drawing offset
                this->draw_x_offset = mksigned11(cmd & 0x7FF);
                this->draw_y_offset = mksigned11((cmd >> 11) & 0x7FF);
#ifdef DBG_GP0
                printf("\nGP0 E5 set drawing offset %d, %d", this->draw_x_offset, this->draw_y_offset);
#endif
                break;
            case 0xE6: // Set Mask Bit setting
#ifdef DBG_GP0
                printf("\nGP0 E6 set bit mask");
#endif
                this->force_set_mask_bit = (cmd & 1) << 15;
                this->preserve_masked_pixels = (cmd >> 1) & 1;
                break;
            default:
                printf("\nUnknown GP0 command %08x", cmd);
                break;
        }
    }
}

static void setup_dotclock(PS1 *this)
{
    if (this->gpu.hr2) {
        this->gpu.hres = 368;
    }
    else {
        static const u32 table[4] =  {256, 320, 512, 640};
        this->gpu.hres = table[this->gpu.hr1];
    }
    float cycles_per_line = ((float)this->clock.timing.gpu.hz / this->clock.timing.fps) / (float)this->clock.timing.frame.lines;
    float cycles_per_px = cycles_per_line / (float)this->gpu.hres;

    this->clock.dot.horizontal_px = (((this->gpu.display_horiz_end-this->gpu.display_horiz_start)/(u32)cycles_per_px)+2) & ~3;
    this->clock.dot.vertical_px = this->clock.timing.frame.lines - this->clock.timing.frame.vblank.start_on_line;

    this->clock.dot.ratio.cpu_to_gpu = (float)this->clock.timing.gpu.hz / (float)this->clock.timing.cpu.hz;
    this->clock.dot.ratio.cpu_to_dotclock = this->clock.dot.ratio.cpu_to_gpu / cycles_per_px;
}

static void dotclock_change(PS1 *this)
{
    this->clock.dot.start.value = PS1_dotclock(this);
    this->clock.dot.start.time = PS1_clock_current(this);
    setup_dotclock(this);
}


void PS1_GPU_reset(PS1_GPU *this)
{
    dotclock_change(this->bus);
}

void PS1_GPU_write_gp1(PS1_GPU *this, u32 cmd)
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
            this->recv_gp0_len = 0;
            this->handle_gp0 = &gp0_cmd;
            this->current_ins = NULL;
            this->cmd_arg_index = 0;
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
            dotclock_change(this->bus);
            break;
        case 0x07: // Display vertical range, in output coordinates
            this->display_line_start = cmd & 0x3FF;
            this->display_line_end = (cmd >> 10) & 0x3FF;
            break;
        case 0x08: {// Display mode
            //console.log('GP1 display mode');
            this->hr1 = cmd & 3;
            this->hr2 = ((cmd >> 6) & 1);

            this->vres = (cmd & 4) ? PS1e_y480lines : PS1e_y240lines;
            this->vmode = (cmd & 8) ? PS1e_pal : PS1e_ntsc;
            this->display_depth = (cmd & 16) ? PS1e_d15bits : PS1e_d24bits;
            this->interlaced = (cmd >> 5) & 1;
            if ((cmd & 0x80) != 0) {
                printf("\nUnsupported display mode!");
            }
            dotclock_change(this->bus);
            break; }
        default:
            printf("\nUnknown GP1 command %08x", cmd);
            break;
    }
}

u32 PS1_GPU_get_gpuread(PS1_GPU *this)
{
    return this->io.GPUREAD;
}
u32 PS1_GPU_get_gpustat(PS1_GPU *this)
{
    return this->io.GPUSTAT | 0x10000000;
}

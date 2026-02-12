//
// Created by . on 2/15/25.
//
#include <cstdlib>
#include <cstring>
#include <math.h>
#include <cassert>

#include "pixel_helpers.h"
#include "../ps1_bus.h"
#include "../ps1_timers.h"
#include "ps1_gpu.h"
#include "rasterize_line.h"

#include "helpers/multisize_memaccess.cpp"

//#define LOG_GP0
//#define DBG_GP0
namespace PS1 {
void core::setup_dotclock()
{
    if (gpu.hr2) {
        gpu.hres = 368;
    }
    else {
        static constexpr u32 table[4] =  {256, 320, 512, 640};
        gpu.hres = table[gpu.hr1];
    }
    float cycles_per_line = (static_cast<float>(clock.timing.gpu.hz) / clock.timing.fps) / static_cast<float>(clock.timing.frame.lines);
    float cycles_per_px = cycles_per_line / static_cast<float>(gpu.hres);

    clock.dot.horizontal_px = (((gpu.display_horiz_end-gpu.display_horiz_start)/static_cast<u32>(cycles_per_px))+2) & ~3;
    clock.dot.vertical_px = clock.timing.frame.lines - clock.timing.frame.vblank.start_on_line;

    clock.dot.ratio.cpu_to_gpu = static_cast<float>(clock.timing.gpu.hz) / static_cast<float>(clock.timing.cpu.hz);
    clock.dot.ratio.cpu_to_dotclock = clock.dot.ratio.cpu_to_gpu / cycles_per_px;
}

void core::dotclock_change()
{
    clock.dot.start.value = dotclock();
    clock.dot.start.time = clock_current();
    setup_dotclock();
}


}

namespace PS1::GPU {

void TEXTURE_SAMPLER::mk_new(u32 page_x_in, u32 page_y_in, u32 clut_addr_in, core *bus)
{
    page_x = (page_x_in & 0x0F) << 6; // * 64
    page_y = (page_y_in & 1) * 256;
    base_addr = (page_y * 2048) + (page_x * 2);
    clut_addr = clut_addr_in;
    VRAM = bus->VRAM;
}

#define R_GPUSTAT 0
#define R_GPUPLAYING 1
#define R_GPUQUIT 2
#define R_GPUGP1 3
#define R_GPUREAD 4
#define R_LASTUSED 23

core::core(PS1::core *parent) : bus(parent)
{
    display_horiz_start = 0x200;
    display_horiz_end = 0xC00;
    display_line_start = 0x10;
    display_line_end = 0x100;
    handle_gp0 = &core::core::gp0_cmd;
}

void core::unready_cmd()
{
    //static u32 e = 0;
    //e++;
    //printf("\nUNREADY CMD %d", e);
    io.GPUSTAT &= 0xFBFFFFFF;
}

void core::cmd02_quick_rect()
{
    unready_all();
    u32 ysize = (CMD[2] >> 16) & 0xFFFF;
    u32 xsize = (CMD[2]) & 0xFFFF;
    u32 BGR = BGR24to15(CMD[0] & 0xFFFFFF);
    u32 start_y = (CMD[1] >> 16) & 0xFFFF;
    u32 start_x = (CMD[1]) & 0xFFFF;
    //if (LOG_DRAW_QUADS) console.log('QUICKRECT! COLOR', hex4(BGR), 'X Y', start_x, start_y, 'SZ X SZ Y', xsize, ysize);
    for (u32 y = start_y; y < (start_y+ysize); y++) {
        for (u32 x = start_x; x < (start_x + xsize); x++) {
            u32 addr = (2048*y)+(x*2);
            cW[2](VRAM, addr, BGR);
        }
    }

    ready_all();
}

static inline i32 mksigned11(u32 v)
{
    return SIGNe11to32(v);
}

void core::cmd28_draw_flat4untex()
{
    // Flat 4-vertex untextured poly
    V0.xy_from_cmd(CMD[1]);
    V1.xy_from_cmd(CMD[2]);
    V2.xy_from_cmd(CMD[3]);
    V3.xy_from_cmd(CMD[4]);

    u32 color = BGR24to15(CMD[0] & 0xFFFFFF);
#ifdef LOG_DRAW_QUADS
    printf("\nflat4untex %d %d %d %d %d %d %06x", x0, y0, x1, y1, x2, y2, color);
#endif
    RT_draw_flat_triangle(&V0, &V1, &V2, color);
    RT_draw_flat_triangle(&V1, &V2, &V3, color);
}


static u16 sample_tex_4bit(TEXTURE_SAMPLER *ts, i32 u, i32 v)
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

static u16 sample_tex_8bit(TEXTURE_SAMPLER *ts, i32 u, i32 v)
{
    u32 d = ts->VRAM[(ts->base_addr + ((v&0xFF)<<11) + (u&0x7F)) & 0xFFFFF];
    return cR16(ts->VRAM, (ts->clut_addr + (d*2)) & 0xFFFFF);
}

static u16 sample_tex_15bit(TEXTURE_SAMPLER *ts, i32 u, i32 v)
{
    u32 addr = ts->base_addr + ((v&0xFF)<<11) + ((u&0x7F)<<1);
    return cR16(ts->VRAM, addr & 0xFFFFF);
}

void core::get_texture_sampler_from_texpage_and_palette(u32 texpage, u32 palette, TEXTURE_SAMPLER *ts)
{
    u32 clx = (palette & 0x3F) << 4;
    u32 cly = (palette >> 6) & 0x1FF;

    u32 clut_addr = (cly * 2048) + (clx * 2);

    u32 tx_x = texpage & 15;
    u32 tx_y = (texpage >> 4) & 1;
    ts->mk_new(tx_x, tx_y, clut_addr, this);
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

void core::cmd20_tri_flat()
{
    V0.xy_from_cmd(CMD[1]);
    V1.xy_from_cmd(CMD[2]);
    V2.xy_from_cmd(CMD[3]);
    u32 color = BGR24to15(CMD[0] & 0xFFFFFF);
#ifdef LOG_GP0
    printf("\nGP0_20 flat tri %f,%f %f,%f %f,%f color:%04x", x0, y0, x1, y1, x2, y2, color);
#endif
    RT_draw_flat_triangle(&V0, &V1, &V2, color);
}

void core::cmd22_tri_flat_semi_transparent()
{
    V0.xy_from_cmd(CMD[1]);
    V1.xy_from_cmd(CMD[2]);
    V2.xy_from_cmd(CMD[3]);
    V0.color24_from_cmd(CMD[0]);
    u32 color = BGR24to15(CMD[0] & 0xFFFFFF);
#ifdef LOG_GP0
    printf("\nGP0_20 flat tri %f,%f %f,%f %f,%f color:%04x", x0, y0, x1, y1, x2, y2, color);
#endif
    RT_draw_flat_triangle_semi(&V0, &V1, &V2, V0.r, V0.g, V0.b);
}

void core::cmd24_tri_raw_modulated()
{
    /*
  0 WRIOW GP0,(0x24<<24)+(COLOR&0xFFFFFF)        ; Write GP0 Command Word (Color+Command)
  1 WRIOW GP0,(Y1<<16)+(X1&0xFFFF)               ; Write GP0  Packet Word (Vertex1)
  2 WRIOW GP0,(PAL<<16)+((V1&0xFF)<<8)+(U1&0xFF) ; Write GP0  Packet Word (Texcoord1+Palette)
  3 WRIOW GP0,(Y2<<16)+(X2&0xFFFF)               ; Write GP0  Packet Word (Vertex2)
  4 WRIOW GP0,(TEX<<16)+((V2&0xFF)<<8)+(U2&0xFF) ; Write GP0  Packet Word (Texcoord2+Texpage)
  5 WRIOW GP0,(Y3<<16)+(X3&0xFFFF)               ; Write GP0  Packet Word (Vertex3)
  6 WRIOW GP0,((V3&0xFF)<<8)+(U3&0xFF)           ; Write GP0  Packet Word (Texcoord3)     */
    V0.xy_from_cmd(CMD[1]);
    V1.xy_from_cmd(CMD[3]);
    V2.xy_from_cmd(CMD[5]);
    V0.uv_from_cmd(CMD[2]);
    V1.uv_from_cmd(CMD[4]);
    V2.uv_from_cmd(CMD[6]);
    u16 texpage = CMD[4] >> 16;
    u16 palette = CMD[2] >> 16;
    u32 color = CMD[0] & 0xFFFFFF;
    TEXTURE_SAMPLER ts{};
    get_texture_sampler_from_texpage_and_palette(texpage, palette, &ts);
    RT_draw_flat_tex_triangle_modulated(&V0, &V1, &V2, color, &ts);
}

void core::cmd25_tri_raw()
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
    V0.xy_from_cmd(CMD[1]);
    V1.xy_from_cmd(CMD[3]);
    V2.xy_from_cmd(CMD[5]);
    V0.uv_from_cmd(CMD[2]);
    V1.uv_from_cmd(CMD[4]);
    V2.uv_from_cmd(CMD[6]);
    u16 texpage = CMD[4] >> 16;
    u16 palette = CMD[2] >> 16;
    TEXTURE_SAMPLER ts;
    get_texture_sampler_from_texpage_and_palette(texpage, palette, &ts);
    RT_draw_flat_tex_triangle(&V0, &V1, &V2, &ts);
}

void core::cmd26_tri_modulated_semi_transparent()
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
    V0.xy_from_cmd(CMD[1]);
    V1.xy_from_cmd(CMD[3]);
    V2.xy_from_cmd(CMD[5]);
    V0.uv_from_cmd(CMD[2]);
    V1.uv_from_cmd(CMD[4]);
    V2.uv_from_cmd(CMD[6]);
    u16 texpage = CMD[4] >> 16;
    u16 palette = CMD[2] >> 16;
    u32 color = CMD[0] & 0xFFFFFF;
    TEXTURE_SAMPLER ts;
    get_texture_sampler_from_texpage_and_palette(texpage, palette, &ts);
    RT_draw_flat_tex_triangle_modulated_semi(&V0, &V1, &V2, color, &ts);
}

void core::cmd27_tri_raw_semi_transparent()
{
    /*
  0 WRIOW GP0,(0x27<<24)                         ; Write GP0 Command Word (Command)
  1 WRIOW GP0,(Y1<<16)+(X1&0xFFFF)               ; Write GP0  Packet Word (Vertex1)
  2 WRIOW GP0,(PAL<<16)+((V1&0xFF)<<8)+(U1&0xFF) ; Write GP0  Packet Word (Texcoord1+Palette)
  3 WRIOW GP0,(Y2<<16)+(X2&0xFFFF)               ; Write GP0  Packet Word (Vertex2)
  4 WRIOW GP0,(TEX<<16)+((V2&0xFF)<<8)+(U2&0xFF) ; Write GP0  Packet Word (Texcoord2+Texpage)
  5 WRIOW GP0,(Y3<<16)+(X3&0xFFFF)               ; Write GP0  Packet Word (Vertex3)
  6 WRIOW GP0,((V3&0xFF)<<8)+(U3&0xFF)           ; Write GP0  Packet Word (Texcoord3)     */
    V0.xy_from_cmd(CMD[1]);
    V1.xy_from_cmd(CMD[3]);
    V2.xy_from_cmd(CMD[5]);
    V0.uv_from_cmd(CMD[2]);
    V1.uv_from_cmd(CMD[4]);
    V2.uv_from_cmd(CMD[6]);
    u16 texpage = CMD[4] >> 16;
    u16 palette = CMD[2] >> 16;
    TEXTURE_SAMPLER ts;
    get_texture_sampler_from_texpage_and_palette(texpage, palette, &ts);
    RT_draw_flat_tex_triangle_semi(&V0, &V1, &V2, &ts);
}


void core::cmd2a_quad_flat_semi_transparent()
{
    V0.xy_from_cmd(CMD[1]);
    V1.xy_from_cmd(CMD[2]);
    V2.xy_from_cmd(CMD[3]);
    V3.xy_from_cmd(CMD[4]);
    V0.color24_from_cmd(CMD[0]);

#ifdef LOG_GP0
    printf("\ncmd2a quad flat semi v0:%d,%d v1:%d,%d v2:%d,%d v3:%d,%d color:%04x", v0.x, v0.y, v1.x, v1.y, v2.x, v2.y, v3.x, v3.y, color);
#endif
    RT_draw_flat_triangle_semi(&V0, &V1, &V2, V0.r, V0.g, V0.b);
    RT_draw_flat_triangle_semi(&V1, &V2, &V3, V0.r, V0.g, V0.b);
}


void core::cmd2c_quad_opaque_flat_textured_modulated() {
    // 0 WRIOW GP0,(0x2C<<24)+(COLOR&0xFFFFFF)        ; Write GP0 Command Word (Color+Command)
    // 1 WRIOW GP0,(Y1<<16)+(X1&0xFFFF)               ; Write GP0  Packet Word (Vertex1)
    // 2 WRIOW GP0,(PAL<<16)+((V1&0xFF)<<8)+(U1&0xFF) ; Write GP0  Packet Word (Texcoord1+Palette)
    // 3 WRIOW GP0,(Y2<<16)+(X2&0xFFFF)               ; Write GP0  Packet Word (Vertex2)
    // 4 WRIOW GP0,(TEX<<16)+((V2&0xFF)<<8)+(U2&0xFF) ; Write GP0  Packet Word (Texcoord2+Texpage)
    // 5 WRIOW GP0,(Y3<<16)+(X3&0xFFFF)               ; Write GP0  Packet Word (Vertex3)
    // 6 WRIOW GP0,((V3&0xFF)<<8)+(U3&0xFF)           ; Write GP0  Packet Word (Texcoord3)
    // 7 WRIOW GP0,(Y4<<16)+(X4&0xFFFF)               ; Write GP0  Packet Word (Vertex4)
    // 8 WRIOW GP0,((V4&0xFF)<<8)+(U4&0xFF)           ; Write GP0  Packet Word (Texcoord4)
    T1.xy_from_cmd(CMD[1]);
    T2.xy_from_cmd(CMD[3]);
    T3.xy_from_cmd(CMD[5]);
    T4.xy_from_cmd(CMD[7]);
    T1.uv_from_cmd(CMD[2]);
    T2.uv_from_cmd(CMD[4]);
    T3.uv_from_cmd(CMD[6]);
    T4.uv_from_cmd(CMD[8]);

    u32 palette = CMD[2] >> 16;
    u32 tx_page = CMD[4] >> 16;
    u32 col = CMD[0] & 0xFFFFFF;

    TEXTURE_SAMPLER ts;
    get_texture_sampler_from_texpage_and_palette(tx_page, palette, &ts);

    RT_draw_flat_tex_triangle_modulated(&T1, &T2, &T3, col, &ts);
    RT_draw_flat_tex_triangle_modulated(&T2, &T3, &T4, col, &ts);
}

void core::cmd2d_quad_opaque_flat_textured() {
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
    T1.xy_from_cmd(CMD[1]);
    T2.xy_from_cmd(CMD[3]);
    T3.xy_from_cmd(CMD[5]);
    T4.xy_from_cmd(CMD[7]);

    T1.uv_from_cmd(CMD[2]);
    T2.uv_from_cmd(CMD[4]);
    T3.uv_from_cmd(CMD[6]);
    T4.uv_from_cmd(CMD[8]);

    u32 palette = CMD[2] >> 16;
    u32 tx_page = CMD[4] >> 16;

    TEXTURE_SAMPLER ts;
    get_texture_sampler_from_texpage_and_palette(tx_page, palette, &ts);

    RT_draw_flat_tex_triangle(&T1, &T2, &T3, &ts);
    RT_draw_flat_tex_triangle(&T2, &T3, &T4, &ts);
}

void core::cmd2e_quad_flat_textured_modulated() {
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
    T1.xy_from_cmd(CMD[1]);
    T2.xy_from_cmd(CMD[3]);
    T3.xy_from_cmd(CMD[5]);
    T4.xy_from_cmd(CMD[7]);
    T1.uv_from_cmd(CMD[2]);
    T2.uv_from_cmd(CMD[4]);
    T3.uv_from_cmd(CMD[6]);
    T4.uv_from_cmd(CMD[8]);

    u32 col = CMD[0] & 0xFFFFFF;
    u32 palette = CMD[2] >> 16;
    u32 tx_page = CMD[4] >> 16;

    TEXTURE_SAMPLER ts;
    get_texture_sampler_from_texpage_and_palette(tx_page, palette, &ts);

    RT_draw_flat_tex_triangle_modulated_semi(&T1, &T2, &T3, col, &ts);
    RT_draw_flat_tex_triangle_modulated_semi(&T2, &T3, &T4, col, &ts);
}

void core::cmd2f_quad_flat_textured_semi() {
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
    T1.xy_from_cmd(CMD[1]);
    T2.xy_from_cmd(CMD[3]);
    T3.xy_from_cmd(CMD[5]);
    T4.xy_from_cmd(CMD[7]);
    T1.uv_from_cmd(CMD[2]);
    T2.uv_from_cmd(CMD[4]);
    T3.uv_from_cmd(CMD[6]);
    T4.uv_from_cmd(CMD[8]);

    u32 palette = CMD[2] >> 16;
    u32 tx_page = CMD[4] >> 16;

    TEXTURE_SAMPLER ts;
    get_texture_sampler_from_texpage_and_palette(tx_page, palette, &ts);

    RT_draw_flat_tex_triangle_semi(&T1, &T2, &T3, &ts);
    RT_draw_flat_tex_triangle_semi(&T2, &T3, &T4, &ts);
}


void core::cmd30_tri_shaded_opaque()
{
    // 0 WRIOW GP0,(0x30<<24)+(COLOR1&0xFFFFFF)       ; Write GP0 Command Word (Color1+Command)
    // 1 WRIOW GP0,(Y1<<16)+(X1&0xFFFF)               ; Write GP0  Packet Word (Vertex1)
    // 2 WRIOW GP0,(COLOR2&0xFFFFFF)                  ; Write GP0  Packet Word (Color2)
    // 3 WRIOW GP0,(Y2<<16)+(X2&0xFFFF)               ; Write GP0  Packet Word (Vertex2)
    // 4 WRIOW GP0,(COLOR3&0xFFFFFF)                  ; Write GP0  Packet Word (Color3)
    // 5 WRIOW GP0,(Y3<<16)+(X3&0xFFFF)               ; Write GP0  Packet Word (Vertex3)
    V1.xy_from_cmd(CMD[1]);
    V2.xy_from_cmd(CMD[3]);
    V3.xy_from_cmd(CMD[5]);
    V1.color24_from_cmd(CMD[0]);
    V2.color24_from_cmd(CMD[2]);
    V3.color24_from_cmd(CMD[4]);
#ifdef LOG_GP0
    printf("\ntri_shaded_opaque ");
    print_v(&V1);
    print_v(&V2);
    print_v(&V3);
#endif
    RT_draw_shaded_triangle(&V1, &V2, &V3);
}

void core::cmd34_tri_shaded_opaque_tex_modulated()
{
    // 0 WRIOW GP0,(0x34<<24)+(COLOR1&0xFFFFFF)       ; Write GP0 Command Word (Color1+Command)
    // 1 WRIOW GP0,(Y1<<16)+(X1&0xFFFF)               ; Write GP0  Packet Word (Vertex1)
    // 2 WRIOW GP0,(PAL<<16)+((V1&0xFF)<<8)+(U1&0xFF) ; Write GP0  Packet Word (Texcoord1+Palette)
    // 3 WRIOW GP0,(COLOR2&0xFFFFFF)                  ; Write GP0  Packet Word (Color2)
    // 4 WRIOW GP0,(Y2<<16)+(X2&0xFFFF)               ; Write GP0  Packet Word (Vertex2)
    // 5 WRIOW GP0,(TEX<<16)+((V2&0xFF)<<8)+(U2&0xFF) ; Write GP0  Packet Word (Texcoord2+Texpage)
    // 6 WRIOW GP0,(COLOR3&0xFFFFFF)                  ; Write GP0  Packet Word (Color3)
    // 7 WRIOW GP0,(Y3<<16)+(X3&0xFFFF)               ; Write GP0  Packet Word (Vertex3)
    // 8 WRIOW GP0,((V3&0xFF)<<8)+(U3&0xFF)           ; Write GP0  Packet Word (Texcoord    V1.xy_from_cmd(CMD[1]);
    V1.xy_from_cmd(CMD[1]);
    V2.xy_from_cmd(CMD[4]);
    V3.xy_from_cmd(CMD[7]);
    V1.color24_from_cmd(CMD[0]);
    V2.color24_from_cmd(CMD[3]);
    V3.color24_from_cmd(CMD[6]);
    V1.uv_from_cmd(CMD[2]);
    V2.uv_from_cmd(CMD[5]);
    V3.uv_from_cmd(CMD[8]);
    u16 texpage = CMD[5] >> 16;
    u16 palette = CMD[2] >> 16;
    TEXTURE_SAMPLER ts;
    get_texture_sampler_from_texpage_and_palette(texpage, palette, &ts);
    RT_draw_shaded_tex_triangle_modulated(&V1, &V2, &V3, &ts);
}

void core::cmd36_tri_shaded_opaque_tex_modulated_semi()
{
    // 0 WRIOW GP0,(0x34<<24)+(COLOR1&0xFFFFFF)       ; Write GP0 Command Word (Color1+Command)
    // 1 WRIOW GP0,(Y1<<16)+(X1&0xFFFF)               ; Write GP0  Packet Word (Vertex1)
    // 2 WRIOW GP0,(PAL<<16)+((V1&0xFF)<<8)+(U1&0xFF) ; Write GP0  Packet Word (Texcoord1+Palette)
    // 3 WRIOW GP0,(COLOR2&0xFFFFFF)                  ; Write GP0  Packet Word (Color2)
    // 4 WRIOW GP0,(Y2<<16)+(X2&0xFFFF)               ; Write GP0  Packet Word (Vertex2)
    // 5 WRIOW GP0,(TEX<<16)+((V2&0xFF)<<8)+(U2&0xFF) ; Write GP0  Packet Word (Texcoord2+Texpage)
    // 6 WRIOW GP0,(COLOR3&0xFFFFFF)                  ; Write GP0  Packet Word (Color3)
    // 7 WRIOW GP0,(Y3<<16)+(X3&0xFFFF)               ; Write GP0  Packet Word (Vertex3)
    // 8 WRIOW GP0,((V3&0xFF)<<8)+(U3&0xFF)           ; Write GP0  Packet Word (Texcoord    V1.xy_from_cmd(CMD[1]);
    V1.xy_from_cmd(CMD[1]);
    V2.xy_from_cmd(CMD[4]);
    V3.xy_from_cmd(CMD[7]);
    V1.color24_from_cmd(CMD[0]);
    V2.color24_from_cmd(CMD[3]);
    V3.color24_from_cmd(CMD[6]);
    V1.uv_from_cmd(CMD[2]);
    V2.uv_from_cmd(CMD[5]);
    V3.uv_from_cmd(CMD[8]);
    u16 texpage = CMD[5] >> 16;
    u16 palette = CMD[2] >> 16;
    TEXTURE_SAMPLER ts;
    get_texture_sampler_from_texpage_and_palette(texpage, palette, &ts);

    RT_draw_shaded_tex_triangle_modulated_semi(&V1, &V2, &V3, &ts);
}

void core::cmd32_tri_shaded_semi_transparent()
{
    // 0 WRIOW GP0,(0x30<<24)+(COLOR1&0xFFFFFF)       ; Write GP0 Command Word (Color1+Command)
    // 1 WRIOW GP0,(Y1<<16)+(X1&0xFFFF)               ; Write GP0  Packet Word (Vertex1)
    // 2 WRIOW GP0,(COLOR2&0xFFFFFF)                  ; Write GP0  Packet Word (Color2)
    // 3 WRIOW GP0,(Y2<<16)+(X2&0xFFFF)               ; Write GP0  Packet Word (Vertex2)
    // 4 WRIOW GP0,(COLOR3&0xFFFFFF)                  ; Write GP0  Packet Word (Color3)
    // 5 WRIOW GP0,(Y3<<16)+(X3&0xFFFF)               ; Write GP0  Packet Word (Vertex3)
    /*struct COLOR_SAMPLER *c1 = &core::color1;
    struct COLOR_SAMPLER *c2 = &core::color2;
    struct COLOR_SAMPLER *c3 = &core::color3;*/
    V1.xy_from_cmd(CMD[1]);
    V2.xy_from_cmd(CMD[3]);
    V3.xy_from_cmd(CMD[5]);
    V1.color24_from_cmd(CMD[0]);
    V2.color24_from_cmd(CMD[2]);
    V3.color24_from_cmd(CMD[4]);
#ifdef LOG_GP0
    printf("\ntri_shaded_opaque ");
    print_v(&V1);
    print_v(&V2);
    print_v(&V3);
#endif
    RT_draw_shaded_triangle_semi(&V1, &V2, &V3);
}


void core::cmd38_quad_shaded_opaque()
{
    // WRIOW GP0,(0x38<<24)+(COLOR1&0xFFFFFF)       ; Write GP0 Command Word (Color1+Command)
    // WRIOW GP0,(Y1<<16)+(X1&0xFFFF)               ; Write GP0  Packet Word (Vertex1)
    // WRIOW GP0,(COLOR2&0xFFFFFF)                  ; Write GP0  Packet Word (Color2)
    // WRIOW GP0,(Y2<<16)+(X2&0xFFFF)               ; Write GP0  Packet Word (Vertex2)
    // WRIOW GP0,(COLOR3&0xFFFFFF)                  ; Write GP0  Packet Word (Color3)
    // WRIOW GP0,(Y3<<16)+(X3&0xFFFF)               ; Write GP0  Packet Word (Vertex3)
    // WRIOW GP0,(COLOR4&0xFFFFFF)                  ; Write GP0  Packet Word (Color4)
    // WRIOW GP0,(Y4<<16)+(X4&0xFFFF)               ; Write GP0  Packet Word (Vertex4)
    T1.color24_from_cmd(CMD[0]);
    T1.xy_from_cmd(CMD[1]);
    T2.color24_from_cmd(CMD[2]);
    T2.xy_from_cmd(CMD[3]);
    T3.color24_from_cmd(CMD[4]);
    T3.xy_from_cmd(CMD[5]);
    T4.color24_from_cmd(CMD[6]);
    T4.xy_from_cmd(CMD[7]);

#ifdef LOG_DRAW_QUADS
    printf("\nquad_shaded ");
    draw_vd(&T1);
    draw_vd(&T2);
    draw_vd(&T3);
    draw_vd(&T4);
#endif
    
    RT_draw_shaded_triangle(&T1, &T2, &T3);
    RT_draw_shaded_triangle(&T2, &T3, &T4);
}

void core::cmd3a_quad_shaded_semi_transparent()
{
    // WRIOW GP0,(0x38<<24)+(COLOR1&0xFFFFFF)       ; Write GP0 Command Word (Color1+Command)
    // WRIOW GP0,(Y1<<16)+(X1&0xFFFF)               ; Write GP0  Packet Word (Vertex1)
    // WRIOW GP0,(COLOR2&0xFFFFFF)                  ; Write GP0  Packet Word (Color2)
    // WRIOW GP0,(Y2<<16)+(X2&0xFFFF)               ; Write GP0  Packet Word (Vertex2)
    // WRIOW GP0,(COLOR3&0xFFFFFF)                  ; Write GP0  Packet Word (Color3)
    // WRIOW GP0,(Y3<<16)+(X3&0xFFFF)               ; Write GP0  Packet Word (Vertex3)
    // WRIOW GP0,(COLOR4&0xFFFFFF)                  ; Write GP0  Packet Word (Color4)
    // WRIOW GP0,(Y4<<16)+(X4&0xFFFF)               ; Write GP0  Packet Word (Vertex4)
    T1.color24_from_cmd(CMD[0]);
    T1.xy_from_cmd(CMD[1]);
    T2.color24_from_cmd(CMD[2]);
    T2.xy_from_cmd(CMD[3]);
    T3.color24_from_cmd(CMD[4]);
    T3.xy_from_cmd(CMD[5]);
    T4.color24_from_cmd(CMD[6]);
    T4.xy_from_cmd(CMD[7]);

#ifdef LOG_DRAW_QUADS
    printf("\nquad_shaded ");
    draw_vd(&T1);
    draw_vd(&T2);
    draw_vd(&T3);
    draw_vd(&T4);
#endif

    RT_draw_shaded_triangle_semi(&T1, &T2, &T3);
    RT_draw_shaded_triangle_semi(&T2, &T3, &T4);
}

void core::cmd3c_quad_opaque_shaded_textured_modulated() {
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
    T1.xy_from_cmd(CMD[1]);
    T2.xy_from_cmd(CMD[4]);
    T3.xy_from_cmd(CMD[7]);
    T4.xy_from_cmd(CMD[10]);

    T1.uv_from_cmd(CMD[2]);
    T2.uv_from_cmd(CMD[5]);
    T3.uv_from_cmd(CMD[8]);
    T4.uv_from_cmd(CMD[11]);

    T1.color24_from_cmd(CMD[0]);
    T2.color24_from_cmd(CMD[3]);
    T3.color24_from_cmd(CMD[6]);
    T4.color24_from_cmd(CMD[9]);

    u16 texpage = CMD[5] >> 16;
    u16 palette = CMD[2] >> 16;
    TEXTURE_SAMPLER ts;
    get_texture_sampler_from_texpage_and_palette(texpage, palette, &ts);

    RT_draw_shaded_tex_triangle_modulated(&T1, &T2, &T3, &ts);
    RT_draw_shaded_tex_triangle_modulated(&T2, &T3, &T4, &ts);
}

void core::cmd3e_quad_opaque_shaded_textured_modulated_semi() {
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
    T1.xy_from_cmd(CMD[1]);
    T2.xy_from_cmd(CMD[4]);
    T3.xy_from_cmd(CMD[7]);
    T4.xy_from_cmd(CMD[10]);

    T1.uv_from_cmd(CMD[2]);
    T2.uv_from_cmd(CMD[5]);
    T3.uv_from_cmd(CMD[8]);
    T4.uv_from_cmd(CMD[11]);

    T1.color24_from_cmd(CMD[0]);
    T2.color24_from_cmd(CMD[3]);
    T3.color24_from_cmd(CMD[6]);
    T4.color24_from_cmd(CMD[9]);

    u16 texpage = CMD[5] >> 16;
    u16 palette = CMD[2] >> 16;
    TEXTURE_SAMPLER ts;
    get_texture_sampler_from_texpage_and_palette(texpage, palette, &ts);

    RT_draw_shaded_tex_triangle_modulated_semi(&T1, &T2, &T3, &ts);
    RT_draw_shaded_tex_triangle_modulated_semi(&T2, &T3, &T4, &ts);
}

void core::cmd40_line_opaque() {
    // WRIOW GP0,(0x40<<24)+(COLOR&0xFFFFFF)  ; Write GP0 Command Word (Color+Command)
    // WRIOW GP0,(Y1<<16)+(X1&0xFFFF)         ; Write GP0  Packet Word (Vertex1)
    // WRIOW GP0,(Y2<<16)+(X2&0xFFFF)         ; Write GP0  Packet Word (Vertex2)
    u32 color = BGR24to15(CMD[0] & 0xFFFFFF);
    V0.xy_from_cmd(CMD[1]);
    V1.xy_from_cmd(CMD[2]);
    bresenham_opaque(&V0, &V1, color);
}

void core::cmd60_rect_opaque_flat()
{
    u32 color = BGR24to15(CMD[0] & 0xFFFFFF);
    u32 xstart = CMD[1] & 0xFFFF;
    u32 ystart = CMD[1] >> 16;
    u32 xsize = CMD[2] & 0xFFFF;
    u32 ysize = CMD[2] >> 16;

#ifdef LOG_GP0
    printf("\n60_rect_opaque_flat %d %d %d %d %06x", xstart, ystart, xsize, ysize, CMD[0] & 0xFFFFFF);
#endif

    u32 xend = (xstart + xsize);
    xend = xend > 1024 ? 1024 : xend;

    u32 yend = (ystart + ysize);
    yend = yend > 512 ? 512 : yend;

    for (u32 y = ystart; y < yend; y++) {
        for (u32 x = xstart; x < xend; x++) {
            setpix(y, x, color, 0, 0);
        }
    }
}

void core::cmd64_rect_opaque_flat_textured_modulated()
{
    // WRIOW GP0,(0x64<<24)+(COLOR&0xFFFFFF)      ; Write GP0 Command Word (Color+Command)

    // WRIOW GP0,(Y<<16)+(X&0xFFFF)               ; Write GP0  Packet Word (Vertex)
    u32 ystart = (CMD[1] & 0xFFFF0000) >> 16;
    u32 xstart = ((CMD[1] & 0xFFFF) << 16) >> 16;

    u32 col = CMD[0] & 0xFFFFFF;
    const static float mm = 1.0f / 128.0f;
    float mr = ((float)(col & 0xFF)) * mm;
    float mg = ((float)((col >> 8) & 0xFF)) * mm;
    float mb = ((float)((col >> 16) & 0xFF)) * mm;

    // WRIOW GP0,(PAL<<16)+((V&0xFF)<<8)+(U&0xFF) ; Write GP0  Packet Word (Texcoord+Palette)
    u32 clut = (CMD[2] >> 16) & 0xFFFF;
    u32 v = (CMD[2] >> 8) & 0xFF;
    u32 ustart = CMD[2] & 0xFF;

    // WRIOW GP0,(HEIGHT<<16)+(WIDTH&0xFFFF)      ; Write GP0  Packet Word (Width+Height)
    u32 height = (CMD[3] >> 16) & 0xFFFF;
    u32 width = CMD[3] & 0xFFFF;

    u32 xend = (xstart + width);
    u32 yend = (ystart + height);
    xend = xend > 1024 ? 1024 : xend;
    yend = yend > 512 ? 512 : yend;
#ifdef LOG_GP0
    printf("\nrect_opaque_flat %d %d %d %d", xstart, ystart, width, height);
#endif

    TEXTURE_SAMPLER ts;
    get_texture_sampler_from_texpage_and_palette(io.GPUSTAT, clut, &ts);
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

            setpix(y, x, lbit, 1, hbit);
            u++;
        }
        v++;
    }
}

void core::cmd65_rect_opaque_flat_textured()
{
    // 0 WRIOW GP0,(0x64<<24)+(COLOR&0xFFFFFF)      ; Write GP0 Command Word (Color+Command)
    // 1 WRIOW GP0,(Y<<16)+(X&0xFFFF)               ; Write GP0  Packet Word (Vertex)
    // 2 WRIOW GP0,(PAL<<16)+((V&0xFF)<<8)+(U&0xFF) ; Write GP0  Packet Word (Texcoord+Palette)
    // 3 WRIOW GP0,(HEIGHT<<16)+(WIDTH&0xFFFF)      ; Write GP0  Packet Word (Width+Height)

    u32 ystart = (CMD[1] & 0xFFFF0000) >> 16;
    u32 xstart = ((CMD[1] & 0xFFFF) << 16) >> 16;

    u32 clut = (CMD[2] >> 16) & 0xFFFF;
    u32 v = (CMD[2] >> 8) & 0xFF;
    u32 ustart = CMD[2] & 0xFF;

    u32 height = (CMD[3] >> 16) & 0xFFFF;
    u32 width = CMD[3] & 0xFFFF;

    u32 xend = (xstart + width);
    u32 yend = (ystart + height);
    xend = xend > 1024 ? 1024 : xend;
    yend = yend > 512 ? 512 : yend;
#ifdef LOG_GP0
    printf("\nrect_opaque_flat %d %d %d %d", xstart, ystart, width, height);
#endif

    TEXTURE_SAMPLER ts;
    //get_texture_sampler_clut(texture_depth, page_base_x, page_base_y, clut, &ts);
    get_texture_sampler_from_texpage_and_palette(io.GPUSTAT, clut, &ts);
    for (u32 y = ystart; y < yend; y++) {
        u32 u = ustart;
        for (u32 x = xstart; x < xend; x++) {
            u16 color = ts.sample(&ts, u, v);
            u32 hbit = color & 0x8000;
            u32 lbit = color & 0x7FFF;
            setpix(y, x, lbit, 1, hbit);
            u++;
        }
        v++;
    }
}

void core::cmd66_rect_semi_flat_textured_modulated()
{
    // WRIOW GP0,(0x64<<24)+(COLOR&0xFFFFFF)      ; Write GP0 Command Word (Color+Command)

    // WRIOW GP0,(Y<<16)+(X&0xFFFF)               ; Write GP0  Packet Word (Vertex)
    u32 ystart = (CMD[1] & 0xFFFF0000) >> 16;
    u32 xstart = ((CMD[1] & 0xFFFF) << 16) >> 16;

    u32 col = CMD[0] & 0xFFFFFF;
    const static float mm = 1.0f / 128.0f;
    float mr = ((float)(col & 0xFF)) * mm;
    float mg = ((float)((col >> 8) & 0xFF)) * mm;
    float mb = ((float)((col >> 16) & 0xFF)) * mm;

    // WRIOW GP0,(PAL<<16)+((V&0xFF)<<8)+(U&0xFF) ; Write GP0  Packet Word (Texcoord+Palette)
    u32 clut = (CMD[2] >> 16) & 0xFFFF;
    u32 v = (CMD[2] >> 8) & 0xFF;
    u32 ustart = CMD[2] & 0xFF;

    // WRIOW GP0,(HEIGHT<<16)+(WIDTH&0xFFFF)      ; Write GP0  Packet Word (Width+Height)
    u32 height = (CMD[3] >> 16) & 0xFFFF;
    u32 width = CMD[3] & 0xFFFF;

    u32 xend = (xstart + width);
    u32 yend = (ystart + height);
    xend = xend > 1024 ? 1024 : xend;
    yend = yend > 512 ? 512 : yend;
#ifdef LOG_GP0
    printf("\nrect_opaque_flat %d %d %d %d", xstart, ystart, width, height);
#endif

    TEXTURE_SAMPLER ts;
    get_texture_sampler_from_texpage_and_palette(io.GPUSTAT, clut, &ts);
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

            semipix(y, x, lbit, 1, hbit);
            u++;
        }
        v++;
    }
}


void core::cmd67_rect_semi_flat_textured()
{
    // 0 WRIOW GP0,(0x64<<24)+(COLOR&0xFFFFFF)      ; Write GP0 Command Word (Color+Command)
    // 1 WRIOW GP0,(Y<<16)+(X&0xFFFF)               ; Write GP0  Packet Word (Vertex)
    // 2 WRIOW GP0,(PAL<<16)+((V&0xFF)<<8)+(U&0xFF) ; Write GP0  Packet Word (Texcoord+Palette)
    // 3 WRIOW GP0,(HEIGHT<<16)+(WIDTH&0xFFFF)      ; Write GP0  Packet Word (Width+Height)

    u32 ystart = (CMD[1] & 0xFFFF0000) >> 16;
    u32 xstart = ((CMD[1] & 0xFFFF) << 16) >> 16;

    u32 clut = (CMD[2] >> 16) & 0xFFFF;
    u32 v = (CMD[2] >> 8) & 0xFF;
    u32 ustart = CMD[2] & 0xFF;

    u32 height = (CMD[3] >> 16) & 0xFFFF;
    u32 width = CMD[3] & 0xFFFF;

    u32 xend = (xstart + width);
    u32 yend = (ystart + height);
    xend = xend > 1024 ? 1024 : xend;
    yend = yend > 512 ? 512 : yend;
#ifdef LOG_GP0
    printf("\nrect_opaque_flat %d %d %d %d", xstart, ystart, width, height);
#endif

    TEXTURE_SAMPLER ts;
    //get_texture_sampler_clut(texture_depth, page_base_x, page_base_y, clut, &ts);
    get_texture_sampler_from_texpage_and_palette(io.GPUSTAT, clut, &ts);
    for (u32 y = ystart; y < yend; y++) {
        u32 u = ustart;
        for (u32 x = xstart; x < xend; x++) {
            u16 color = ts.sample(&ts, u, v);
            u32 hbit = color & 0x8000;
            u32 lbit = color & 0x7FFF;
            semipix(y, x, lbit, 1, hbit);
            u++;
        }
        v++;
    }
}

void core::cmd68_rect_1x1()
{
    u32 y = (CMD[1] & 0xFFFF0000) >> 16;
    u32 x = ((CMD[1] & 0xFFFF) << 16) >> 16;
    u32 color = BGR24to15(CMD[0] & 0xFFFFFF);
    setpix(y, x, color, 0, 0);
}

void core::cmd6d_rect_1x1_tex()
{
    u32 y = (CMD[1] & 0xFFFF0000) >> 16;
    u32 x = ((CMD[1] & 0xFFFF) << 16) >> 16;
    u32 v = (CMD[2] & 0xFF00) >> 8;
    u32 u = (CMD[2] & 0xFF);
    u32 palette = (CMD[2] >> 16);
    TEXTURE_SAMPLER ts;
    get_texture_sampler_from_texpage_and_palette(io.GPUSTAT, palette, &ts);
    u32 color = ts.sample(&ts, u, v);
    setpix(y, x, color & 0x7FFF, 1, color & 0x8000);
}

void core::cmd6c_rect_1x1_tex_modulated()
{
    u32 y = (CMD[1] & 0xFFFF0000) >> 16;
    u32 x = ((CMD[1] & 0xFFFF) << 16) >> 16;
    u32 v = (CMD[2] & 0xFF00) >> 8;
    u32 u = (CMD[2] & 0xFF);
    u32 palette = (CMD[2] >> 16);
    TEXTURE_SAMPLER ts;
    get_texture_sampler_from_texpage_and_palette(io.GPUSTAT, palette, &ts);
    u32 color = ts.sample(&ts, u, v);
    u32 hbit = color & 0x8000;

    u32 tc = CMD[0] & 0xFFFFFF;
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

    setpix(y, x, color & 0x7FFF, 1, hbit);
}

void core::cmd6e_rect_1x1_tex_semi_modulated()
{
    u32 y = (CMD[1] & 0xFFFF0000) >> 16;
    u32 x = ((CMD[1] & 0xFFFF) << 16) >> 16;
    u32 v = (CMD[2] & 0xFF00) >> 8;
    u32 u = (CMD[2] & 0xFF);
    u32 palette = (CMD[2] >> 16);
    TEXTURE_SAMPLER ts;
    get_texture_sampler_from_texpage_and_palette(io.GPUSTAT, palette, &ts);
    u32 color = ts.sample(&ts, u, v);
    u32 hbit = color & 0x8000;

    u32 tc = CMD[0] & 0xFFFFFF;
    static const float mm = 1.0f / 128.0f;
    float rm = (float)(tc & 0xFF) * mm;
    float gm = (float)((tc >> 8) & 0xFF) * mm;
    float bm = (float)((tc >> 16) & 0xFF) * mm;

    float r = (float)(color & 0x1F) * rm;
    float g = (float)((color >> 5) & 0x1F) * gm;
    float b = (float)((color >> 10) & 0x1F) * bm;

    color = ((u32)r) | ((u32)g << 5) | ((u32)b << 10);

    semipix(y, x, color & 0x7FFF, 1, hbit);
}


void core::cmd6f_rect_1x1_tex_semi()
{
    u32 y = (CMD[1] & 0xFFFF0000) >> 16;
    u32 x = ((CMD[1] & 0xFFFF) << 16) >> 16;
    u32 v = (CMD[2] & 0xFF00) >> 8;
    u32 u = (CMD[2] & 0xFF);
    u32 palette = (CMD[2] >> 16);
    TEXTURE_SAMPLER ts;
    get_texture_sampler_from_texpage_and_palette(io.GPUSTAT, palette, &ts);
    u32 color = ts.sample(&ts, u, v);
    semipix(y, x, color & 0x7FFF, 1, color & 0x8000);
}


void core::cmd80_vram_copy()
{
    u32 y1 = CMD[1] >> 16;
    u32 x1 = CMD[1] & 0xFFFF;
    u32 y2 = CMD[2] >> 16;
    u32 x2 = CMD[2] & 0xFFFF;
    u32 height = CMD[3] >> 16;
    u32 width = CMD[3] & 0xFFFF;
#ifdef LOG_GP0
    printf("\nGP0 cmd80 copy from %d,%d to %d,%d size:%d,%d", x1, y1, x2, y2, width, height);
#endif

    for (u32 iy = 0; iy < height; iy++) {
        for (u32 ix = 0; ix < width; ix++) {
            u32 get_addr = ((ix+x1) & 1023) * 2;
            get_addr += ((iy+y1) & 511) * 2048; // 2048 bytes per 1024-pixel row
            if (preserve_masked_pixels) {
                u16 t = cR16(VRAM, (((iy + y2) & 511) * 2048) + (((ix + x2) & 1023) * 2));
                if (t & 0x8000) continue;
            }

            u16 v = cR16(VRAM, get_addr & 0xFFFFF);
            setpix(iy+y2, ix+x2, v & 0x7FFF, 1, v & 0x8000);
        }
    }

    //printf("\nCOPY VRAM %d,%d to %d,%d size:%d,%d")
}

void core::rect_opaque_flat_textured_modulated_xx(u32 wh)
{
    // WRIOW GP0,(0x64<<24)+(COLOR&0xFFFFFF)      ; Write GP0 Command Word (Color+Command)
    //let color24 = CMD[0] & 0xFFFFFF;

    // WRIOW GP0,(Y<<16)+(X&0xFFFF)               ; Write GP0  Packet Word (Vertex)
    u32 ystart = (CMD[1] & 0xFFFF0000) >> 16;
    u32 xstart = ((CMD[1] & 0xFFFF) << 16) >> 16;

    // WRIOW GP0,(PAL<<16)+((V&0xFF)<<8)+(U&0xFF) ; Write GP0  Packet Word (Texcoord+Palette)
    u32 clut = (CMD[2] >> 16) & 0xFFFF;
    u32 v = (CMD[2] >> 8) & 0xFF;
    u32 ustart = CMD[2] & 0xFF;

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

    u32 tc = CMD[0] & 0xFFFFFF;
    static const float mm = 1.0f / 128.0f;
    float rm = (float)(tc & 0xFF) * mm;
    float gm = (float)((tc >> 8) & 0xFF) * mm;
    float bm = (float)((tc >> 16) & 0xFF) * mm;

    TEXTURE_SAMPLER ts;
    get_texture_sampler_from_texpage_and_palette(io.GPUSTAT, clut, &ts);
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

            setpix_split(y, x, (u32)r, (u32)g, (u32)b, 1, hbit);
            u++;
        }
        v++;
    }
}

void core::cmd7c_rect_opaque_flat_textured_modulated_16x16()
{
    rect_opaque_flat_textured_modulated_xx(16);
}

void core::cmd74_rect_opaque_flat_textured_modulated_8x8()
{
    rect_opaque_flat_textured_modulated_xx(8);
}

void core::rect_opaque_flat_textured_xx(u32 wh)
{
    // WRIOW GP0,(0x64<<24)+(COLOR&0xFFFFFF)      ; Write GP0 Command Word (Color+Command)
    //let color24 = CMD[0] & 0xFFFFFF;

    // WRIOW GP0,(Y<<16)+(X&0xFFFF)               ; Write GP0  Packet Word (Vertex)
    u32 ystart = (CMD[1] & 0xFFFF0000) >> 16;
    u32 xstart = ((CMD[1] & 0xFFFF) << 16) >> 16;

    // WRIOW GP0,(PAL<<16)+((V&0xFF)<<8)+(U&0xFF) ; Write GP0  Packet Word (Texcoord+Palette)
    u32 clut = (CMD[2] >> 16) & 0xFFFF;
    u32 v = (CMD[2] >> 8) & 0xFF;
    u32 ustart = CMD[2] & 0xFF;

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

    TEXTURE_SAMPLER ts;
    get_texture_sampler_from_texpage_and_palette(io.GPUSTAT, clut, &ts);
    for (u32 y = ystart; y < yend; y++) {
        u32 u = ustart;
        for (u32 x = xstart; x < xend; x++) {
            u16 color = ts.sample(&ts, u, v);
            u32 hbit = color & 0x8000;
            u32 lbit = color & 0x7FFF;
            setpix(y, x, lbit, 1, hbit);
            u++;
        }
        v++;
    }
}

void core::cmd75_rect_opaque_flat_textured_8x8()
{
    rect_opaque_flat_textured_xx(8);
}

void core::cmd7d_rect_opaque_flat_textured_16x16()
{
    rect_opaque_flat_textured_xx(16);
}


void core::rect_semi_flat_textured_modulated_xx(u32 wh)
{
    // WRIOW GP0,(0x64<<24)+(COLOR&0xFFFFFF)      ; Write GP0 Command Word (Color+Command)
    //let color24 = CMD[0] & 0xFFFFFF;

    // WRIOW GP0,(Y<<16)+(X&0xFFFF)               ; Write GP0  Packet Word (Vertex)
    u32 ystart = (CMD[1] & 0xFFFF0000) >> 16;
    u32 xstart = ((CMD[1] & 0xFFFF) << 16) >> 16;

    // WRIOW GP0,(PAL<<16)+((V&0xFF)<<8)+(U&0xFF) ; Write GP0  Packet Word (Texcoord+Palette)
    u32 clut = (CMD[2] >> 16) & 0xFFFF;
    u32 v = (CMD[2] >> 8) & 0xFF;
    u32 ustart = CMD[2] & 0xFF;

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

    u32 tc = CMD[0] & 0xFFFFFF;
    static const float mm = 1.0f / 128.0f;
    float rm = (float)(tc & 0xFF) * mm;
    float gm = (float)((tc >> 8) & 0xFF) * mm;
    float bm = (float)((tc >> 16) & 0xFF) * mm;

    TEXTURE_SAMPLER ts;
    get_texture_sampler_from_texpage_and_palette(io.GPUSTAT, clut, &ts);
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

            semipix_split(y, x, (u32)r, (u32)g, (u32)b, 1, hbit);
            u++;
        }
        v++;
    }
}

void core::cmd76_rect_semi_flat_textured_modulated_8x8()
{
    rect_semi_flat_textured_modulated_xx(8);
}

void core::cmd7e_rect_semi_flat_textured_modulated_16x16()
{
    rect_semi_flat_textured_modulated_xx(16);
}

void core::rect_semi_flat_textured_xx(u32 wh)
{
// WRIOW GP0,(0x64<<24)+(COLOR&0xFFFFFF)      ; Write GP0 Command Word (Color+Command)
    //let color24 = CMD[0] & 0xFFFFFF;

    // WRIOW GP0,(Y<<16)+(X&0xFFFF)               ; Write GP0  Packet Word (Vertex)
    u32 ystart = (CMD[1] & 0xFFFF0000) >> 16;
    u32 xstart = ((CMD[1] & 0xFFFF) << 16) >> 16;

    // WRIOW GP0,(PAL<<16)+((V&0xFF)<<8)+(U&0xFF) ; Write GP0  Packet Word (Texcoord+Palette)
    u32 clut = (CMD[2] >> 16) & 0xFFFF;
    u32 v = (CMD[2] >> 8) & 0xFF;
    u32 ustart = CMD[2] & 0xFF;

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

    TEXTURE_SAMPLER ts;
    get_texture_sampler_from_texpage_and_palette(io.GPUSTAT, clut, &ts);
    for (u32 y = ystart; y < yend; y++) {
        u32 u = ustart;
        for (u32 x = xstart; x < xend; x++) {
            u16 color = ts.sample(&ts, u, v);
            u32 hbit = color & 0x8000;
            u32 lbit = color & 0x7FFF;
            semipix(y, x, lbit, 1, hbit);
            u++;
        }
        v++;
    }
}

void core::cmd77_rect_semi_flat_textured_8x8()
{
    rect_semi_flat_textured_xx(8);
}

void core::cmd7f_rect_semi_flat_textured_16x16()
{
    rect_semi_flat_textured_xx(16);
}

void core::load_buffer_reset(u32 x, u32 y, u32 width, u32 height)
{
    load_buffer.x = x;
    load_buffer.y = y;
    load_buffer.width = width;
    load_buffer.height = height;
    load_buffer.line_ptr = (y * 2048) + x;
    load_buffer.img_x = load_buffer.img_y = 0;
}

void core::gp0_image_save_continue() {
    u32 px = 0;
    for (u32 i = 0; i < 2; i++) {
        px >>= 16;
        u32 y = load_buffer.y+load_buffer.img_y;
        u32 x = load_buffer.x+load_buffer.img_x;
        u32 addr = (2048*y)+(x*2) & 0xFFFFF;
        u16 v = cR16(VRAM, addr);
        px |= (static_cast<u32>(v) << 16);
        load_buffer.img_x++;
        if ((x+1) >= (load_buffer.width+load_buffer.x)) {
            load_buffer.img_x = 0;
            load_buffer.img_y++;
        }
    }
    io.GPUREAD = px;
    gp0_transfer_remaining--;
    if (gp0_transfer_remaining == 0) {
        current_ins = nullptr;
        handle_gp0 = &core::gp0_cmd;
        ready_cmd();
        unready_vram_to_CPU();
    }
}

void core::gp0_image_load_continue(u32 cmd)
{
    /*recv_gp0[recv_gp0_len] = cmd;
    recv_gp0_len++;
    if (recv_gp0_len >= (1024*1024)) {
        printf("\nWARNING GP0 TRANSFER OVERFLOW!");
        recv_gp0_len--;
    }*/
    // Put in 2 16-bit pixels
    //console.log('TRANSFERRING!', gp0_transfer_remaining);
    for (u32 i = 0; i < 2; i++) {
        u32 px = cmd & 0xFFFF;
        cmd >>= 16;
        u32 y = load_buffer.y+load_buffer.img_y;
        u32 x = load_buffer.x+load_buffer.img_x;
        u32 addr = (2048*y)+(x*2) & 0xFFFFF;
        u32 draw_it = 1;
        if (preserve_masked_pixels) {
            u16 v = cR16(VRAM, addr);
            if (v & 0x8000) draw_it = 0;
        }
        //if (!((px & 0x8000) || ((px & 0x7FFF) != 0))) draw_it = 0;
        if (draw_it) {
            cW16(VRAM, addr, px);
        }

        load_buffer.img_x++;
        if ((x+1) >= (load_buffer.width+load_buffer.x)) {
            load_buffer.img_x = 0;
            load_buffer.img_y++;
        }
    }
    gp0_transfer_remaining--;
    if (gp0_transfer_remaining == 0) {
        //printf("\nTRANSFER COMPLETE!");
        current_ins = nullptr;
        handle_gp0 = &core::gp0_cmd;
        ready_cmd();
        unready_recv_dma();
    }
}

void core::gp0_image_save_start() {
    unready_cmd();
    ready_vram_to_CPU();
    u32 x = CMD[1] & 1023;
    u32 y = (CMD[1] > 16) & 511;
    u32 width = CMD[2] & 0xFFFF;
    u32 height = (CMD[2] >> 16) & 0xFFFF;

    // Get imgsize, round it
    u32 imgsize = ((width * height) + 1) & 0xFFFFFFFE;
    gp0_transfer_remaining = imgsize/2;
    if (gp0_transfer_remaining > 0) {
        VRAM_to_CPU_in_progress = true;
        load_buffer_reset(x, y, width, height);
        gp0_image_save_continue();
    } else {
        printf("\nBad size image save: 0?");
        current_ins = nullptr;
    }

}

void core::gp0_image_load_start()
{
    unready_cmd();
    ready_recv_dma();
    // Top-left corner in VRAM
    u32 x = CMD[1] & 1023;
    u32 y = (CMD[1] >> 16) & 511;

    // Resolution
    u32 width = CMD[2] & 0xFFFF;
    u32 height = (CMD[2] >> 16) & 0xFFFF;

    // Get imgsize, round it
    u32 imgsize = ((width * height) + 1) & 0xFFFFFFFE;
    //printf("\nNEW TRANSFER x,y:%d,%d width,height:%d,%d", x, y, width, height);

    gp0_transfer_remaining = imgsize/2;
#ifdef LOG_GP0
    printf("\nTRANSFER IMGSIZE %d X:%d Y:%d WIDTH:%d HEIGHT:%d CMD:%08x", imgsize, x, y, width, height, CMD[0]);
#endif
    if (gp0_transfer_remaining > 0) {
        load_buffer_reset(x, y, width, height);
        handle_gp0 = &core::gp0_image_load_continue;
    } else {
        printf("\nBad size image load: 0?");
        current_ins = nullptr;
    }
}

void core::gp0_cmd_unhandled()
{

}

void core::GPUSTAT_update()
{
    u32 o = page_base_x;
    o |= (page_base_y) << 4;
    o |= (semi_transparency) << 5;
    o |= (texture_depth) << 7;
    o |= (dithering) << 9;
    o |= (draw_to_display) << 10;
    o |= (force_set_mask_bit) >> 4;
    o |= (preserve_masked_pixels) << 12;
    o |= (field) << 14;
    o |= (texture_disable) << 15;
    o |= hres << 16;
    o |= vres << 19;
    o |= vmode << 20;
    o |= display_depth << 21;
    o |= interlaced << 22;
    o |= display_disabled << 23;
    o |= irq1 << 24;
    o |= dma_direction << 29;

    u32 dmar;
    switch(dma_direction) {
        // 0 if FIFO full, 1 otherwise
        case e_dma_off:
            dmar = 0;
            break;
        case e_dma_fifo:
            dmar = 0; // set during read on other side
            break;
        case e_dma_cpu_to_gp0:
            dmar = (o >> 28) & 1;
            break;
        case e_dma_vram_to_cpu:
            dmar = (o >> 27) & 1;
            break;
    }

    o |= (dmar << 25);

    // Preserve GPU ready or not bits
    io.GPUSTAT = o | (io.GPUSTAT & 0x1C000000);
}

void core::new_frame() {
    io.frame ^= 1;
}

void core::write_gp0(u32 cmd) {
    (this->*handle_gp0)(cmd);
}

void core::gp0_cmd(u32 cmd) {
    gp0_buffer[recv_gp0_len] = cmd;
    recv_gp0_len++;
    assert(recv_gp0_len < 256);

    // Check if we have an instruction..
    if (current_ins) {
        CMD[cmd_arg_index++] = cmd;
        assert(cmd_arg_index<32);
        if (cmd_arg_index == cmd_arg_num) {
            // Execute instruction!
            (this->*current_ins)();
            current_ins = nullptr;
            recv_gp0_len = 0;
            cmd_arg_index = 0;
        }
    } else {
        CMD[0] = cmd;
        cmd_arg_index = 1;
        ins_special = 0;
        cmd_arg_num = 1;
        u32 cmdr = cmd >> 24;
#ifdef LOG_GP0
        printf("\nCMD %02x", cmdr);
#endif
        switch(cmdr) {
            case 0: // NOP
                //if (cmd != 0) printf("\nINTERPRETED AS NOP:%08x", cmd);
                break;
            case 0x01: // Clear cache (not implemented)
                break;
            case 0x02: // Quick Rectangle
                //console.log('Quick rectangle!');
                current_ins = &core::cmd02_quick_rect;
                cmd_arg_num = 3;
                break;
                //case 0x21: // ??
                //    console.log('NOT IMPLEMENT 0x21');
                //    break;
            case 0x1F: // Set IRQ1
                printf("\nIRQ1 trigger");
                irq1 = 1;
                bus->set_irq(IRQ_GPU, 1);
                GPUSTAT_update();
                break;
            case 0x20: // flat-shaded opaque triangle
                current_ins = &core::cmd20_tri_flat;
                cmd_arg_num = 4;
                break;
            case 0x22: // flat-shaded opaque triangle, semi-transparent
                current_ins = &core::cmd22_tri_flat_semi_transparent;
                cmd_arg_num = 4;
                break;
            case 0x24: // flat-shaded opaque triangle, semi-transparent, textured, modulated
                current_ins = &core::cmd24_tri_raw_modulated;
                cmd_arg_num = 7;
                break;
            case 0x25: // flat-shaded triangle, opaque, "raw"
                current_ins = &core::cmd25_tri_raw;
                cmd_arg_num = 7;
                break;
            case 0x26: // flat-shaded triangle, opaque, modulated, semi-transparent
                current_ins = &core::cmd26_tri_modulated_semi_transparent;
                cmd_arg_num = 7;
                break;
            case 0x27: // Textured Triangle, Semi-Transparent, Raw-Texture
                current_ins = &core::cmd27_tri_raw_semi_transparent;
                cmd_arg_num = 7;
                break;
            case 0x28: // flat-shaded rectangle
                current_ins = &core::cmd28_draw_flat4untex;
                cmd_arg_num = 5;
                break;
            case 0x2A: // semi-transparent monochrome quad
                current_ins = &core::cmd2a_quad_flat_semi_transparent;
                cmd_arg_num = 5;
                break;
            case 0x2C: // polygon, 4 points, textured, flat, modulated
                current_ins = &core::cmd2c_quad_opaque_flat_textured_modulated;
                cmd_arg_num = 9;
                break;
            case 0x2D: // textured quad opaque raw
                current_ins = &core::cmd2d_quad_opaque_flat_textured;
                cmd_arg_num = 9;
                break;
            case 0x2E: // textured quad, semi-transparent, modulated
                current_ins = &core::cmd2e_quad_flat_textured_modulated;
                cmd_arg_num = 9;
                break;
            case 0x2F: // textured quad, semi-transparent, raw tex
                current_ins = &core::cmd2f_quad_flat_textured_semi;
                cmd_arg_num = 9;
                break;
            case 0x30: // opaque shaded trinalge
                current_ins = &core::cmd30_tri_shaded_opaque;
                cmd_arg_num = 6;
                break;
            case 0x32: // shaded triangle semi-transparent
                current_ins = &core::cmd32_tri_shaded_semi_transparent;
                cmd_arg_num = 6;
                break;
            case 0x34: // opaque shaded textured triangle modulated
                current_ins = &core::cmd34_tri_shaded_opaque_tex_modulated;
                cmd_arg_num = 9;
                break;
            case 0x36: // opaque shaded textured semi-transparent triangle modulated
                current_ins = &core::cmd36_tri_shaded_opaque_tex_modulated_semi;
                cmd_arg_num = 9;
                break;
            case 0x38: // polygon, 4 points, gouraud-shaded
                current_ins = &core::cmd38_quad_shaded_opaque;
                cmd_arg_num = 8;
                break;
            case 0x3A: // polygon, 4 points, goraud-shaded, semi-transparent
                current_ins = &core::cmd3a_quad_shaded_semi_transparent;
                cmd_arg_num = 8;
                break;
            case 0x3C: // polygon, 4 points, goraud-shaded, textured & modulated
                current_ins = &core::cmd3c_quad_opaque_shaded_textured_modulated;
                cmd_arg_num = 12;
                break;
            case 0x3E: // polygon, 4 points, goraud-shaded, textured & modulated & semi-transparent
                current_ins = &core::cmd3e_quad_opaque_shaded_textured_modulated_semi;
                cmd_arg_num = 12;
                break;
            case 0x40: // monochrome line, opaque
                current_ins = &core::cmd40_line_opaque;
                cmd_arg_num = 3;
                break;
            case 0x60: // Rectangle, variable size, opaque
                current_ins = &core::cmd60_rect_opaque_flat;
                cmd_arg_num = 3;
                break;
            case 0x64: // Rectangle, variable size, textured, flat, opaque, modulated
                current_ins = &core::cmd64_rect_opaque_flat_textured_modulated;
                cmd_arg_num = 4;
                break;
            case 0x65: // rectangle, variable size, textured, flat, opaque
                current_ins = &core::cmd65_rect_opaque_flat_textured;
                cmd_arg_num = 4;
                break;
            case 0x66: // Rectangle, variable size, textured, flat, opaque, modulated
                current_ins = &core::cmd66_rect_semi_flat_textured_modulated;
                cmd_arg_num = 4;
                break;
            case 0x67: // rectangle, variable size, textured, flat, semi-transparent
                current_ins = &core::cmd67_rect_semi_flat_textured;
                cmd_arg_num = 4;
                break;
            case 0x6A: // Solid 1x1 rect
            case 0x68:
                current_ins = &core::cmd68_rect_1x1;
                cmd_arg_num = 2;
                break;
            case 0x6C: // 1x1 rect texuted semi-transparent
                current_ins = &core::cmd6c_rect_1x1_tex_modulated;
                cmd_arg_num = 3;
                break;
            case 0x6D: // 1x1 rect texuted opaque
                current_ins = &core::cmd6d_rect_1x1_tex;
                cmd_arg_num = 3;
                break;
            case 0x6e: // 1x1 rect texuted semi modulated
                current_ins = &core::cmd6e_rect_1x1_tex_semi_modulated;
                cmd_arg_num = 3;
                break;
            case 0x6F: // 1x1 rect texuted semi-transparent
                current_ins = &core::cmd6f_rect_1x1_tex_semi;
                cmd_arg_num = 3;
                break;
            case 0x74: // Rectangle, 8x8, opaque, textured, modulated
                current_ins = &core::cmd74_rect_opaque_flat_textured_modulated_8x8;
                cmd_arg_num = 3;
                break;
            case 0x75: // Rectangle, 8x8, opaque, textured
                current_ins = &core::cmd75_rect_opaque_flat_textured_8x8;
                cmd_arg_num = 3;
                break;
            case 0x76: // Rectangle, 8x8, semi-transparent, textured, modulated
                current_ins = &core::cmd76_rect_semi_flat_textured_modulated_8x8;
                cmd_arg_num = 3;
                break;
            case 0x77: // Rectangle, 8x8, semi, textured
                current_ins = &core::cmd77_rect_semi_flat_textured_8x8;
                cmd_arg_num = 3;
                break;
            case 0x7C:// Rectangle, 16x16, opaque, textured, modulated
                current_ins = &core::cmd7c_rect_opaque_flat_textured_modulated_16x16;
                cmd_arg_num = 3;
                break;

            case 0x7D:// Rectangle, 16x16, opaque, textured
                current_ins = &core::cmd7d_rect_opaque_flat_textured_16x16;
                cmd_arg_num = 3;
                break;
            case 0x7E: // Rectangle, 8x8, semi-transparent, textured, modulated
                current_ins = &core::cmd7e_rect_semi_flat_textured_modulated_16x16;
                cmd_arg_num = 3;
                break;
            case 0x7F: // Rectangle, 8x8, semi, textured
                current_ins = &core::cmd7f_rect_semi_flat_textured_16x16;
                cmd_arg_num = 3;
                break;
            case 0x80: // Copy from place to place
                current_ins = &core::cmd80_vram_copy;
                cmd_arg_num = 4;
                break;
            case 0xC0:
                current_ins = &core::gp0_image_save_start;
                cmd_arg_num = 3;
                break;
            case 0xBC:
            case 0xB8:
            case 0xA0: // Image stream to GPU
                current_ins = &core::gp0_image_load_start;
                cmd_arg_num = 3;
                break;
            case 0xE1: // GP0 Draw Mode
#ifdef DBG_GP0
                printf("GP0 E1 set draw mode");
#endif
                //printf("\nSET DRAW MODE %08x", cmd);
                page_base_x = cmd & 15;
                page_base_y = (cmd >> 4) & 1;
                semi_transparency = (cmd >> 5) & 3;
                switch ((cmd >> 7) & 3) {
                    case 0:
                        texture_depth = e_t4bit;
                        break;
                    case 1:
                        texture_depth = e_t8bit;
                        break;
                    case 2:
                        texture_depth = e_t15bit;
                        break;
                    case 3:
                        printf("\nUNHANDLED TEXTAR DEPTH 3!!!");
                        break;
                }
                dithering = (cmd >> 9) & 1;
                draw_to_display = (cmd >> 10) & 1;
                texture_disable = (cmd >> 1) & 1;
                GPUSTAT_update();
                rect.texture_x_flip = (cmd >> 12) & 1;
                rect.texture_y_flip = (cmd >> 13) & 1;
                break;
            case 0xE2: // Texture window
#ifdef DBG_GP0
                printf("\nGP0 E2 set draw mode");
#endif
                tx_win_x_mask = cmd & 0x1F;
                tx_win_y_mask = (cmd >> 5) & 0x1F;
                tx_win_x_offset = (cmd >> 10) & 0x1F;
                tx_win_y_offset = (cmd >> 15) & 0x1F;
                break;
            case 0xE3: // Set draw area upper-left corner
                draw_area_top = (cmd >> 10) & 0x3FF;
                draw_area_left = cmd & 0x3FF;
#ifdef DBG_GP0
                printf("\nGP0 E3 set draw area UL corner %d, %d", draw_area_top, draw_area_left);
#endif
                break;
            case 0xE4: // Draw area lower-right corner
                draw_area_bottom = (cmd >> 10) & 0x3FF;
                draw_area_right = cmd & 0x3FF;
#ifdef DBG_GP0
                printf("\nGP0 E4 set draw area LR corner %d, %d", draw_area_right, draw_area_bottom);
#endif
                break;
            case 0xE5: // Drawing offset
                draw_x_offset = mksigned11(cmd & 0x7FF);
                draw_y_offset = mksigned11((cmd >> 11) & 0x7FF);
#ifdef DBG_GP0
                printf("\nGP0 E5 set drawing offset %d, %d", draw_x_offset, draw_y_offset);
#endif
                break;
            case 0xE6: // Set Mask Bit setting
#ifdef DBG_GP0
                printf("\nGP0 E6 set bit mask");
#endif
                force_set_mask_bit = (cmd & 1) << 15;
                preserve_masked_pixels = (cmd >> 1) & 1;
                break;
            default:
                printf("\nUnknown GP0 command %08x", cmd);
                break;
        }
    }
}



void core::reset()
{
    bus->dotclock_change();
}

void core::write_gp1(u32 cmd)
{
    u32 cmdb = cmd >> 24;
    if ((cmdb >= 0x10) && (cmdb <= 0x1F)) cmdb = 0x10;
    switch(cmdb) {
        case 0:
            //printf("\nGP1 soft reset %08x", bus->cpu.gte.flags);
            // Soft reset
            unready_cmd();
            unready_recv_dma();
            unready_vram_to_CPU();
            page_base_x = 0;
            page_base_y = 0;
            semi_transparency = 0;
            texture_depth = e_t4bit;
            tx_win_x_mask = tx_win_y_mask = 0;
            tx_win_x_offset = tx_win_y_offset = 0;
            dithering = 0;
            draw_to_display = 0;
            texture_disable = 0;
            rect.texture_x_flip = rect.texture_y_flip = 0;
            draw_area_bottom = draw_area_right = draw_area_left = draw_area_top = 0;
            draw_x_offset = draw_y_offset = 0;
            force_set_mask_bit = 0;
            preserve_masked_pixels = 0;
            dma_direction = e_dma_off;
            display_disabled = 1;
            display_vram_x_start = display_vram_y_start = 0;
            hres = 0;
            vres = e_y240lines;
            vmode = e_ntsc;
            interlaced = 1;
            display_horiz_start = 0x200;
            display_horiz_end = 0xC00;
            display_line_start = 0x10;
            display_line_end = 0x100;
            display_depth = e_d15bits;
            recv_gp0_len = 0;
            handle_gp0 = &core::gp0_cmd;
            current_ins = nullptr;
            cmd_arg_index = 0;
            //clear_FIFO();
            GPUSTAT_update();
            ready_cmd();
            ready_recv_dma();
            ready_vram_to_CPU();
            // TODO: remember to flush GPU texture cache
            break;
        case 0x01: // reset CMD FIFO
            //console.log('RESET CMD FIFO NOT IMPLEMENT');
            break;
        case 0x02:
            irq1 = 0;
            bus->set_irq(IRQ_GPU, 0);
            GPUSTAT_update();
            break;
        case 0x03: // DISPLAY DISABLE
            //TODO: do this
            break;
        case 0x04: // DMA direction
            switch(cmd & 3) {
                case 0:
                    dma_direction = e_dma_off;
                    break;
                case 1:
                    dma_direction = e_dma_fifo;
                    break;
                case 2:
                    dma_direction = e_dma_cpu_to_gp0;
                    break;
                case 3:
                    dma_direction = e_dma_vram_to_cpu;
                    break;
            }
            GPUSTAT_update();
            break;
        case 0x05: // VRAM start
            //console.log('GP1 VRAM start');
            display_vram_x_start = cmd & 0x3FE;
            display_vram_y_start = (cmd >> 10) & 0x1FF;
            break;
        case 0x06: // Display horizontal range, in output coordinates
            display_horiz_start = cmd & 0xFFF;
            display_horiz_end = (cmd >> 12) & 0xFFF;
            bus->dotclock_change();
            break;
        case 0x07: // Display vertical range, in output coordinates
            display_line_start = cmd & 0x3FF;
            display_line_end = (cmd >> 10) & 0x3FF;
            break;
        case 0x08: {// Display mode
            //console.log('GP1 display mode');
            hr1 = cmd & 3;
            hr2 = ((cmd >> 6) & 1);

            vres = (cmd & 4) ? e_y480lines : e_y240lines;
            vmode = (cmd & 8) ? e_pal : e_ntsc;
            display_depth = (cmd & 16) ? e_d15bits : e_d24bits;
            interlaced = (cmd >> 5) & 1;
            if ((cmd & 0x80) != 0) {
                printf("\nUnsupported display mode!");
            }
            bus->dotclock_change();
            break; }
        case 0x10: {
            // Read internal register
            u32 reg = cmd & 0xFFFFFF;
            reg &= 7;
            u32 r = 0;
            switch (reg) {
                case 0x00:
                case 0x01:
                case 0x06:
                case 0x07:
                    break;
                case 0x02:
                    r = tx_win_x_mask;
                    r |= (tx_win_y_mask << 5);
                    r |= (tx_win_x_offset << 10);
                    r |= (tx_win_y_offset << 15);
                    io.GPUREAD = r;
                    break;
                case 0x03:
                    r = draw_area_top;
                    r |= (draw_area_left << 10);
                    io.GPUREAD = r;
                    break;
                case 0x04:
                    r = draw_area_bottom;
                    r |= (draw_area_right << 10);
                    io.GPUREAD = r;
                    break;
                case 0x05:
                    r = (draw_x_offset & 0x7FF);
                    r |= ((draw_y_offset & 0x7FF) << 11);
                    io.GPUREAD = r;
                    break;
            }
            break; }
        default:
            printf("\nUnknown GP1 command %08x", cmd);
            break;
    }
}

u32 core::get_gpuread()
{
    u32 v = io.GPUREAD;
    if (VRAM_to_CPU_in_progress) gp0_image_save_continue();
    return v;
}
u32 core::get_gpustat() const
{
    return io.GPUSTAT | 0x10000000 | (io.frame << 31);
}
}
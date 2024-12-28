//
// Created by . on 12/4/24.
//

#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "helpers/debug.h"
#include "helpers/debugger/debugger.h"
#include "gba_bus.h"
#include "helpers/multisize_memaccess.c"

#include "gba_ppu.h"

void GBA_PPU_init(struct GBA *this)
{
    memset(&this->ppu, 0, sizeof(this->ppu));
}

void GBA_PPU_delete(struct GBA *this)
{

}

void GBA_PPU_reset(struct GBA *this)
{

}

static void pprint_palette_ram(struct GBA *this)
{
    for (u32 i = 0; i < 512; i++) {
        u16 c = this->ppu.palette_RAM[i];
    }
}

static void vblank(struct GBA *this, u32 val)
{
    //pprint_palette_ram(this);
    this->clock.ppu.vblank_active = val;
    if (val == 1) {
        if (!(this->io.IF & 1)) printf("\nVBLANK IRQ frame:%lld line:%d cyc:%lld", this->clock.master_frame, this->clock.ppu.y, this->clock.master_cycle_count);
        this->io.IF |= 1;
        GBA_eval_irqs(this);
        GBA_check_dma_at_vblank(this);
    }
}

static void hblank(struct GBA *this, u32 val)
{
    this->clock.ppu.hblank_active = 1;
    if (val == 0) {
        if (this->ppu.io.vcount_at == this->clock.ppu.y) {
            if (!(this->io.IF & 4)) printf("\nVCOUNT IRQ frame:%lld line:%d cyc:%lld", this->clock.master_frame, this->clock.ppu.y, this->clock.master_cycle_count);
            this->io.IF |= 4;
            GBA_eval_irqs(this);
        }
    }
    if ((val == 1) && (this->clock.ppu.y < 160)) {
        if (!(this->io.IF & 2)) printf("\nHBLANK IRQ frame:%lld line:%d cyc:%lld", this->clock.master_frame, this->clock.ppu.y, this->clock.master_cycle_count);
        this->io.IF |= 2;
        GBA_eval_irqs(this);
    }
}

static void new_frame(struct GBA *this)
{
    this->clock.ppu.y = 0;
    this->ppu.cur_output = ((u16 *)this->ppu.display->output[this->ppu.display->last_written ^ 1]);
    this->ppu.display->last_written ^= 1;
    this->clock.master_frame++;
    vblank(this, 0);
}

void GBA_PPU_start_scanline(struct GBA*this)
{
    hblank(this, 0);
    this->ppu.cur_pixel = 0;
    this->clock.ppu.scanline_start = this->clock.master_cycle_count;
    if (this->clock.ppu.y == 0) {
        struct GBA_PPU_bg *bg;
        for (u32 i = 2; i < 4; i++) {
            bg = &this->ppu.bg[i];
            bg->x_lerp = bg->x;
            bg->y_lerp = bg->y;
        }
    }
}

#define OUT_WIDTH 240

static void get_obj_tile_size(u32 sz, u32 shape, u32 *htiles, u32 *vtiles)
{
#define T(s1, s2, hn, vn) case (((s1) << 2) | (s2)): *htiles = hn; *vtiles = vn; return;
    switch((sz << 2) | shape) {
        T(0, 0, 1, 1)
        T(0, 1, 2, 1)
        T(0, 2, 1, 2)
        T(1, 0, 2, 2)
        T(1, 1, 4, 1)
        T(1, 2, 1, 4)
        T(2, 0, 4, 4)
        T(2, 1, 4, 2)
        T(2, 2, 2, 4)
        T(3, 0, 8, 8)
        T(3, 1, 8, 4)
        T(3, 2, 4, 8)
    }
    printf("\nHEY! INVALID SHAPE %d", shape);
    *htiles = 1;
    *vtiles = 1;
#undef T
}

// Thanks tonc!
static u32 se_index_fast(u32 tx, u32 ty, u32 bgcnt) {
    u32 n = tx + ty * 32;
    if (tx >= 32)
        n += 0x03E0;
    if (ty >= 32 && (bgcnt == 3))
        n += 0x0400;
    return n;
}

static void get_affine_bg_pixel(struct GBA *this, u32 bgnum, struct GBA_PPU_bg *bg, i32 px, i32 py, struct GBA_PX *opx)
{
    // so first deal with clipping...
    if (bg->display_overflow) { // wraparound
        //while (px < 0) px += bg->hpixels;
        //while (py < 0) py += bg->vpixels;
        px &= bg->hpixels_mask;
        py &= bg->vpixels_mask;
    }
    else { // clip/transparent
        if (px < 0) return;
        if (py < 0) return;
        if (px >= bg->hpixels) return;
        if (py >= bg->vpixels) return;
    }

    // Now px and py represent a number inside 0...hpixels and 0...vpixels
    u32 tile_x = px >> 3;
    u32 tile_y = py >> 3;
    u32 line_in_tile = py & 7;

    u32 tile_width = bg->htiles;

    u32 screenblock_addr = bg->screen_base_block;
    screenblock_addr += tile_x + (tile_y * tile_width);
    u32 tile_num = ((u8 *)this->ppu.VRAM)[screenblock_addr];

    // so now, grab that tile...
    u32 tile_start_addr = bg->character_base_block + (tile_num * 64);
    u32 line_start_addr = tile_start_addr + (line_in_tile * 8);
    u8 *ptr = ((u8 *)this->ppu.VRAM) + line_start_addr;
    u8 color = ptr[px & 7];
    if (color != 0) {
        opx->has = 1;
        opx->bpp8 = 1;
        opx->color = color;
        opx->priority = bg->priority;
        opx->palette = 0;
    }
    else {
        opx->has = 0;
    }
}

// get color from (px,py)
static void get_affine_sprite_pixel(struct GBA *this, u32 mode, i32 px, i32 py, u32 tile_num, u32 htiles, u32 vtiles, u32 bpp8, u32 palette, u32 priority, u32 obj_mapping_2d, u32 dsize, i32 screen_x, struct GBA_PPU_window *w)
{
    i32 hpixels = htiles * 8;
    i32 vpixels = vtiles * 8;
    if (dsize) {
        //px >>= 1;
        //py >>= 1;
        hpixels >>= 1;
        vpixels >>= 1;
    }
    px += (hpixels >> 1);
    py += (vpixels >> 1);
    if ((px < 0) || (py < 0)) return;
    if ((px >= hpixels) || (py > vpixels)) return;
    // py is line_in_sprite
    u32 line_in_tile = py & 7;
    // Get start of tile
    u32 tile_offset = 32 * tile_num;

    // Offset to the correct line inside the tile
    u32 tile_bytes = bpp8 ? 64 : 32;
    u32 tile_line_bytes = bpp8 ? 8 : 4;
    u32 offset_in_tile_for_y = line_in_tile * tile_line_bytes;

    // Add together
    u32 tile_start_addr = tile_offset + offset_in_tile_for_y;

    // Add 0x10000 where tiles start
    tile_start_addr += 0x10000;

    // Now we must offset for tile y #

    u32 tile_line_stride = 0;
    if (obj_mapping_2d) { // 2d mapping. rows are 32 tiles wide
        tile_line_stride = 64 * tile_line_bytes;
    }
    else { // 1d mapping. rows are htiles wide
        tile_line_stride = htiles * tile_line_bytes;
    }
    tile_start_addr += (tile_line_stride * (py >> 3));

    // Now we must offset for the X inside the overall thing we are at
    u32 x_tile = px >> 3;
    u32 x_in_tile = px & 7;
    u32 px_halves = bpp8 ? 2 : 1;
    u32 x_offset = (x_tile * tile_bytes) + ((x_in_tile * px_halves) >> 1);
    tile_start_addr += x_offset;

    u8 *ptr = ((u8 *)this->ppu.VRAM) + tile_start_addr;
    if (bpp8) {
        u8 data = *ptr;
        if (data != 0) {
            switch (mode) {
                case 1:
                case 0: {
                    struct GBA_PX *opx = &this->ppu.obj.line[screen_x];
                    if (!opx->has) {
                        opx->has = 1;
                        opx->priority = priority;
                        opx->palette = 0;
                        opx->color = data;
                        opx->bpp8 = 1;
                    }
                    break; }
                case 2:
                    w->inside[screen_x] = 1;
                    break;
            }
        }
    }
    else {
        u8 data = *ptr;
        u32 half = x_in_tile & 1;
        u16 c;
        if (half == 0) c = data & 15;
        else c = (data >> 4) & 15;

        if (c != 0) {
            switch(mode) {
                case 1:
                case 0: {
                    struct GBA_PX *opx = &this->ppu.obj.line[screen_x];
                    if (!opx->has) {
                        opx->has = 1;
                        opx->priority = priority;
                        opx->palette = palette;
                        opx->color = c;
                        opx->bpp8 = 0;
                    }
                    break; }
                case 2: {
                    w->inside[screen_x] = 1;
                break; }
            }
        }
    }
}

static void get_sprite_tile_ptrs(struct GBA *this, u32 tile_num, u32 htiles, u32 tile_y_num, u32 line_in_tile, u32 bpp8, u32 d2, u8 *tile_ptrs[8])
{
    // 0:0000h
    // 1:4000h
    u32 tile_offset = 32 * tile_num;
    // 32 bytes per tile in 4bpp
    // 64 in 8bpp
    u32 tile_bytes = bpp8 ? 64 : 32;
    u32 tile_line_bytes = bpp8 ? 8 : 4;
    u32 offset_in_tile = line_in_tile * tile_line_bytes;
    u32 tile_start_addr = tile_offset + offset_in_tile;
    tile_start_addr += 0x10000;
    // tile stride for 1d is tile_bytes
    // tile stride for 2d is tile_bytes*32
    // Now account for tile Y inside overall sprite...
    u32 tile_line_stride = 0;
    if (d2) { // 2d mapping. rows are 32 tiles wide
        tile_line_stride = 64 * tile_line_bytes;
    }
    else { // 1d mapping. rows are htiles wide
        tile_line_stride = htiles * tile_line_bytes;
    }
    tile_start_addr += (tile_line_stride * tile_y_num);
    assert(tile_start_addr < 0x18000);
    u8 *ptr = ((u8 *)this->ppu.VRAM) + tile_start_addr;
    for (u32 i = 0; i < htiles; i++) {
        tile_ptrs[i] = ptr;
        ptr += tile_bytes;
    }
}

static void draw_sprite_affine(struct GBA *this, u16 *ptr, struct GBA_PPU_window *w)
{
    this->ppu.obj.drawing_cycles -= 1;
    if (this->ppu.obj.drawing_cycles < 1) return;

    u32 dsize = (ptr[0] >> 9) & 1;

    i32 y = ptr[0] & 0xFF;
    if (y > 160) {
        y -= 160;
        y = 0 - y;
    }
    if ((this->clock.ppu.y < y)) return;

    u32 mode = (ptr[0] >> 10) & 3;
    u32 shape = (ptr[0] >> 14) & 3; // 0 = square, 1 = horiozontal, 2 = vertical
    u32 sz = (ptr[1] >> 14) & 3;
    u32 htiles, vtiles;
    get_obj_tile_size(sz, shape, &htiles, &vtiles);
    if (dsize) {
        htiles *= 2;
        vtiles *= 2;
    }

    i32 y_bottom = y + (i32)(vtiles * 8);
    if (this->clock.ppu.y >= y_bottom) return;

    u32 tile_num = ptr[2] & 0x3FF;
    u32 bpp8 = (ptr[0] >> 13) & 1;
    u32 x = ptr[1] & 0x1FF;
    x = SIGNe9to32(x);
    u32 pgroup = (ptr[1] >> 9) & 31;
    u32 pbase = (pgroup * 0x20) >> 1;
    u16 *pbase_ptr = ((u16 *)this->ppu.OAM) + pbase;
    i32 pa = SIGNe16to32(pbase_ptr[3]);
    i32 pb = SIGNe16to32(pbase_ptr[7]);
    i32 pc = SIGNe16to32(pbase_ptr[11]);
    i32 pd = SIGNe16to32(pbase_ptr[15]);
    this->ppu.obj.drawing_cycles -= 9;
    if (this->ppu.obj.drawing_cycles < 0) return;

    u32 priority = (ptr[2] >> 10) & 3;
    u32 palette = bpp8 ? 0 : ((ptr[2] >> 12) & 15);
    if (bpp8) tile_num &= 0x3FE;

    i32 line_in_sprite = this->clock.ppu.y - y;
    //i32 x_origin = x - (htiles << 2);

    i32 screen_x = x;
    i32 iy = line_in_sprite - (vtiles * 4);
    i32 hwidth = htiles * 4;
    for (i32 ix=-hwidth; ix < hwidth; ix++) {
        i32 px = (pa*ix + pb*iy)>>8;    // get x texture coordinate
        i32 py = (pc*ix + pd*iy)>>8;    // get y texture coordinate
        if ((screen_x >= 0) && (screen_x < 240))
            get_affine_sprite_pixel(this, mode, px, py, tile_num, htiles, vtiles, bpp8, palette, priority,
                                    this->ppu.io.obj_mapping_2d, dsize, screen_x, w);   // get color from (px,py)
        screen_x++;
        this->ppu.obj.drawing_cycles -= 2;
        if (this->ppu.obj.drawing_cycles < 0) return;
    }
}

static void output_sprite_8bpp(struct GBA *this, u8 *tptr, u32 mode, i32 screen_x, u32 priority, u32 hflip, struct GBA_PPU_window *w) {
    for (i32 tile_x = 0; tile_x < 8; tile_x++) {
        i32 sx = tile_x + screen_x;
        if ((sx >= 0) && (sx < 240)) {
            this->ppu.obj.drawing_cycles -= 1;
            struct GBA_PX *opx = NULL;
            if (mode < 2) opx = &this->ppu.obj.line[sx];
            if ((mode > 1) || (!opx->has)) {
                u32 rpx = hflip ? tile_x : (7 - tile_x);
                u16 c = tptr[rpx];
                if (c != 0) {
                    switch (mode) {
                        case 1:
                        case 0:
                            opx->color = c;
                            opx->bpp8 = 1;
                            opx->priority = priority;
                            opx->has = 1;
                            opx->palette = 0;
                            break;
                        case 2: { // OBJ window
                            w->inside[sx] = 1;
                            break;
                        }
                    }
                }
                break;
            }
        }
        if (this->ppu.obj.drawing_cycles < 1) return;
    }
}


static void output_sprite_4bpp(struct GBA *this, u8 *tptr, u32 mode, i32 screen_x, u32 priority, u32 hflip, u32 palette, struct GBA_PPU_window *w) {
    for (i32 tile_x = 0; tile_x < 4; tile_x++) {
        i32 sx;
        if (hflip) sx = (7 - (tile_x * 2)) + screen_x;
        else sx = (tile_x * 2) + screen_x;
        u8 data = tptr[tile_x];
        for (u32 i = 0; i < 2; i++) {
            if ((sx >= 0) && (sx < 240)) {
                this->ppu.obj.drawing_cycles -= 1;
                struct GBA_PX *opx = NULL;
                if (mode < 2) opx = &this->ppu.obj.line[sx];
                if ((mode > 1) || (!opx->has)) {
                    u32 c = data & 15;
                    if (c != 0) {
                        switch (mode) {
                            case 1:
                            case 0: {
                                opx->color = c;
                                opx->bpp8 = 0;
                                opx->priority = priority;
                                opx->has = 1;
                                opx->palette = palette;
                                break;
                            }
                            case 2: {
                                w->inside[sx] = 1;
                                break;
                            }
                        }
                    }
                    if (this->ppu.obj.drawing_cycles < 1) return;
                }
                if (hflip) sx--;
                else sx++;
            }
        }
    }
}

static void draw_sprite_normal(struct GBA *this, u16 *ptr, struct GBA_PPU_window *w)
{
    // 1 cycle to evaluate and 1 cycle per pixel
    this->ppu.obj.drawing_cycles -= 1;
    if (this->ppu.obj.drawing_cycles < 1) return;

    u32 obj_disable = (ptr[0] >> 9) & 1;
    if (obj_disable) return;

    i32 y = ptr[0] & 0xFF;
    if (y > 160) {
        y -= 160;
        y = 0 - y;
    }
    if ((this->clock.ppu.y < y)) return;

    // Clip sprite
    u32 shape = (ptr[0] >> 14) & 3; // 0 = square, 1 = horiozontal, 2 = vertical
    u32 sz = (ptr[1] >> 14) & 3;
    u32 htiles, vtiles;
    get_obj_tile_size(sz, shape, &htiles, &vtiles);

    i32 y_bottom = y + (i32)(vtiles * 8);
    if (this->clock.ppu.y >= y_bottom) return;

    u32 tile_num = ptr[2] & 0x3FF;
    if ((this->ppu.io.bg_mode >= 3) && (tile_num < 512)) {
        printf("\nLOW NUMBER in BG MODE!");
        return;
    }

    u32 mode = (ptr[0] >> 10) & 3;
    u32 mosaic = (ptr[0] >> 12) & 1;
    u32 bpp8 = (ptr[0] >> 13) & 1;
    u32 x = ptr[1] & 0x1FF;
    x = SIGNe9to32(x);
    u32 hflip = (ptr[1] >> 12) & 1;
    u32 vflip = (ptr[1] >> 13) & 1;

    u32 priority = (ptr[2] >> 10) & 3;
    u32 palette = bpp8 ? 0 : ((ptr[2] >> 12) & 15);
    if (bpp8) tile_num &= 0x3FE;

    // OK we got all the attributes. Let's draw it!
    u32 line_in_sprite = this->clock.ppu.y - y;
    //printf("\nPPU LINE %d LINE IN SPR:%d Y:%d", this->clock.ppu.y, line_in_sprite, y);
    if (vflip) line_in_sprite = (((vtiles * 8) - 1) - line_in_sprite);
    u32 tile_y_in_sprite = line_in_sprite >> 3; // /8
    u32 line_in_tile = line_in_sprite & 7;

    // OK so we know which line
    // We have two possibilities; 1d or 2d layout
    u8 *tile_ptrs[8];
    get_sprite_tile_ptrs(this, tile_num, htiles, tile_y_in_sprite, line_in_tile, bpp8, this->ppu.io.obj_mapping_2d, tile_ptrs);

    i32 screen_x = x;
    for (u32 tile_xs = 0; tile_xs < htiles; tile_xs++) {
        u32 tile_x = hflip ? ((htiles - 1) - tile_xs) : tile_xs;
        u8 *tptr = tile_ptrs[tile_x];
        if (bpp8) output_sprite_8bpp(this, tptr, mode, screen_x, priority, hflip, w);
        else output_sprite_4bpp(this, tptr, mode, screen_x, priority, hflip, palette, w);
        screen_x += 8;
        if (this->ppu.obj.drawing_cycles < 1) return;
    }
}


static void draw_obj_line(struct GBA *this)
{
    this->ppu.obj.drawing_cycles = this->ppu.io.hblank_free ? 954 : 1210;

    memset(this->ppu.obj.line, 0, sizeof(this->ppu.obj.line));
    struct GBA_PPU_window *w = &this->ppu.window[GBA_WINOBJ];
    memset(&w->inside, 0, sizeof(w->inside));

    if (!this->ppu.obj.enable) return;
    // Each OBJ takes:
    // n*1 cycles per pixel
    // 10 + n*2 per pixel if rotated/scaled
    for (u32 i = 0; i < 128; i++) {
        u16 *ptr = ((u16 *)this->ppu.OAM) + (i * 4);
        u32 affine = (ptr[0] >> 8) & 1;
        if (affine) draw_sprite_affine(this, ptr, w);
        else draw_sprite_normal(this, ptr, w);
    }
}

static void fetch_bg_slice(struct GBA *this, struct GBA_PPU_bg *bg, u32 bgnum, u32 tile_x, u32 vpos, struct GBA_PX px[8])
{
    // First, get location in VRAM of nametable...
    //tilemap entries are 2 bytes
    // divided into 2kb screenblocks
    // each screenblock is 32x32
    //   64x32   32x64     64x64
    // 0   01     0         0 1
    //            1         2 3
    //
    u32 screenblock_x = tile_x >> 6;
    u32 tile_y = vpos >> 3;
    /*u32 screenblock_y = tile_y >> 6;
    u32 screenblock_offset = 0;
    switch(bg->screen_size) {
        case 0: break;
        case 1: // 64x32
            screenblock_offset += (2048 * screenblock_x);
            break;
        case 2: // 32x64
            screenblock_offset += (2048 * screenblock_y);
            break;
        case 3:
            screenblock_offset += (4096 * screenblock_y) + (2048 * screenblock_x);
            break;
    }
    u32 sb_tile_x = tile_x & 31;
    u32 sb_tile_y = tile_y & 31;
    u32 screenblock_addr = bg->screen_base_block + screenblock_offset + (sb_tile_y * 64) + (sb_tile_x * 2);*/
    u32 screenblock_addr = bg->screen_base_block + (se_index_fast(tile_x, tile_y, bg->screen_size) << 1);

    u16 attr = *(u16 *)(((u8 *)this->ppu.VRAM) + screenblock_addr);
    u32 tile_num = attr & 0x3FF;
    u32 hflip = (attr >> 10) & 1;
    u32 vflip = (attr >> 11) & 1;
    u32 palbank = (attr >> 12) & 15;

    u32 line_in_tile = vpos & 7;
    if (vflip) line_in_tile = 7 - line_in_tile;

    u32 tile_bytes = bg->bpp8 ? 64 : 32;
    u32 line_size = bg->bpp8 ? 8 : 4;
    u32 tile_start_addr = bg->character_base_block + (tile_num * tile_bytes);
    u32 line_addr = tile_start_addr + (line_in_tile * line_size);
    if (line_addr > 0x10000) return; // hardware doesn't draw from up there
    u8 *ptr = ((u8 *)this->ppu.VRAM) + line_addr;

    if (bg->bpp8) {
        u32 mx = hflip ? 7 : 0;
        for (u32 ex = 0; ex < 8; ex++) {
            u8 data = ptr[mx];
            struct GBA_PX *p = &px[ex];
            if (data != 0) {
                p->has = 1;
                p->palette = 0;
                p->color = data;
                p->bpp8 = 1;
                p->priority = bg->priority;
            }
            else
                p->has = 0;
            if (hflip) mx--;
            else mx++;
        }
    }
    else {
        u32 mx = 0;
        if (hflip) mx = 7;
        for (u32 ex = 0; ex < 4; ex++) {
            u8 data = *ptr;
            ptr++;
            for (u32 i = 0; i < 2; i++) {
                u16 c = data & 15;
                data >>= 4;
                struct GBA_PX *p = &px[mx];
                if (c != 0) {
                    p->has = 1;
                    p->palette = palbank;
                    p->color = c;
                    p->bpp8 = 0;
                    p->priority = bg->priority;}
                else
                    p->has = 0;
                if (hflip) mx--;
                else mx++;
            }
        }
    }
}

static void calculate_windows(struct GBA *this)
{
    if (!this->ppu.window[0].enable && !this->ppu.window[1].enable && !this->ppu.window[GBA_WINOBJ].enable) return;

    // Calculate windows...
    struct GBA_PPU_window *w;
    for (u32 wn = 0; wn < 2; wn++) {
        w = &this->ppu.window[wn];

        if (!w->enable) {
            memset(&w->inside, 1, sizeof(w->inside));
            continue;
        }

        u32 x;
        u32 l_morethan_r = w->left > w->right;
        u32 in_topbottom;
        u32 y = this->clock.ppu.y;

        if (w->top > w->bottom) { // wrapping...
            in_topbottom = ((y < w->top) || (y >= w->bottom));
        }
        else { // normal!
            in_topbottom = ((y >= w->top) && (y < w->bottom));
        }

        for (x = 0; x < 240; x++) {
            u32 inside = ((x >= w->left) && (x < w->right)) ^ l_morethan_r;
            w->inside[x] = inside && in_topbottom;
        }
    }

    // Now take care of the outside window
    struct GBA_PPU_window *w0 = &this->ppu.window[0];
    struct GBA_PPU_window *w1 = &this->ppu.window[1];
    struct GBA_PPU_window *ws = &this->ppu.window[GBA_WINOBJ];
    u32 w0r = w0->enable ^ 1;
    u32 w1r = w1->enable ^ 1;
    u32 wsr = ws->enable ^ 1;
    w = &this->ppu.window[GBA_WINOUTSIDE];
    memset(w->inside, 0, sizeof(w->inside));
    for (u32 x = 0; x < 240; x++) {
        u32 w0i = w0r | w0->inside[x];
        u32 w1i = w1r | w1->inside[x];
        u32 wsi = wsr | ws->inside[x];
        w->inside[x] = !(w0i || w1i || wsi);
    }
}

static void apply_windows(struct GBA *this, u32 sp_window, u32 bg0, u32 bg1, u32 bg2, u32 bg3)
{
    if (!this->ppu.window[0].enable && !this->ppu.window[1].enable && !this->ppu.window[GBA_WINOBJ].enable) return;
    // Each window can be individually enable/disabled
    //
}

static void draw_bg_line_affine(struct GBA *this, u32 bgnum)
{
    struct GBA_PPU_bg *bg = &this->ppu.bg[bgnum];
    memset(bg->line, 0, sizeof(bg->line));
    if (!bg->enable) return;

/*
set affine coords to BGnX/BGnY at the start of the frame
also reset to BGnX/BGnY whenever these registers are changed (not sure how that behaves if done mid-scanline, check NBA)
increment by BGnPB/BGnPD after every scanline
increment by BGnPA/BGnPC after every pixel
 */
    i32 fx = bg->x_lerp;
    i32 fy = bg->y_lerp;
    for (u32 screen_x = 0; screen_x < 240; screen_x++) {
        get_affine_bg_pixel(this, bgnum, bg, fx>>8, fy>>8, &bg->line[screen_x]);
        fx += bg->pa;
        fy += bg->pc;
    }
    bg->x_lerp += bg->pb;
    bg->y_lerp += bg->pd;
}

static void draw_bg_line_normal(struct GBA *this, u32 bgnum)
{
    struct GBA_PPU_bg *bg = &this->ppu.bg[bgnum];
    memset(bg->line, 0, sizeof(bg->line));
    if (!bg->enable) return;
    // first do a fetch for fine scroll -1
    u32 hpos = bg->hscroll & bg->hpixels_mask;
    u32 vpos = (bg->vscroll + this->clock.ppu.y) & bg->vpixels_mask;
    u32 fine_x = hpos & 7;
    u32 screen_x = 0;
    struct GBA_PX bgpx[8];
    //hpos = ((hpos >> 3) - 1) << 3;
    fetch_bg_slice(this, bg, bgnum, hpos >> 3, vpos, bgpx);
    // TODO HERE
    u32 startx = fine_x;
    for (u32 i = startx; i < 8; i++) {
        bg->line[screen_x] = bgpx[i];
        screen_x++;
        hpos++;
    }
    //hpos = (((hpos >> 3) + 1) << 3);
    hpos &= bg->hpixels_mask;

    while (screen_x < 240) {
        fetch_bg_slice(this, bg, bgnum, hpos >> 3, vpos, &bg->line[screen_x]);
        screen_x += 8;
        hpos = (hpos + 8) & bg->hpixels_mask;
    }
}

static void output_pixel(struct GBA *this, u32 x, u32 obj_enable, u32 bg_enables[4], u16 *line_output)
{
    struct GBA_PX *sp = &this->ppu.obj.line[x];
    //if (this->clock.ppu.y == 32) c = 0x001F;
    struct GBA_PX *highest_priority_bg_px = NULL;
    u32 bg_priority = 5;
    struct GBA_PX *mp;
    for (u32 i = 0; i < 4; i++) {
        if (!bg_enables[i]) continue;
        mp = &this->ppu.bg[i].line[x];
        if ((mp->has) && (mp->priority < bg_priority)) {
            highest_priority_bg_px = mp;
            bg_priority = mp->priority;
        }
    }
    struct GBA_PX *op = NULL;
    u32 pal_offset = 0;
    if ((sp->has) && (sp->priority <= bg_priority)) {
        // do sprite!
        op = sp;
        pal_offset = 256;
    }
    else if (highest_priority_bg_px != NULL) {
        // do bg!
        op = highest_priority_bg_px;
    }
    u16 c;
    if (op == NULL) {
        c = 0;
    }
    else {
        c = op->color;
        if (op->bpp8) c = this->ppu.palette_RAM[pal_offset + c];
        else c = this->ppu.palette_RAM[pal_offset + (op->palette << 4) + c];
    }

    line_output[x] = c;
}

static void draw_line0(struct GBA *this, u16 *line_output)
{
    ///printf("\nno line0...")
    draw_obj_line(this);
    draw_bg_line_normal(this, 0);
    draw_bg_line_normal(this, 1);
    draw_bg_line_normal(this, 2);
    draw_bg_line_normal(this, 3);
    calculate_windows(this);
    apply_windows(this, 1, 1, 1, 1, 1);
    u32 bg_enables[4] = {this->ppu.bg[0].enable, this->ppu.bg[1].enable, this->ppu.bg[2].enable, this->ppu.bg[3].enable};
    for (u32 x = 0; x < 240; x++) {
        output_pixel(this, x, this->ppu.obj.enable, &bg_enables[0], line_output);
    }
}

static void draw_line1(struct GBA *this, u16 *line_output)
{
    draw_obj_line(this);
    draw_bg_line_normal(this, 0);
    draw_bg_line_normal(this, 1);
    draw_bg_line_affine(this, 2);
    apply_windows(this, 1, 1, 1, 1, 0);
    u32 bg_enables[4] = {this->ppu.bg[0].enable, this->ppu.bg[1].enable, this->ppu.bg[2].enable, 0};
    for (u32 x = 0; x < 240; x++) {
        output_pixel(this, x, this->ppu.obj.enable, &bg_enables[0], line_output);
    }
}

static void draw_line2(struct GBA *this, u16 *line_output)
{
    draw_obj_line(this);
    draw_bg_line_affine(this, 2);
    draw_bg_line_affine(this, 3);
    apply_windows(this, 1, 0, 0, 1, 1);
    u32 bg_enables[4] = {0, 0, this->ppu.bg[2].enable, this->ppu.bg[3].enable};
    for (u32 x = 0; x < 240; x++) {
        output_pixel(this, x, this->ppu.obj.enable, &bg_enables[0], line_output);
    }
}

static void draw_line3(struct GBA *this, u16 *line_output)
{
    u16 *line_input = ((u16 *)this->ppu.VRAM) + (this->clock.ppu.y * 240);
    if (this->ppu.obj.enable) draw_obj_line(this);
    for (u32 x = 0; x < 240; x++) {
        struct GBA_PX *sp = &this->ppu.obj.line[x];
        u16 c;
        if (sp->has) {
            c = sp->color;
            if (sp->bpp8) {
                c = this->ppu.palette_RAM[0x100 + c];
            }
            else {
                c = this->ppu.palette_RAM[0x100 + (c + (sp->palette << 4))];
            }
        }
        else {
            c = line_input[x];
        }
        line_output[x] = c;
    }
}

static void draw_line4(struct GBA *this, u16 *line_output)
{
    u32 base_addr = 0xA000 * this->ppu.io.frame;
    u8 *line_input = ((u8 *)this->ppu.VRAM) + base_addr + (this->clock.ppu.y * 240);
    if (this->ppu.obj.enable) draw_obj_line(this);
    for (u32 x = 0; x < 240; x++) {
        u8 palcol = line_input[x];
        line_output[x] = this->ppu.palette_RAM[palcol];
    }
}

static void draw_line5(struct GBA *this, u16 *line_output)
{
    if (this->clock.ppu.y > 127) {
      memset(line_output, 0, 480);
      return;
    }
    u32 base_addr = 0xA000 * this->ppu.io.frame;
    u16 *line_input = (u16 *)(((u8 *)this->ppu.VRAM) + base_addr + (this->clock.ppu.y * 320));
    memset((line_output+160), 0, 160);
    memcpy(line_output, line_input, 320);
}

void GBA_PPU_hblank(struct GBA*this)
{
    // It's cleared at cycle 0 and set at cycle 1007
    hblank(this, 1);

    // Check if we have any DMA transfers that need to go...
    GBA_check_dma_at_hblank(this);

    // Now draw line!
    if (this->clock.ppu.y < 160) {
        u16 *line_output = this->ppu.cur_output + (this->clock.ppu.y * OUT_WIDTH);
        if (this->ppu.io.force_blank) {
            memset(line_output, 0xFF, 480);
            return;
        }
        switch (this->ppu.io.bg_mode) {
            case 0:
                draw_line0(this, line_output);
                break;
            case 1:
                draw_line1(this, line_output);
                break;
            case 2:
                draw_line2(this, line_output);
                break;
            case 3:
                draw_line3(this, line_output);
                break;
            case 4:
                draw_line4(this, line_output);
                break;
            case 5:
                draw_line5(this, line_output);
                break;
            default:
                assert(1 == 2);
        }
    }
}

void GBA_PPU_finish_scanline(struct GBA*this)
{
    // do stuff, then
    this->clock.ppu.hblank_active = 0;
    this->clock.ppu.y++;
    if (this->clock.ppu.y == 160) {
        vblank(this, 1);
    }
    if (this->clock.ppu.y == 227) {
        this->clock.ppu.vblank_active = 0;
    }
    if (this->clock.ppu.y == 228) {
        new_frame(this);
    }
}

static u32 GBA_PPU_read_invalid(struct GBA *this, u32 addr, u32 sz, u32 access, u32 has_effect)
{
    printf("\nREAD UNKNOWN PPU ADDR:%08x sz:%d", addr, sz);
    return 0;
}

static void GBA_PPU_write_invalid(struct GBA *this, u32 addr, u32 sz, u32 access, u32 val)
{
    printf("\nWRITE UNKNOWN PPU ADDR:%08x sz:%d DATA:%08x", addr, sz, val);
    //dbg.var++;
    //if (dbg.var > 5) dbg_break("too many ppu write", this->clock.master_cycle_count);
}

u32 GBA_PPU_mainbus_read_palette(struct GBA *this, u32 addr, u32 sz, u32 access, u32 has_effect)
{
    if (addr < 0x05000400)
        return cR[sz](this->ppu.palette_RAM, addr & 0x3FF);

    return GBA_PPU_read_invalid(this, addr, sz, access, has_effect);
}

u32 GBA_PPU_mainbus_read_VRAM(struct GBA *this, u32 addr, u32 sz, u32 access, u32 has_effect)
{
    if (addr < 0x06018000)
        return cR[sz](this->ppu.VRAM, addr - 0x06000000);

    return GBA_PPU_read_invalid(this, addr, sz, access, has_effect);

}

u32 GBA_PPU_mainbus_read_OAM(struct GBA *this, u32 addr, u32 sz, u32 access, u32 has_effect) {
    if (addr < 0x07000400)
        return cR[sz](this->ppu.OAM, addr & 0x3FF);

    return GBA_PPU_read_invalid(this, addr, sz, access, has_effect);
}

void GBA_PPU_mainbus_write_palette(struct GBA *this, u32 addr, u32 sz, u32 access, u32 val)
{
    if (addr < 0x05000400) {
        return cW[sz](this->ppu.palette_RAM, addr & 0x3FF, val);
    }

    GBA_PPU_write_invalid(this, addr, sz, access, val);
}

void GBA_PPU_mainbus_write_VRAM(struct GBA *this, u32 addr, u32 sz, u32 access, u32 val)
{
    if (addr < 0x06018000)
        return cW[sz](this->ppu.VRAM, addr - 0x06000000, val);

    GBA_PPU_write_invalid(this, addr, sz, access, val);
}

void GBA_PPU_mainbus_write_OAM(struct GBA *this, u32 addr, u32 sz, u32 access, u32 val)
{
    if (addr < 0x07000400)
        return cW[sz](this->ppu.OAM, addr & 0x3FF, val);

    GBA_PPU_write_invalid(this, addr, sz, access, val);
}

static u32 vcount(struct GBA *this)
{
    return (this->clock.ppu.y == this->ppu.io.vcount_at);
}

u32 GBA_PPU_mainbus_read_IO(struct GBA *this, u32 addr, u32 sz, u32 access, u32 has_effect)
{
    u32 v = 0;
    switch(addr) {
        case 0x04000000: {// DISPCTRL
            v = this->ppu.io.bg_mode;
            v |= (this->ppu.io.frame) << 4;
            v |= (this->ppu.io.hblank_free) << 5;
            v |= (this->ppu.io.obj_mapping_2d) << 6;
            v |= (this->ppu.io.force_blank) << 7;
            v |= (this->ppu.bg[0].enable) << 8;
            v |= (this->ppu.bg[1].enable) << 9;
            v |= (this->ppu.bg[2].enable) << 10;
            v |= (this->ppu.bg[3].enable) << 11;
            v |= (this->ppu.obj.enable) << 12;
            v |= (this->ppu.window[0].enable) << 13;
            v |= (this->ppu.window[1].enable) << 14;
            v |= (this->ppu.window[GBA_WINOBJ].enable) << 15;
            return v;}
        case 0x04000004: {// DISPSTAT
            v = this->clock.ppu.vblank_active;
            v |= this->clock.ppu.hblank_active << 1;
            v |= vcount(this);
            v |= this->ppu.io.vblank_irq_enable << 3;
            v |= this->ppu.io.hblank_irq_enable << 4;

            v |= this->ppu.io.vcount_irq_enable << 5;
            v |= (this->clock.ppu.y << 8);
            return v; }
        case 0x04000006: { // VCNT
            printf("\nREAD VCNT cyc:%lld", this->clock.master_cycle_count);
            return this->clock.ppu.y;
        }
    }
    return GBA_PPU_read_invalid(this, addr, sz, access, has_effect);
}

static void calc_screen_size(struct GBA *this, u32 num, u32 mode)
{
    if (mode >= 3) return;
    struct GBA_PPU_bg *bg = &this->ppu.bg[num];
    // mode0 they're all the same no scaling
    u32 scales = mode >= 1 ? (num > 1) :  0;
    // mode1 bg 0-1 no scaling, 2 scaling
    // mode2 is 2 and 3
#define T(scales, ssize, hrs, vrs) case (((scales) << 2) | (ssize)): bg->htiles = hrs; bg->vtiles = vrs; break
    switch((scales << 2) | bg->screen_size) {
        T(0, 0, 32, 32);
        T(1, 0, 16, 16);
        T(0, 1, 64, 32);
        T(1, 1, 32, 32);
        T(0, 2, 32, 64);
        T(1, 2, 64, 64);
        T(0, 3, 64, 64);
        T(1, 3, 128, 128);
    }
#undef T
    bg->htiles_mask = bg->htiles - 1;
    bg->vtiles_mask = bg->vtiles - 1;
    bg->hpixels = bg->htiles << 3;
    bg->vpixels = bg->vtiles << 3;
    bg->hpixels_mask = bg->hpixels - 1;
    bg->vpixels_mask = bg->vpixels - 1;
}

static void update_bg_x(struct GBA *this, u32 bgnum, u32 which, u32 val)
{
    u32 v = this->ppu.bg[bgnum].x & 0x0FFFFFFF;
    val &= 0xFF;
    switch(which) {
        case 0: v = (v & 0x0FFFFF00) | val; break;
        case 1: v = (v & 0x0FFF00FF) | (val << 8); break;
        case 2: v = (v & 0x0F00FFFF) | (val << 16); break;
        case 3: v = (v & 0x00FFFFFF) | ((val & 0x0F) << 24); break;
    }
    if ((v >> 27) & 1) v |= 0xF0000000;
    this->ppu.bg[bgnum].x = v;
}

static void update_bg_y(struct GBA *this, u32 bgnum, u32 which, u32 val)
{
    u32 v = this->ppu.bg[bgnum].y & 0x0FFFFFFF;
    val &= 0xFF;
    switch(which) {
        case 0: v = (v & 0x0FFFFF00) | val; break;
        case 1: v = (v & 0x0FFF00FF) | (val << 8); break;
        case 2: v = (v & 0x0F00FFFF) | (val << 16); break;
        case 3: v = (v & 0x00FFFFFF) | ((val & 0x0F) << 24); break;
    }
    if ((v >> 27) & 1) v |= 0xF0000000;
    this->ppu.bg[bgnum].y = v;
}

void GBA_PPU_mainbus_write_IO(struct GBA *this, u32 addr, u32 sz, u32 access, u32 val) {
    struct GBA_PPU *ppu = &this->ppu;
    switch(addr) {
        case 0x04000000: {// DISPCNT lo
            //printf("\nDISPCNT WRITE %04x", val);
            u32 new_mode = val & 7;
            if (new_mode >= 6) {
                printf("\nILLEGAL BG MODE:%d", new_mode);
                dbg_break("ILLEGAL BG MODE", this->clock.master_cycle_count);
            }
            else {
                if (new_mode != ppu->io.bg_mode) printf("\nBG MODE:%d", val & 7);
            }

            ppu->io.bg_mode = new_mode;
            calc_screen_size(this, 0, this->ppu.io.bg_mode);
            calc_screen_size(this, 1, this->ppu.io.bg_mode);
            calc_screen_size(this, 2, this->ppu.io.bg_mode);
            calc_screen_size(this, 3, this->ppu.io.bg_mode);

            ppu->io.frame = (val >> 4) & 1;
            this->ppu.io.hblank_free = (val >> 5) & 1;
            this->ppu.io.obj_mapping_2d = (val >> 6) & 1;
            ppu->io.force_blank = (val >> 7) & 1;
            return; }
        case 0x04000001: { // DISPCNT hi
            ppu->bg[0].enable = (val >> 0) & 1;
            ppu->bg[1].enable = (val >> 1) & 1;
            ppu->bg[2].enable = (val >> 2) & 1;
            ppu->bg[3].enable = (val >> 3) & 1;
            ppu->obj.enable = (val >> 4) & 1;
            ppu->window[0].enable = (val >> 5) & 1;
            ppu->window[1].enable = (val >> 6) & 1;
            ppu->window[GBA_WINOBJ].enable = (val >> 7) & 1;
            printf("\nBGs 0:%d 1:%d 2:%d 3:%d obj:%d window0:%d window1:%d force_hblank:%d",
                   ppu->bg[0].enable, ppu->bg[1].enable, ppu->bg[2].enable, ppu->bg[3].enable,
                   ppu->obj.enable, ppu->window[0].enable, ppu->window[1].enable, ppu->io.force_blank
                   );
            printf("\nOBJ mapping 2d:%d", ppu->io.obj_mapping_2d);
            return; }
        case 0x04000004: {// DISPSTAT
            this->ppu.io.vblank_irq_enable = (val >> 3) & 1;
            this->ppu.io.hblank_irq_enable = (val >> 4) & 1;
            this->ppu.io.vcount_irq_enable = (val >> 5) & 1;
            return; }
        case 0x04000005: return; // DISPSTAT hi

        case 0x04000008: // BG control
        case 0x0400000A:
        case 0x0400000C:
        case 0x0400000E: {
            u32 bgnum = (addr & 0b0110) >> 1;
            struct GBA_PPU_bg *bg = &this->ppu.bg[bgnum];
            bg->priority = val & 3;
            bg->character_base_block = ((val >> 2) & 3) << 14;
            bg->mosaic_enable = (val >> 6) & 1;
            bg->bpp8 = (val >> 7) & 1;
            return; }
        case 0x04000009: // BG control
        case 0x0400000B:
        case 0x0400000D:
        case 0x0400000F: {
            u32 bgnum = (addr & 0b0110) >> 1;
            struct GBA_PPU_bg *bg = &this->ppu.bg[bgnum];
            bg->screen_base_block = ((val >> 0) & 31) << 11;
            if (bgnum >= 2) bg->display_overflow = (val >> 5) & 1;
            bg->screen_size = (val >> 6) & 3;
            calc_screen_size(this, bgnum, this->ppu.io.bg_mode);
            return; }
#define BG2 2
#define BG3 3
        case 0x04000010: this->ppu.bg[0].hscroll = (this->ppu.bg[0].hscroll & 0xFF00) | val; return;
        case 0x04000011: this->ppu.bg[0].hscroll = (this->ppu.bg[0].hscroll & 0x00FF) | (val << 8); return;
        case 0x04000012: this->ppu.bg[0].vscroll = (this->ppu.bg[0].vscroll & 0xFF00) | val; return;
        case 0x04000013: this->ppu.bg[0].vscroll = (this->ppu.bg[0].vscroll & 0x00FF) | (val << 8); return;
        case 0x04000014: this->ppu.bg[1].hscroll = (this->ppu.bg[1].hscroll & 0xFF00) | val; return;
        case 0x04000015: this->ppu.bg[1].hscroll = (this->ppu.bg[1].hscroll & 0x00FF) | (val << 8); return;
        case 0x04000016: this->ppu.bg[1].vscroll = (this->ppu.bg[1].vscroll & 0xFF00) | val; return;
        case 0x04000017: this->ppu.bg[1].vscroll = (this->ppu.bg[1].vscroll & 0x00FF) | (val << 8); return;
        case 0x04000018: this->ppu.bg[2].hscroll = (this->ppu.bg[2].hscroll & 0xFF00) | val; return;
        case 0x04000019: this->ppu.bg[2].hscroll = (this->ppu.bg[2].hscroll & 0x00FF) | (val << 8); return;
        case 0x0400001A: this->ppu.bg[2].vscroll = (this->ppu.bg[2].vscroll & 0xFF00) | val; return;
        case 0x0400001B: this->ppu.bg[2].vscroll = (this->ppu.bg[2].vscroll & 0x00FF) | (val << 8); return;
        case 0x0400001C: this->ppu.bg[3].hscroll = (this->ppu.bg[3].hscroll & 0xFF00) | val; return;
        case 0x0400001D: this->ppu.bg[3].hscroll = (this->ppu.bg[3].hscroll & 0x00FF) | (val << 8); return;
        case 0x0400001E: this->ppu.bg[3].vscroll = (this->ppu.bg[3].vscroll & 0xFF00) | val; return;
        case 0x0400001F: this->ppu.bg[3].vscroll = (this->ppu.bg[3].vscroll & 0x00FF) | (val << 8); return;

        case 0x04000020: this->ppu.bg[2].pa = (this->ppu.bg[2].pa & 0xFF00) | val; return;
        case 0x04000021: this->ppu.bg[2].pa = (this->ppu.bg[2].pa & 0xFF) | (val << 8) | (((val >> 7) & 1) * 0xFFFF0000); return;
        case 0x04000022: this->ppu.bg[2].pb = (this->ppu.bg[2].pb & 0xFF00) | val; return;
        case 0x04000023: this->ppu.bg[2].pb = (this->ppu.bg[2].pb & 0xFF) | (val << 8) | (((val >> 7) & 1) * 0xFFFF0000); return;
        case 0x04000024: this->ppu.bg[2].pc = (this->ppu.bg[2].pc & 0xFF00) | val; return;
        case 0x04000025: this->ppu.bg[2].pc = (this->ppu.bg[2].pc & 0xFF) | (val << 8) | (((val >> 7) & 1) * 0xFFFF0000); return;
        case 0x04000026: this->ppu.bg[2].pd = (this->ppu.bg[2].pd & 0xFF00) | val; return;
        case 0x04000027: this->ppu.bg[2].pd = (this->ppu.bg[2].pd & 0xFF) | (val << 8) | (((val >> 7) & 1) * 0xFFFF0000); return;
        case 0x04000028: update_bg_x(this, BG2, 0, val); return;
        case 0x04000029: update_bg_x(this, BG2, 1, val); return;
        case 0x0400002A: update_bg_x(this, BG2, 2, val); return;
        case 0x0400002B: update_bg_x(this, BG2, 3, val); return;
        case 0x0400002C: update_bg_y(this, BG2, 0, val); return;
        case 0x0400002D: update_bg_y(this, BG2, 1, val); return;
        case 0x0400002E: update_bg_y(this, BG2, 2, val); return;
        case 0x0400002F: update_bg_y(this, BG2, 3, val); return;

        case 0x04000030: this->ppu.bg[3].pa = (this->ppu.bg[3].pa & 0xFF00) | val; return;
        case 0x04000031: this->ppu.bg[3].pa = (this->ppu.bg[3].pa & 0xFF) | (val << 8) | (((val >> 7) & 1) * 0xFFFF0000); return;
        case 0x04000032: this->ppu.bg[3].pb = (this->ppu.bg[3].pb & 0xFF00) | val; return;
        case 0x04000033: this->ppu.bg[3].pb = (this->ppu.bg[3].pb & 0xFF) | (val << 8) | (((val >> 7) & 1) * 0xFFFF0000); return;
        case 0x04000034: this->ppu.bg[3].pc = (this->ppu.bg[3].pc & 0xFF00) | val; return;
        case 0x04000035: this->ppu.bg[3].pc = (this->ppu.bg[3].pc & 0xFF) | (val << 8) | (((val >> 7) & 1) * 0xFFFF0000); return;
        case 0x04000036: this->ppu.bg[3].pd = (this->ppu.bg[3].pd & 0xFF00) | val; return;
        case 0x04000037: this->ppu.bg[3].pd = (this->ppu.bg[3].pd & 0xFF) | (val << 8) | (((val >> 7) & 1) * 0xFFFF0000); return;
        case 0x04000038: update_bg_x(this, BG3, 0, val); return;
        case 0x04000039: update_bg_x(this, BG3, 1, val); return;
        case 0x0400003A: update_bg_x(this, BG3, 2, val); return;
        case 0x0400003B: update_bg_x(this, BG3, 3, val); return;
        case 0x0400003C: update_bg_y(this, BG3, 0, val); return;
        case 0x0400003D: update_bg_y(this, BG3, 1, val); return;
        case 0x0400003E: update_bg_y(this, BG3, 2, val); return;
        case 0x0400003F: update_bg_y(this, BG3, 3, val); return;
#undef BG2
#undef BG3
        }

    GBA_PPU_write_invalid(this, addr, sz, access, val);
}


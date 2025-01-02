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
#include "helpers/color.h"
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
        //if (!(this->io.IF & 1)) printf("\nVBLANK IRQ frame:%lld line:%d cyc:%lld", this->clock.master_frame, this->clock.ppu.y, this->clock.master_cycle_count);
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
            //if (!(this->io.IF & 4)) printf("\nVCOUNT IRQ frame:%lld line:%d cyc:%lld", this->clock.master_frame, this->clock.ppu.y, this->clock.master_cycle_count);
            this->io.IF |= 4;
            GBA_eval_irqs(this);
        }
    }
    if ((val == 1) && (this->clock.ppu.y < 160)) {
        //if (!(this->io.IF & 2)) printf("\nHBLANK IRQ frame:%lld line:%d cyc:%lld", this->clock.master_frame, this->clock.ppu.y, this->clock.master_cycle_count);
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
    u32 block_x = px >> 3;
    u32 block_y = py >> 3;
    u32 line_in_tile = py & 7;

    u32 tile_width = bg->htiles;

    u32 screenblock_addr = bg->screen_base_block;
    screenblock_addr += block_x + (block_y * tile_width);
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
static void get_affine_sprite_pixel(struct GBA *this, u32 mode, i32 px, i32 py, u32 tile_num, u32 htiles, u32 vtiles, u32 bpp8, u32 palette, u32 priority, u32 obj_mapping_1d, u32 dsize, i32 screen_x, u32 blended, struct GBA_PPU_window *w)
{
    i32 hpixels = htiles * 8;
    i32 vpixels = vtiles * 8;
    if (dsize) {
        hpixels >>= 1;
        vpixels >>= 1;
    }
    px += (hpixels >> 1);
    py += (vpixels >> 1);
    if ((px < 0) || (py < 0)) return;
    if ((px >= hpixels) || (py > vpixels)) return;

    u32 block_x = px >> 3;
    u32 block_y = py >> 3;
    u32 tile_x = px & 7;
    u32 tile_y = py & 7;
    if (obj_mapping_1d) {
        tile_num += ((htiles >> dsize) * block_y);
    }
    else {
        tile_num += ((32 << bpp8) * block_y);
    }
    tile_num += block_x;
    tile_num &= 0x3FF;

    u32 tile_base_addr = 0x10000 + (32 * tile_num);
    u32 tile_line_addr = tile_base_addr + ((4 << bpp8) * tile_y);
    u32 tile_px_addr = bpp8 ? (tile_line_addr + tile_x) : (tile_line_addr + (tile_x >> 1));
    u8 c = this->ppu.VRAM[tile_px_addr];

    if (!bpp8) {
        if (tile_x & 1) c >>= 4;
        c &= 15;
    }
    else palette = 0x100;

    if (c != 0) {
        switch(mode) {
            case 1:
            case 0: {
                struct GBA_PX *opx = &this->ppu.obj.line[screen_x];
                if (!opx->has) {
                    opx->has = 1;
                    opx->priority = priority;
                    opx->palette = palette; // 0x100+ already accounter for
                    opx->color = c;
                    opx->bpp8 = bpp8;
                    opx->blended = blended;
                }
                break; }
            case 2: {
                w->inside[screen_x] = 1;
            break; }
        }
    }
}

static u32 get_sprite_tile_addr(struct GBA *this, u32 tile_num, u32 htiles, u32 block_y, u32 line_in_tile, u32 bpp8, u32 d1)
{
    u32 base_addr = 0x10000;
    if (d1) {
        tile_num += block_y * (htiles << bpp8);
        tile_num &= 0x3FF;
        u32 tile_addr = base_addr + ((32 * tile_num) << bpp8);
        tile_addr += (line_in_tile * (4 << bpp8));
        return tile_addr;
    }
    tile_num += block_y * (32 << bpp8);
    tile_num &= 0x3FF;
    u32 tile_addr = base_addr + (32 * tile_num);
    tile_addr += (line_in_tile * (4 << bpp8));
    return tile_addr;
}

static void draw_sprite_affine(struct GBA *this, u16 *ptr, struct GBA_PPU_window *w, u32 num)
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
    u32 blended = mode == 1;
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
    u32 palette = bpp8 ? 0x100 : (0x100 + (((ptr[2] >> 12) & 15) << 4));
    if (bpp8) tile_num &= 0x3FE;

    i32 line_in_sprite = (i32)this->clock.ppu.y - y;
    //i32 x_origin = x - (htiles << 2);

    i32 screen_x = x;
    i32 iy = (i32)line_in_sprite - ((i32)vtiles * 4);
    i32 hwidth = (i32)htiles * 4;
        for (i32 ix=-hwidth; ix < hwidth; ix++) {
        i32 px = (pa*ix + pb*iy)>>8;    // get x texture coordinate
        i32 py = (pc*ix + pd*iy)>>8;    // get y texture coordinate
        if ((screen_x >= 0) && (screen_x < 240))
            get_affine_sprite_pixel(this, mode, px, py, tile_num, htiles, vtiles, bpp8, palette, priority,
                                    this->ppu.io.obj_mapping_1d, dsize, screen_x, blended, w);   // get color from (px,py)
        screen_x++;
        this->ppu.obj.drawing_cycles -= 2;
        if (this->ppu.obj.drawing_cycles < 0) return;
    }
}

static void output_sprite_8bpp(struct GBA *this, u8 *tptr, u32 mode, i32 screen_x, u32 priority, u32 hflip, u32 blended, struct GBA_PPU_window *w) {
    for (i32 tile_x = 0; tile_x < 8; tile_x++) {
        i32 sx = tile_x + screen_x;
        if ((sx >= 0) && (sx < 240)) {
            this->ppu.obj.drawing_cycles -= 1;
            struct GBA_PX *opx = &this->ppu.obj.line[sx];
            if ((mode > 1) || (!opx->has)) {
                u32 rpx = hflip ? tile_x : (7 - tile_x);
                u16 c = tptr[rpx];
                if (c != 0) {
                    switch (mode) {
                        case 1:
                        case 0:
                            opx->blended = blended;
                            opx->color = c;
                            opx->bpp8 = 1;
                            opx->priority = priority;
                            opx->has = 1;
                            opx->palette = 0x100;
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


static void output_sprite_4bpp(struct GBA *this, u8 *tptr, u32 mode, i32 screen_x, u32 priority, u32 hflip, u32 palette, u32 blended, struct GBA_PPU_window *w) {
    for (i32 tile_x = 0; tile_x < 4; tile_x++) {
        i32 sx;
        if (hflip) sx = (7 - (tile_x * 2)) + screen_x;
        else sx = (tile_x * 2) + screen_x;
        u8 data = tptr[tile_x];
        for (u32 i = 0; i < 2; i++) {
            if ((sx >= 0) && (sx < 240)) {
                this->ppu.obj.drawing_cycles -= 1;
                struct GBA_PX *opx = &this->ppu.obj.line[sx];
                if ((mode > 2) || (!opx->has)) {
                    u32 c = data & 15;
                    data >>= 4;
                    if (c != 0) {
                        switch (mode) {
                            case 1:
                            case 0: {
                                opx->color = c;
                                opx->bpp8 = 0;
                                opx->blended = blended;
                                opx->priority = priority;
                                opx->has = 1;
                                opx->palette = 0x100 + (palette << 4);
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
            }
            if (hflip) sx--;
            else sx++;
        }
    }
}

static void draw_sprite_normal(struct GBA *this, u16 *ptr, struct GBA_PPU_window *w, u32 num)
{
    // 1 cycle to evaluate and 1 cycle per pixel
    this->ppu.obj.drawing_cycles -= 1;
    if (this->ppu.obj.drawing_cycles < 1) return;

    u32 obj_disable = (ptr[0] >> 9) & 1;
    if (obj_disable) return;

    u32 shape = (ptr[0] >> 14) & 3; // 0 = square, 1 = horiozontal, 2 = vertical
    u32 sz = (ptr[1] >> 14) & 3;
    u32 htiles, vtiles;
    get_obj_tile_size(sz, shape, &htiles, &vtiles);

    i32 y_min = ptr[0] & 0xFF;
    i32 y_max = (y_min + ((i32)vtiles * 8)) & 255;
    if(y_max < y_min)
        y_min -= 256;

    if((i32)this->clock.ppu.y < y_min || (i32)this->clock.ppu.y >= y_max) {
        return;
    }

    // Clip sprite

    i32 y_bottom = y_min + (i32)(vtiles * 8);
    u32 tile_num = ptr[2] & 0x3FF;
    if ((this->ppu.io.bg_mode >= 3) && (tile_num < 512)) {
        return;
    }

    u32 mode = (ptr[0] >> 10) & 3;
    u32 blended = mode == 1;
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
    i32 line_in_sprite = this->clock.ppu.y - y_min;
    //printf("\nPPU LINE %d LINE IN SPR:%d Y:%d", this->clock.ppu.y, line_in_sprite, y);
    if (vflip) line_in_sprite = (((vtiles * 8) - 1) - line_in_sprite);
    u32 tile_y_in_sprite = line_in_sprite >> 3; // /8
    u32 line_in_tile = line_in_sprite & 7;

    // OK so we know which line
    // We have two possibilities; 1d or 2d layout
    u32 tile_addr = get_sprite_tile_addr(this, tile_num, htiles, tile_y_in_sprite, line_in_tile, bpp8, this->ppu.io.obj_mapping_1d);
    if (hflip) tile_addr += (htiles - 1) * 32;
    //if (y_min != -1) printf("\nSPRITE%d y:%d PALETTE:%d", num, y_min, palette);

    i32 screen_x = x;
    for (u32 tile_xs = 0; tile_xs < htiles; tile_xs++) {
        u8 *tptr = ((u8 *) this->ppu.VRAM) + tile_addr;
        if (hflip) tile_addr -= 32;
        else tile_addr += 32;
        if (bpp8) output_sprite_8bpp(this, tptr, mode, screen_x, priority, hflip, blended, w);
        else output_sprite_4bpp(this, tptr, mode, screen_x, priority, hflip, palette, blended, w);
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
        if (affine) draw_sprite_affine(this, ptr, w, i);
        else draw_sprite_normal(this, ptr, w, i);
    }
}

static void fetch_bg_slice(struct GBA *this, struct GBA_PPU_bg *bg, u32 bgnum, u32 block_x, u32 vpos, struct GBA_PX px[8])
{
    u32 block_y = vpos >> 3;
    u32 screenblock_addr = bg->screen_base_block + (se_index_fast(block_x, block_y, bg->screen_size) << 1);

    u16 attr = *(u16 *)(((u8 *)this->ppu.VRAM) + screenblock_addr);
    u32 tile_num = attr & 0x3FF;
    u32 hflip = (attr >> 10) & 1;
    u32 vflip = (attr >> 11) & 1;
    u32 palbank = bg->bpp8 ? 0 : ((attr >> 12) & 15) << 4;

    u32 line_in_tile = vpos & 7;
    if (vflip) line_in_tile = 7 - line_in_tile;

    u32 tile_bytes = bg->bpp8 ? 64 : 32;
    u32 line_size = bg->bpp8 ? 8 : 4;
    u32 tile_start_addr = bg->character_base_block + (tile_num * tile_bytes);
    u32 line_addr = tile_start_addr + (line_in_tile * line_size);
    if (line_addr >= 0x10000) return; // hardware doesn't draw from up there
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

static void calculate_windows(struct GBA *this, u32 force)
{
    //if (!force && !this->ppu.window[0].enable && !this->ppu.window[1].enable && !this->ppu.window[GBA_WINOBJ].enable) return;

    // Calculate windows...
    if (this->clock.ppu.y >= 160) return;
    struct GBA_PPU_window *w;
    for (u32 wn = 0; wn < 2; wn++) {
        w = &this->ppu.window[wn];
        if (!w->enable) {
            memset(&w->inside, 0, sizeof(w->inside));
            continue;
        }

        if (w->left == w->right) {
            memset(&w->inside, 0, sizeof(w->inside));
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
    u32 w0r = w0->enable;
    u32 w1r = w1->enable;
    u32 wsr = ws->enable;
    w = &this->ppu.window[GBA_WINOUTSIDE];
    memset(w->inside, 0, sizeof(w->inside));
    for (u32 x = 0; x < 240; x++) {
        u32 w0i = w0r & w0->inside[x];
        u32 w1i = w1r & w1->inside[x];
        u32 wsi = wsr & ws->inside[x];
        w->inside[x] = !(w0i || w1i || wsi);
    }

    struct GBA_PPU_window *wo = &this->ppu.window[GBA_WINOUTSIDE];
    struct GBA_DBG_line *l = &this->dbg_info.line[this->clock.ppu.y];
    for (u32 x = 0; x < 240; x++) {
        l->window_coverage[x] = w0->inside[x] | (w1->inside[x] << 1) | (ws->inside[x] << 2) | (wo->inside[x] << 3);
    }
}

static void apply_windows(struct GBA *this, u32 sp_window, u32 bgtest[4]) {
    //if (!this->ppu.window[0].enable && !this->ppu.window[1].enable && !this->ppu.window[GBA_WINOBJ].enable) return;
    if (!(this->ppu.window[0].enable & !this->ppu.window[0].special_effect) &&
            !(this->ppu.window[1].enable & !this->ppu.window[1].special_effect) &&
            !(this->ppu.window[2].enable & !this->ppu.window[2].special_effect)) return;
    // Each window can be individually enable/disabled
    // Each background can be individually selected
    // We shouldn't cut pixel if window is disabled, or enabled with special effect
    static u32 priorities[4] = {GBA_WINOUTSIDE, GBA_WINOBJ, GBA_WIN1, GBA_WIN0};
    u8 *wprio[4] = {this->ppu.window[0].inside, this->ppu.window[1].inside, this->ppu.window[2].inside,
                    this->ppu.window[3].inside};
    for (u32 bgnum = 0; bgnum < 3; bgnum++) {
        struct GBA_PPU_bg *bg = &this->ppu.bg[bgnum];
        if (!bg->enable || !bgtest[bgnum]) continue;
        u32 bginclude[4] = {this->ppu.window[0].bg[bgnum], this->ppu.window[1].bg[bgnum], this->ppu.window[2].bg[bgnum],
                            this->ppu.window[3].bg[bgnum]};
        for (u32 x = 0; x < 240; x++) {
            u32 v = 0;
            u32 se = 0;
            for (u32 i = 0; i < 4; i++) {
                se |= bginclude[i] & this->ppu.window[i].special_effect;
                v |= bginclude[i] && wprio[i][x];
            }
            if (!v) {
                struct GBA_PX *p = &bg->line[x];
                if (se)
                    p->blended = 1;
                else
                    p->has = 0;
            }
        }
    }
    if (!sp_window) return;
    u32 spinclude[4] = { this->ppu.window[0].obj, this->ppu.window[1].obj, this->ppu.window[2].obj, this->ppu.window[3].obj};
    for (u32 x = 0; x < 240; x++) {
        u32 v = 0, se = 0;
        for (u32 i = 0; i < 4; i++) {
            se |= spinclude[i] & this->ppu.window[i].special_effect;
            v |= spinclude[i] && wprio[i][x];
        }
        if (!v) {
            struct GBA_PX *p = &this->ppu.obj.line[x];
            if (se)
                p->blended = 1;
            else
                p->has = 0;
        }
    }
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

static void find_targets_and_priorities(struct GBA *this, u32 bg_enables[6], struct GBA_PX *layers[6], u32 *layer_a_out, u32 *layer_b_out, i32 x)
{
    u32 laout = 5;
    u32 lbout = 5;
    u32 second = 5;
    // Get highest priority 1st target
    for (i32 priority = 3; priority >= 0; priority--) {
        for (i32 i = 4; i >= 0; i--) {
            if (bg_enables[i] && (this->ppu.blend.targets_a[i] || layers[i]->blended) && layers[i]->has && (layers[i]->priority == priority)) {
                laout = i;
            }
            if (bg_enables[i] && (this->ppu.blend.targets_b[i] || layers[i]->blended) && layers[i]->has && (layers[i]->priority == priority)) {
                second = lbout;
                lbout = i;
            }
        }
    }
    //if ((this->clock.ppu.y == 50) && (x == 30)) printf("\nlaout:%d lbout:%d bg_enable2:%d l2_has:%d l2_blended:%d l2_priority:%d", laout, lbout, bg_enables[2], layers[2]->has, layers[2]->blended, layers[2]->priority);
    if (second == 5) second = lbout;
    if (laout == lbout) lbout = second;
    *layer_a_out = laout;
    *layer_b_out = lbout;
}

static void output_pixel(struct GBA *this, u32 x, u32 obj_enable, u32 bg_enables[4], u16 *line_output) {
    struct GBA_PX *sp = &this->ppu.obj.line[x];
    //if (this->clock.ppu.y == 32) c = 0x001F;
    struct GBA_PX *highest_priority_bg_px = NULL;
    u32 bg_priority = 5;
    struct GBA_PX *mp;
    if (this->ppu.blend.mode == 0) {
        for (u32 i = 0; i < 4; i++) {
            if (!bg_enables[i]) continue;
            mp = &this->ppu.bg[i].line[x];
            if ((mp->has) && (mp->priority < bg_priority)) {
                highest_priority_bg_px = mp;
                bg_priority = mp->priority;
            }
        }
        struct GBA_PX *op = NULL;
        if ((sp->has) && (sp->priority <= bg_priority)) {
            // do sprite!
            op = sp;
        } else if (highest_priority_bg_px != NULL) {
            // do bg!
            op = highest_priority_bg_px;
        }
        u16 c;
        if (op == NULL) {
            c = 0;
        } else {
            c = this->ppu.palette_RAM[op->palette + op->color];
        }
        line_output[x] = c;
    } else {
        // Get two highest-priority.
        // If a sprite is marked as semi-transparent, it is forced into target A regardless of priority, and target A is forced into B
        struct GBA_PX *target_a = NULL, *target_b = NULL;
        struct GBA_PX *sp_px = &this->ppu.obj.line[x];
        struct GBA_PX empty_px = {.bpp8 = 1, .color=0, .palette=0, .priority=0, .blended=1, .has=1, .blend_color = 0};
        sp_px->has &= obj_enable;

        struct GBA_PX *layers[6] = {
                sp_px,
                &this->ppu.bg[0].line[x],
                &this->ppu.bg[1].line[x],
                &this->ppu.bg[2].line[x],
                &this->ppu.bg[3].line[x],
                &empty_px
        };
        u32 obg_enables[6] = {
                obj_enable,
                bg_enables[0],
                bg_enables[1],
                bg_enables[2],
                bg_enables[3],
                1
        };
        u32 mode = this->ppu.blend.mode;
        /* Find targets A and B */
        target_a = &empty_px;
        target_b = &empty_px;
        u32 target_a_layer, target_b_layer;
        find_targets_and_priorities(this, obg_enables, layers, &target_a_layer, &target_b_layer, x);

        target_a = layers[target_a_layer];
        target_b = layers[target_b_layer];

        if (sp_px->has && sp_px->blended) {
            mode = 1; // force to alpha blend
            target_b = target_a;
            target_a = sp_px;
        }

        u32 output_color = (0x1f << 10) | 0x1F; // purple!
        target_a->blend_color = this->ppu.palette_RAM[target_a->palette + target_a->color];
        if ((target_a == sp_px) && (target_a->blended == 0)) {
            output_color = target_a->blend_color;
        }
        else {
            //if ((this->clock.ppu.y == 50) && (x == 30)) printf("\nMODE:%d a:%d b:%d EVA:%d EVB:%d BTA:%d,%d,%d,%d,%d,%d BTB:%d,%d,%d,%d,%d,%d", mode, target_a_layer, target_b_layer, this->ppu.blend.eva_a, this->ppu.blend.eva_b,
            //                                                   this->ppu.blend.targets_a[0], this->ppu.blend.targets_a[1], this->ppu.blend.targets_a[2], this->ppu.blend.targets_a[3], this->ppu.blend.targets_a[4], this->ppu.blend.targets_a[5],
            //                                                    this->ppu.blend.targets_b[0], this->ppu.blend.targets_b[1], this->ppu.blend.targets_b[2], this->ppu.blend.targets_b[3], this->ppu.blend.targets_b[4], this->ppu.blend.targets_b[5]);
            // Resolve output colors of up to two layers...
            target_b->blend_color = this->ppu.palette_RAM[target_b->palette + target_b->color];
            switch (mode) {
                case 1: {// alpha-blend!
                    if (target_a == target_b) {
                        output_color = target_a->blend_color;
                    } else {
                        output_color = gba_alpha(target_a->blend_color, target_b->blend_color, this->ppu.blend.eva_a,
                                                 this->ppu.blend.eva_b);
                    }
                    break;
                }
                case 2: { // brightness increase
                    output_color = gba_brighten(target_a->blend_color, this->ppu.blend.bldy);
                    break;
                }
                case 3: {
                    //if (x == 50)
                    output_color = gba_darken(target_a->blend_color, this->ppu.blend.bldy);
                    break;
                }
            }
        }
        line_output[x] = output_color;
    }
    // line_output[x] = (31 << 10) | 31;
}

static void draw_line0(struct GBA *this, u16 *line_output)
{
    draw_obj_line(this);
    draw_bg_line_normal(this, 0);
    draw_bg_line_normal(this, 1);
    draw_bg_line_normal(this, 2);
    draw_bg_line_normal(this, 3);
    calculate_windows(this, 0);
    apply_windows(this, 1, (u32[4]){1, 1, 1, 1});
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
    memset(&this->ppu.bg[3].line, 0, sizeof(this->ppu.bg[3].line));

    calculate_windows(this, 0);
    apply_windows(this, 1, (u32[4]){1, 1, 1, 0});
    u32 bg_enables[4] = {this->ppu.bg[0].enable, this->ppu.bg[1].enable, this->ppu.bg[2].enable, 0};
    for (u32 x = 0; x < 240; x++) {
        output_pixel(this, x, this->ppu.obj.enable, &bg_enables[0], line_output);
    }
}

static void draw_line2(struct GBA *this, u16 *line_output)
{
    draw_obj_line(this);
    memset(&this->ppu.bg[0].line, 0, sizeof(this->ppu.bg[0].line));
    memset(&this->ppu.bg[1].line, 0, sizeof(this->ppu.bg[1].line));
    draw_bg_line_affine(this, 2);
    draw_bg_line_affine(this, 3);
    calculate_windows(this, 0);
    apply_windows(this, 1, (u32[4]){0, 0, 1, 1});
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
            c = this->ppu.palette_RAM[sp->palette + c];
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

        // Copy debug data
        struct GBA_DBG_line *l = &this->dbg_info.line[this->clock.ppu.y];
        l->bg_mode = this->ppu.io.bg_mode;
        for (u32 bgn = 0; bgn < 3; bgn++) {
            struct GBA_PPU_bg *bg = &this->ppu.bg[bgn];
            struct GBA_DBG_line_bg *mdbg = &l->bg[bgn];
            mdbg->htiles = bg->htiles;
            mdbg->vtiles = bg->vtiles;
            mdbg->hscroll = bg->hscroll;
            mdbg->vscroll = bg->vscroll;
            mdbg->display_overflow = bg->display_overflow;
            mdbg->screen_base_block = bg->screen_base_block;
            mdbg->character_base_block = bg->character_base_block;
            mdbg->bpp8 = bg->bpp8;
            mdbg->priority = bg->priority;
            if (bgn >= 2) {
                mdbg->hpos = bg->x;
                mdbg->vpos = bg->y;
                mdbg->pa = bg->pa;
                mdbg->pb = bg->pb;
                mdbg->pc = bg->pc;
                mdbg->pd = bg->pd;
            }
        }

        // Actually draw the line
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
        // Copy window stuff...

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
    addr &= 0x1FFFF;
    if (addr < 0x18000)
        return cR[sz](this->ppu.VRAM, addr);
    else
        return cR[sz](this->ppu.VRAM, addr - 0x8000);
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
    addr &= 0x1FFFF;
    if (addr < 0x18000)
        return cW[sz](this->ppu.VRAM, addr, val);
    else
        return cW[sz](this->ppu.VRAM, addr - 0x8000, val);

    /*if (addr < 0x06018000)
        return cW[sz](this->ppu.VRAM, addr - 0x06000000, val);*/

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
        case 0x04000000: // DISPCTRL lo
            v = this->ppu.io.bg_mode;
            v |= (this->ppu.io.frame) << 4;
            v |= (this->ppu.io.hblank_free) << 5;
            v |= (this->ppu.io.obj_mapping_1d) << 6;
            v |= (this->ppu.io.force_blank) << 7;
            return v;
        case 0x04000001: // DISPCTRL hi
            v = (this->ppu.bg[0].enable);
            v |= (this->ppu.bg[1].enable) << 1;
            v |= (this->ppu.bg[2].enable) << 2;
            v |= (this->ppu.bg[3].enable) << 3;
            v |= (this->ppu.obj.enable) << 4;
            v |= (this->ppu.window[0].enable) << 5;
            v |= (this->ppu.window[1].enable) << 6;
            v |= (this->ppu.window[GBA_WINOBJ].enable) << 7;
            return v;
        case 0x04000004: // DISPSTAT lo
            v = this->clock.ppu.vblank_active;
            v |= this->clock.ppu.hblank_active << 1;
            v |= vcount(this);
            v |= this->ppu.io.vblank_irq_enable << 3;
            v |= this->ppu.io.hblank_irq_enable << 4;

            v |= this->ppu.io.vcount_irq_enable << 5;
            return v;
        case 0x04000005: // DISPSTAT hi
            v = this->clock.ppu.y;
            return v;
        case 0x04000006: // VCNT lo
            return this->clock.ppu.y;
        case 0x04000007: // VCNT hi
            return 0;
        case 0x04000008: // BG control lo
        case 0x0400000A:
        case 0x0400000C:
        case 0x0400000E: {
            u32 bgnum = (addr & 0b0110) >> 1;
            struct GBA_PPU_bg *bg = &this->ppu.bg[bgnum];
            v = bg->priority;
            v |= (bg->character_base_block >> 12);
            v |= bg->mosaic_enable << 6;
            v |= bg->bpp8 << 7;
            return v; }
        case 0x04000009: // BG control
        case 0x0400000B:
        case 0x0400000D:
        case 0x0400000F: {
            u32 bgnum = (addr & 0b0110) >> 1;
            struct GBA_PPU_bg *bg = &this->ppu.bg[bgnum];
            v = bg->screen_base_block >> 11;
            if (bgnum >= 2) v |= bg->display_overflow << 5;
            v |= bg->screen_size << 6;
            return v; }

        case 0x04000050:
            return this->ppu.blend.eva_a;
        case 0x04000051:
            return this->ppu.blend.eva_b;

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
            this->ppu.io.obj_mapping_1d = (val >> 6) & 1;
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
            /*printf("\nBGs 0:%d 1:%d 2:%d 3:%d obj:%d window0:%d window1:%d force_hblank:%d",
                   ppu->bg[0].enable, ppu->bg[1].enable, ppu->bg[2].enable, ppu->bg[3].enable,
                   ppu->obj.enable, ppu->window[0].enable, ppu->window[1].enable, ppu->io.force_blank
                   );
            printf("\nOBJ mapping 2d:%d", ppu->io.obj_mapping_2d);*/
            return; }
        case 0x04000002:
            printf("\nGREEN SWAP? %d", val);
            return;
        case 0x04000003:
            printf("\nGREEN SWAP2? %d", val);
            return;
        case 0x04000004: {// DISPSTAT
            this->ppu.io.vblank_irq_enable = (val >> 3) & 1;
            this->ppu.io.hblank_irq_enable = (val >> 4) & 1;
            this->ppu.io.vcount_irq_enable = (val >> 5) & 1;
            return; }
        case 0x04000005: return; // DISPSTAT hi
        case 0x04000006: return; // not used
        case 0x04000007: return; // not used

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

        case 0x04000040: this->ppu.window[0].right = val; return;
        case 0x04000041: this->ppu.window[0].left = val; return;
        case 0x04000042: this->ppu.window[1].right = val; return;
        case 0x04000043: this->ppu.window[1].left = val; return;
        case 0x04000044: this->ppu.window[0].bottom = val; return;
        case 0x04000045: this->ppu.window[0].top = val; return;
        case 0x04000046: this->ppu.window[1].bottom = val; return;
        case 0x04000047: this->ppu.window[1].top = val; return;

        case 0x04000048:
            this->ppu.window[0].bg[0] = (val >> 0) & 1;
            this->ppu.window[0].bg[1] = (val >> 1) & 1;
            this->ppu.window[0].bg[2] = (val >> 2) & 1;
            this->ppu.window[0].bg[3] = (val >> 3) & 1;
            this->ppu.window[0].obj = (val >> 4) & 1;
            this->ppu.window[0].special_effect = (val >> 5) & 1;
            return;
        case 0x04000049:
            this->ppu.window[1].bg[0] = (val >> 0) & 1;
            this->ppu.window[1].bg[1] = (val >> 1) & 1;
            this->ppu.window[1].bg[2] = (val >> 2) & 1;
            this->ppu.window[1].bg[3] = (val >> 3) & 1;
            this->ppu.window[1].obj = (val >> 4) & 1;
            this->ppu.window[1].special_effect = (val >> 5) & 1;
            return;
        case 0x0400004A:
            this->ppu.window[GBA_WINOUTSIDE].bg[0] = (val >> 0) & 1;
            this->ppu.window[GBA_WINOUTSIDE].bg[1] = (val >> 1) & 1;
            this->ppu.window[GBA_WINOUTSIDE].bg[2] = (val >> 2) & 1;
            this->ppu.window[GBA_WINOUTSIDE].bg[3] = (val >> 3) & 1;
            this->ppu.window[GBA_WINOUTSIDE].obj = (val >> 4) & 1;
            this->ppu.window[GBA_WINOUTSIDE].special_effect = (val >> 5) & 1;
            return;
        case 0x0400004B:
            this->ppu.window[GBA_WINOBJ].bg[0] = (val >> 0) & 1;
            this->ppu.window[GBA_WINOBJ].bg[1] = (val >> 1) & 1;
            this->ppu.window[GBA_WINOBJ].bg[2] = (val >> 2) & 1;
            this->ppu.window[GBA_WINOBJ].bg[3] = (val >> 3) & 1;
            this->ppu.window[GBA_WINOBJ].obj = (val >> 4) & 1;
            this->ppu.window[GBA_WINOBJ].special_effect = (val >> 5) & 1;
            return;
        case 0x0400004C:
            this->ppu.mosaic.bg.hsize = (val & 15) + 1;
            this->ppu.mosaic.bg.vsize = ((val >> 4) & 15) + 1;
            return;
        case 0x0400004D:
            this->ppu.mosaic.obj.hsize = (val & 15) + 1;
            this->ppu.mosaic.obj.vsize = ((val >> 4) & 15) + 1;
            return;
#define BT_BG0 0
#define BT_BG1 1
#define BT_BG2 2
#define BT_BG3 3
#define BT_OBJ 4
#define BT_BD 5

        case 0x04000050:
            this->ppu.blend.mode = (val >> 6) & 3;
            // sp, bg0, bg1, bg2, bg3, bd
            this->ppu.blend.targets_a[0] = (val >> 4) & 1;
            this->ppu.blend.targets_a[1] = (val >> 0) & 1;
            this->ppu.blend.targets_a[2] = (val >> 1) & 1;
            this->ppu.blend.targets_a[3] = (val >> 2) & 1;
            this->ppu.blend.targets_a[4] = (val >> 3) & 1;
            this->ppu.blend.targets_a[5] = (val >> 5) & 1;
            return;
        case 0x04000051:
            this->ppu.blend.targets_b[0] = (val >> 4) & 1;
            this->ppu.blend.targets_b[1] = (val >> 0) & 1;
            this->ppu.blend.targets_b[2] = (val >> 1) & 1;
            this->ppu.blend.targets_b[3] = (val >> 2) & 1;
            this->ppu.blend.targets_b[4] = (val >> 3) & 1;
            this->ppu.blend.targets_b[5] = (val >> 5) & 1;
            return;

        case 0x04000052:
            this->ppu.blend.eva_a = val & 31;
            return;
        case 0x04000053:
            this->ppu.blend.eva_b = val & 31;
            return;
        case 0x04000054:
            this->ppu.blend.bldy = val & 31;
            return;
        case 0x04000055:
            // TODO: support this stuff
            return;
        case 0x0400004e: // not used
        case 0x0400004f: // not used
        case 0x04000056: // not used
        case 0x04000057: // not used
        case 0x04000058: // not used
        case 0x04000059: // not used
        case 0x0400005A: // not used
        case 0x0400005B: // not used
        case 0x0400005C: // not used
        case 0x0400005D: // not used
        case 0x0400005E: // not used
        case 0x0400005F: // not used

            return;

#undef BG2
#undef BG3
        }

    GBA_PPU_write_invalid(this, addr, sz, access, val);
}


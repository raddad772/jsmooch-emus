//
// Created by . on 12/4/24.
//

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "helpers/debug.h"
#include "helpers/debugger/debugger.h"
#include "gba_bus.h"
#include "helpers/color.h"
#include "helpers/multisize_memaccess.c"

#include "gba_ppu.h"
#include "gba_debugger.h"

#define MASTER_CYCLES_PER_SCANLINE 1232
#define HBLANK_CYCLES 226
#define MASTER_CYCLES_BEFORE_HBLANK (MASTER_CYCLES_PER_SCANLINE - HBLANK_CYCLES)
#define MASTER_CYCLES_PER_FRAME (228 * MASTER_CYCLES_PER_SCANLINE)
#define SCANLINE_HBLANK 1006

void GBA_PPU_init(struct GBA *this)
{
    memset(&this->ppu, 0, sizeof(this->ppu));
    this->ppu.mosaic.bg.hsize = this->ppu.mosaic.bg.vsize = 1;
    this->ppu.mosaic.obj.hsize = this->ppu.mosaic.obj.vsize = 1;
    this->ppu.bg[2].pa = 1 << 8; this->ppu.bg[2].pd = 1 << 8;
    this->ppu.bg[3].pa = 1 << 8; this->ppu.bg[3].pd = 1 << 8;
}

void GBA_PPU_delete(struct GBA *this)
{

}

void GBA_PPU_reset(struct GBA *this)
{
    this->ppu.mosaic.bg.hsize = this->ppu.mosaic.bg.vsize = 1;
    this->ppu.mosaic.obj.hsize = this->ppu.mosaic.obj.vsize = 1;
    this->ppu.bg[2].pa = 1 << 8; this->ppu.bg[2].pd = 1 << 8;
    this->ppu.bg[3].pa = 1 << 8; this->ppu.bg[3].pd = 1 << 8;
}

static void vblank(void *ptr, u64 val, u64 clock, u32 jitter)
{
    //pprint_palette_ram(this);
    struct GBA *this = (struct GBA *)ptr;
    this->clock.ppu.vblank_active = val;
    if (val) {
        GBA_check_dma_at_vblank(this);
        u32 old_IF = this->io.IF;
        this->io.IF |= this->ppu.io.vblank_irq_enable && val;
        if (old_IF != this->io.IF) {
            DBG_EVENT(DBG_GBA_EVENT_SET_VBLANK_IRQ);
            GBA_eval_irqs(this);
        }
    }
    else {

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
        opx->color = this->ppu.palette_RAM[color];
        opx->priority = bg->priority;
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
    if ((px >= hpixels) || (py >= vpixels)) return;

    u32 block_x = px >> 3;
    u32 block_y = py >> 3;
    u32 tile_x = px & 7;
    u32 tile_y = py & 7;
    if (obj_mapping_1d) {
        //1D!
        tile_num += (((htiles >> dsize) << bpp8) * block_y);
    }
    else {
        tile_num += block_y << 5;
    }
    tile_num += block_x << bpp8;
    tile_num &= 0x3FF;
    if (bpp8) tile_num &= 0x3FE;

    u32 tile_base_addr = 0x10000 + (32 * tile_num);
    u32 tile_line_addr = tile_base_addr + (tile_y << (2 + bpp8));
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
                    opx->color = this->ppu.palette_RAM[c + palette];
                    opx->translucent_sprite = blended;
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
    if (this->ppu.io.obj_mapping_1d) {
        tile_num += ((htiles << bpp8) * block_y);
    }
    else {
        tile_num += block_y << 5;
    }

    // Advance by block x. NOT!
    //tile_num += block_x << bpp8;

    if (bpp8) tile_num &= 0x3FE;
    tile_num &= 0x3FF;

    // Now get pointer to that tile
    u32 tile_base_addr = 0x10000 + (32 * tile_num);
    u32 tile_line_addr = tile_base_addr + (line_in_tile << (2 + bpp8));
    return tile_line_addr;
}

static void draw_sprite_affine(struct GBA *this, u16 *ptr, struct GBA_PPU_window *w, u32 num)
{
    this->ppu.obj.drawing_cycles -= 1;
    if (this->ppu.obj.drawing_cycles < 1) return;

    u32 mosaic = (ptr[0] >> 12) & 1;
    i32 my_y = mosaic ? this->ppu.mosaic.obj.y_current : this->clock.ppu.y;

    u32 dsize = (ptr[0] >> 9) & 1;
    u32 htiles, vtiles;
    u32 shape = (ptr[0] >> 14) & 3; // 0 = square, 1 = horiozontal, 2 = vertical
    u32 sz = (ptr[1] >> 14) & 3;
    get_obj_tile_size(sz, shape, &htiles, &vtiles);
    if (dsize) {
        htiles *= 2;
        vtiles *= 2;
    }

    i32 y_min = ptr[0] & 0xFF;
    i32 y_max = (y_min + ((i32)vtiles * 8)) & 255;
    if(y_max < y_min)
        y_min -= 256;
    if(my_y < y_min || my_y >= y_max) {
        return;
    }

    u32 mode = (ptr[0] >> 10) & 3;
    u32 blended = mode == 1;

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

    i32 line_in_sprite = my_y - y_min;
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

static void output_sprite_8bpp(struct GBA *this, u8 *tptr, u32 mode, i32 screen_x, u32 priority, u32 hflip, u32 mosaic, struct GBA_PPU_window *w) {
    for (i32 tile_x = 0; tile_x < 8; tile_x++) {
        i32 sx;
        if (hflip) sx = (7 - tile_x) + screen_x;
        else sx = tile_x + screen_x;
        if ((sx >= 0) && (sx < 240)) {
            this->ppu.obj.drawing_cycles -= 1;
            struct GBA_PX *opx = &this->ppu.obj.line[sx];
            if ((mode > 1) || (!opx->has) || (priority < opx->priority)) {
                u8 c = tptr[tile_x];
                switch (mode) {
                    case 1:
                    case 0:
                        if (c != 0) {
                            opx->has = 1;
                            opx->color = this->ppu.palette_RAM[0x100 + c];
                            opx->translucent_sprite = mode;
                        }
                        opx->priority = priority;
                        opx->mosaic_sprite = mosaic;
                        break;
                    case 2: { // OBJ window
                        if (c != 0) w->inside[sx] = 1;
                        break;
                    }
                }
            }
        }
        if (this->ppu.obj.drawing_cycles < 1) return;
    }
}


static void output_sprite_4bpp(struct GBA *this, u8 *tptr, u32 mode, i32 screen_x, u32 priority, u32 hflip, u32 palette, u32 mosaic, struct GBA_PPU_window *w) {
    for (i32 tile_x = 0; tile_x < 4; tile_x++) {
        i32 sx;
        if (hflip) sx = (7 - (tile_x * 2)) + screen_x;
        else sx = (tile_x * 2) + screen_x;
        u8 data = tptr[tile_x];
        for (u32 i = 0; i < 2; i++) {
            this->ppu.obj.drawing_cycles -= 1;
            if ((sx >= 0) && (sx < 240)) {
                struct GBA_PX *opx = &this->ppu.obj.line[sx];
                u32 c = data & 15;
                data >>= 4;
                if ((mode > 1) || (!opx->has) || (priority < opx->priority)) {
                    switch (mode) {
                        case 1:
                        case 0: {
                            if (c != 0) {
                                opx->has = 1;
                                opx->color = this->ppu.palette_RAM[c + palette];
                                opx->translucent_sprite = mode;
                            }
                            opx->priority = priority;
                            opx->mosaic_sprite = mosaic;
                            break;
                        }
                        case 2: {
                            if (c != 0) w->inside[sx] = 1;
                            break;
                        }
                    }
                }
            }
            if (this->ppu.obj.drawing_cycles < 1) return;
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

    u32 mosaic = (ptr[0] >> 12) & 1;
    i32 my_y = mosaic ? this->ppu.mosaic.obj.y_current : this->clock.ppu.y;

    if(my_y < y_min || my_y >= y_max) {
        return;
    }

    // Clip sprite

    u32 tile_num = ptr[2] & 0x3FF;

    u32 mode = (ptr[0] >> 10) & 3;
    u32 bpp8 = (ptr[0] >> 13) & 1;
    u32 x = ptr[1] & 0x1FF;
    x = SIGNe9to32(x);
    u32 hflip = (ptr[1] >> 12) & 1;
    u32 vflip = (ptr[1] >> 13) & 1;

    u32 priority = (ptr[2] >> 10) & 3;
    u32 palette = bpp8 ? 0 : (0x100 + (((ptr[2] >> 12) & 15) << 4));
    if (bpp8) tile_num &= 0x3FE;

    // OK we got all the attributes. Let's draw it!
    i32 line_in_sprite = my_y - y_min;
    //printf("\nPPU LINE %d LINE IN SPR:%d Y:%d", this->clock.ppu.y, line_in_sprite, y);
    if (vflip) line_in_sprite = (((vtiles * 8) - 1) - line_in_sprite);
    u32 tile_y_in_sprite = line_in_sprite >> 3; // /8
    u32 line_in_tile = line_in_sprite & 7;

    // OK so we know which line
    // We have two possibilities; 1d or 2d layout
    u32 tile_addr = get_sprite_tile_addr(this, tile_num, htiles, tile_y_in_sprite, line_in_tile, bpp8, this->ppu.io.obj_mapping_1d);
    if (hflip) tile_addr += (htiles - 1) * (32 << bpp8);

    i32 screen_x = x;
    for (u32 tile_xs = 0; tile_xs < htiles; tile_xs++) {
        u8 *tptr = ((u8 *) this->ppu.VRAM) + tile_addr;
        if (hflip) tile_addr -= (32 << bpp8);
        else tile_addr += (32 << bpp8);
        if (bpp8) output_sprite_8bpp(this, tptr, mode, screen_x, priority, hflip, mosaic, w);
        else output_sprite_4bpp(this, tptr, mode, screen_x, priority, hflip, palette, mosaic, w);
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

static void fetch_bg_slice(struct GBA *this, struct GBA_PPU_bg *bg, u32 bgnum, u32 block_x, u32 vpos, struct GBA_PX px[8], u32 screen_x)
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
            if ((screen_x + mx) < 240) {
                if (data != 0) {
                    p->has = 1;
                    p->color = this->ppu.palette_RAM[data];
                    p->priority = bg->priority;
                } else
                    p->has = 0;
            }
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
                if ((screen_x + mx) < 240) {
                    if (c != 0) {
                        p->has = 1;
                        p->color = this->ppu.palette_RAM[c + palbank];
                        p->priority = bg->priority;
                    } else
                        p->has = 0;
                }
                if (hflip) mx--;
                else mx++;
            }
        }
    }
}

static void apply_mosaic(struct GBA *this)
{
    // This function updates vertical mosaics, and applies horizontal.
    struct GBA_PPU_bg *bg;
    if (this->ppu.mosaic.bg.y_counter == 0) {
        this->ppu.mosaic.bg.y_current = this->clock.ppu.y;
    }
    for (u32 i = 0; i < 4; i++) {
        bg = &this->ppu.bg[i];
        if (!bg->enable) continue;
        if (bg->mosaic_enable) bg->mosaic_y = this->ppu.mosaic.bg.y_current;
        else bg->mosaic_y = this->clock.ppu.y + 1;
    }
    this->ppu.mosaic.bg.y_counter = (this->ppu.mosaic.bg.y_counter + 1) % this->ppu.mosaic.bg.vsize;

    if (this->ppu.mosaic.obj.y_counter == 0) {
        this->ppu.mosaic.obj.y_current = this->clock.ppu.y;
    }
    this->ppu.mosaic.obj.y_counter = (this->ppu.mosaic.obj.y_counter + 1) % this->ppu.mosaic.obj.vsize;


    // Now do horizontal blend
    for (u32 i = 0; i < 4; i++) {
        bg = &this->ppu.bg[i];
        if (!bg->enable || !bg->mosaic_enable) continue;
        u32 mosaic_counter = 0;
        struct GBA_PX *src;
        for (u32 x = 0; x < 240; x++) {
            if (mosaic_counter == 0) {
                src = &bg->line[x];
            }
            else {
                bg->line[x].has = src->has;
                bg->line[x].color = src->color;
                bg->line[x].priority = src->priority;
            }
            mosaic_counter = (mosaic_counter + 1) % this->ppu.mosaic.bg.hsize;
        }
    }

    // Now do sprites, which is a bit different
    struct GBA_PX *src = &this->ppu.obj.line[0];
    u32 mosaic_counter = 0;
    for (u32 x = 0; x < 240; x++) {
        struct GBA_PX *current = &this->ppu.obj.line[x];
        if (!current->mosaic_sprite || !src->mosaic_sprite || (mosaic_counter == 0))  {
            src = current;
        }
        else {
            current->has = src->has;
            current->color = src->color;
            current->priority = src->priority;
            current->translucent_sprite = src->translucent_sprite;
            current->mosaic_sprite = src->mosaic_sprite;
        }
        mosaic_counter = (mosaic_counter + 1) % this->ppu.mosaic.obj.hsize;
    }
}

static void calculate_windows_vflag(struct GBA *this)
{
    for (u32 wn = 0; wn < 2; wn++) {
        struct GBA_PPU_window *w = &this->ppu.window[wn];
        if (this->clock.ppu.y == w->top) w->v_flag = 1;
        if (this->clock.ppu.y == w->bottom) w->v_flag = 0;
    }
}

static void calculate_windows(struct GBA *this, u32 in_vblank)
{
    //if (!force && !this->ppu.window[0].enable && !this->ppu.window[1].enable && !this->ppu.window[GBA_WINOBJ].enable) return;

    // Calculate windows...
    calculate_windows_vflag(this);
    if (this->clock.ppu.y >= 160) return;
    for (u32 wn = 0; wn < 2; wn++) {
        struct GBA_PPU_window *w = &this->ppu.window[wn];
        if (!w->enable) {
            memset(&w->inside, 0, sizeof(w->inside));
            continue;
        }

        for (u32 x = 0; x < 240; x++) {
            if (x == w->left) w->h_flag = 1;
            if (x == w->right) w->h_flag = 0;

            w->inside[x] =  w->h_flag & w->v_flag;
        }
    }

    // Now take care of the outside window
    struct GBA_PPU_window *w0 = &this->ppu.window[GBA_WIN0];
    struct GBA_PPU_window *w1 = &this->ppu.window[GBA_WIN1];
    struct GBA_PPU_window *ws = &this->ppu.window[GBA_WINOBJ];
    u32 w0r = w0->enable;
    u32 w1r = w1->enable;
    u32 wsr = ws->enable;
    struct GBA_PPU_window *w = &this->ppu.window[GBA_WINOUTSIDE];
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

static struct GBA_PPU_window *get_active_window(struct GBA *this, u32 x)
{
    struct GBA_PPU_window *active_window = NULL;
    if (this->ppu.window[GBA_WIN0].enable || this->ppu.window[GBA_WIN1].enable || this->ppu.window[GBA_WINOBJ].enable) {
        active_window = &this->ppu.window[GBA_WINOUTSIDE];
        if (this->ppu.window[GBA_WINOBJ].enable && this->ppu.window[GBA_WINOBJ].inside[x]) active_window = &this->ppu.window[GBA_WINOBJ];
        if (this->ppu.window[GBA_WIN1].enable && this->ppu.window[GBA_WIN1].inside[x]) active_window = &this->ppu.window[GBA_WIN1];
        if (this->ppu.window[GBA_WIN0].enable && this->ppu.window[GBA_WIN0].inside[x]) active_window = &this->ppu.window[GBA_WIN0];
    }
    return active_window;
}

#define GBACTIVE_OBJ 0
#define GBACTIVE_BG0 1
#define GBACTIVE_BG1 2
#define GBACTIVE_BG3 3
#define GBACTIVE_BG4 4
#define GBACTIVE_SFX 5

static void affine_line_start(struct GBA *this, struct GBA_PPU_bg *bg, i32 *fx, i32 *fy)
{
    if (!bg->enable) {
        if (bg->mosaic_y != bg->last_y_rendered) {
            bg->x_lerp += bg->pb * this->ppu.mosaic.bg.vsize;
            bg->y_lerp += bg->pd * this->ppu.mosaic.bg.vsize;
            bg->last_y_rendered = bg->mosaic_y;
        }
        return;
    }
    *fx = bg->x_lerp;
    *fy = bg->y_lerp;
}

static void affine_line_end(struct GBA *this, struct GBA_PPU_bg *bg)
{
    if (bg->mosaic_y != bg->last_y_rendered) {
        bg->x_lerp += bg->pb * this->ppu.mosaic.bg.vsize;
        bg->y_lerp += bg->pd * this->ppu.mosaic.bg.vsize;
        bg->last_y_rendered = bg->mosaic_y;
    }
}

static void draw_bg_line_affine(struct GBA *this, u32 bgnum)
{
    struct GBA_PPU_bg *bg = &this->ppu.bg[bgnum];
    memset(bg->line, 0, sizeof(bg->line));

    i32 fx, fy;
    affine_line_start(this, bg, &fx, &fy);
    if (!bg->enable) return;

    struct GBA_DBG_tilemap_line_bg *dtl = &this->dbg_info.bg_scrolls[bgnum];

    for (i64 screen_x = 0; screen_x < 240; screen_x++) {
        i32 px = fx >> 8;
        i32 py = fy >> 8;
        fx += bg->pa;
        fy += bg->pc;
        if (bg->display_overflow) { // wraparound
            px &= bg->hpixels_mask;
            py &= bg->vpixels_mask;
        }
        else { // clip/transparent
            if (px < 0) continue;
            if (py < 0) continue;
            if (px >= bg->hpixels) continue;
            if (py >= bg->vpixels) continue;
        }
        get_affine_bg_pixel(this, bgnum, bg, px, py, &bg->line[screen_x]);
        dtl->lines[(py << 7) + (px >> 3)] |= 1 << (px & 7);
    }
    affine_line_end(this, bg);
}

static void draw_bg_line_normal(struct GBA *this, u32 bgnum)
{
    struct GBA_PPU_bg *bg = &this->ppu.bg[bgnum];
    memset(bg->line, 0, sizeof(bg->line));
    if (!bg->enable) return;
    // first do a fetch for fine scroll -1
    u32 hpos = bg->hscroll & bg->hpixels_mask;
    u32 vpos = (bg->vscroll + bg->mosaic_y) & bg->vpixels_mask;
    u32 fine_x = hpos & 7;
    u32 screen_x = 0;
    struct GBA_PX bgpx[8];
    //hpos = ((hpos >> 3) - 1) << 3;
    fetch_bg_slice(this, bg, bgnum, hpos >> 3, vpos, bgpx, 0);
    struct GBA_DBG_line *dbgl = &this->dbg_info.line[this->clock.ppu.y];
    // TODO HERE
    u8 *scroll_line = &this->dbg_info.bg_scrolls[bgnum].lines[((bg->vscroll + this->clock.ppu.y) & bg->vpixels_mask) * 128];
    u32 startx = fine_x;
    for (u32 i = startx; i < 8; i++) {
        bg->line[screen_x].color = bgpx[i].color;
        bg->line[screen_x].has = bgpx[i].has;
        bg->line[screen_x].priority = bgpx[i].priority;
        screen_x++;
        hpos = (hpos + 1) & bg->hpixels_mask;
        scroll_line[hpos >> 3] |= 1 << (hpos & 7);
    }

    while (screen_x < 240) {
        fetch_bg_slice(this, bg, bgnum, hpos >> 3, vpos, &bg->line[screen_x], screen_x);
        if (screen_x <= 232) {
            scroll_line[hpos >> 3] = 0xFF;
        }
        else {
            for (u32 rx = screen_x; rx < 240; rx++) {
                scroll_line[hpos >> 3] |= 1 << (rx - screen_x);
            }
        }
        screen_x += 8;
        hpos = (hpos + 8) & bg->hpixels_mask;
    }
}

static void find_targets_and_priorities(struct GBA *this, u32 bg_enables[6], struct GBA_PX *layers[6], u32 *layer_a_out, u32 *layer_b_out, i32 x, u32 *actives)
{
    u32 laout = 5;
    u32 lbout = 5;

    // Get highest priority 1st target
    for (i32 priority = 3; priority >= 0; priority--) {
        for (i32 layer_num = 4; layer_num >= 0; layer_num--) {
            if (actives[layer_num] && bg_enables[layer_num] && layers[layer_num]->has && (layers[layer_num]->priority == priority)) {
                lbout = laout;
                laout = layer_num;
            }
        }
    }
    *layer_a_out = laout;
    *layer_b_out = lbout;
}

static void output_pixel(struct GBA *this, u32 x, u32 obj_enable, u32 bg_enables[4], u16 *line_output) {
    // Find which window applies to us.
    u32 default_active[6] = {1, 1, 1, 1, 1, 1}; // Default to active if no window.

    u32 *actives = default_active;
    struct GBA_PPU_window *active_window = get_active_window(this, x);
    if (active_window) actives = active_window->active;

    struct GBA_PX *sp_px = &this->ppu.obj.line[x];
    struct GBA_PX empty_px = {.color=this->ppu.palette_RAM[0], .priority=4, .translucent_sprite=0, .has=1};
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
    u32 target_a_layer, target_b_layer;
    find_targets_and_priorities(this, obg_enables, layers, &target_a_layer, &target_b_layer, x, actives);

    // Blending ONLY happens if the topmost pixel is a valid A target and the next-topmost is a valid B target
    // Brighten/darken only happen if topmost pixel is a valid A target

    struct GBA_PX *target_a = layers[target_a_layer];
    struct GBA_PX *target_b = layers[target_b_layer];

    u32 blend_b = target_b->color;

    u32 output_color = target_a->color;
    if (actives[GBACTIVE_SFX] || (target_a->has && target_a->translucent_sprite &&
                                  this->ppu.blend.targets_b[target_b_layer])) { // backdrop is contained in active for the highest-priority window OR above is a semi-transparent sprite & below is a valid target
        if (target_a->has && target_a->translucent_sprite &&
            this->ppu.blend.targets_b[target_b_layer]) { // above is semi-transparent sprite and below is a valid target
            output_color = gba_alpha(output_color, blend_b, this->ppu.blend.use_eva_a, this->ppu.blend.use_eva_b);
        } else if (mode == 1 && this->ppu.blend.targets_a[target_a_layer] &&
                   this->ppu.blend.targets_b[target_b_layer]) { // mode == 1, both are valid
            output_color = gba_alpha(output_color, blend_b, this->ppu.blend.use_eva_a, this->ppu.blend.use_eva_b);
        } else if (mode == 2 && this->ppu.blend.targets_a[target_a_layer]) { // mode = 2, A is valid
            output_color = gba_brighten(output_color, (i32) this->ppu.blend.use_bldy);
        } else if (mode == 3 && this->ppu.blend.targets_a[target_a_layer]) { // mode = 3, B is valid
            output_color = gba_darken(output_color, (i32) this->ppu.blend.use_bldy);
        }
    }

    line_output[x] = output_color;
}

static void draw_line0(struct GBA *this, u16 *line_output)
{
    draw_obj_line(this);
    draw_bg_line_normal(this, 0);
    draw_bg_line_normal(this, 1);
    draw_bg_line_normal(this, 2);
    draw_bg_line_normal(this, 3);
    apply_mosaic(this);
    calculate_windows(this, 0);
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
    apply_mosaic(this);
    memset(&this->ppu.bg[3].line, 0, sizeof(this->ppu.bg[3].line));

    calculate_windows(this, 0);
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
    apply_mosaic(this);
    u32 bg_enables[4] = {0, 0, this->ppu.bg[2].enable, this->ppu.bg[3].enable};
    for (u32 x = 0; x < 240; x++) {
        output_pixel(this, x, this->ppu.obj.enable, &bg_enables[0], line_output);
    }
}

static void draw_bg_line3(struct GBA *this)
{
    struct GBA_PPU_bg *bg = &this->ppu.bg[2];
    memset(bg->line, 0, sizeof(bg->line));

    i32 fx, fy;
    affine_line_start(this, bg, &fx, &fy);
    if (!bg->enable) return;
    for (u32 x = 0; x < 240; x++) {
        i32 tx = fx >> 8;
        i32 ty = fy >> 8;
        fx += bg->pa;
        fy += bg->pc;
        if ((tx < 0) || (ty < 0) || (tx >= 240) || (ty >= 160)) continue;

        u16 *line_input = ((u16 *) this->ppu.VRAM) + (ty * 240);
        bg->line[x].color = line_input[tx];
        bg->line[x].has = (*line_input) != 0;
        bg->line[x].priority = bg->priority;
    }

    affine_line_end(this, bg);
}

static void draw_bg_line4(struct GBA *this)
{
    struct GBA_PPU_bg *bg = &this->ppu.bg[2];
    memset(bg->line, 0, sizeof(bg->line));

    i32 fx, fy;
    affine_line_start(this, bg, &fx, &fy);
    if (!bg->enable) return;


    assert(this->ppu.io.frame < 2);
    u32 base_addr = 0xA000 * this->ppu.io.frame;
    //if (this->clock.ppu.y == 50) printf("\nF:%lld L:%d BIT:%d", this->clock.master_frame, this->clock.ppu.y, this->ppu.io.frame);
    struct GBA_PX *px = &bg->line[0];

    for (u32 x = 0; x < 240; x++) {
        i32 tx = fx >> 8;
        i32 ty = fy >> 8;
        fx += bg->pa;
        fy += bg->pc;
        if ((tx < 0) || (ty < 0) || (tx >= 240) || (ty >= 160)) continue;

        u8 *line_input = ((u8 *) this->ppu.VRAM) + base_addr + (ty * 240);
        u32 color = this->ppu.palette_RAM[line_input[tx]];
        px->has = line_input[tx] != 0;
        px->priority = bg->priority;
        px->color = color;

        px++;
    }
    affine_line_end(this, bg);
}

static void draw_bg_line5(struct GBA *this)
{
    struct GBA_PPU_bg *bg = &this->ppu.bg[2];
    memset(bg->line, 0, sizeof(bg->line));

    i32 fx, fy;
    affine_line_start(this, bg, &fx, &fy);
    if (!bg->enable) return;

    u32 base_addr = 0xA000 * this->ppu.io.frame;
    struct GBA_PX *px = &bg->line[0];
    for (u32 x = 0; x < 240; x++) {
        i32 tx = fx >> 8;
        i32 ty = fy >> 8;
        fx += bg->pa;
        fy += bg->pc;
        if ((tx < 0) || (ty < 0) || (tx >= 160) || (ty >= 128)) continue;

        u16 *line_input = (u16 *)(((u8 *)this->ppu.VRAM) + base_addr + (ty * 320));
        px->color = this->ppu.palette_RAM[line_input[x]];
        px->has = px->color != 0;
        px->priority = bg->priority;

        px++;
    }
    affine_line_end(this, bg);
}

static void draw_line3(struct GBA *this, u16 *line_output)
{
    draw_obj_line(this);
    memset(&this->ppu.bg[0].line, 0, sizeof(this->ppu.bg[0].line));
    memset(&this->ppu.bg[1].line, 0, sizeof(this->ppu.bg[1].line));
    draw_bg_line3(this);
    memset(&this->ppu.bg[3].line, 0, sizeof(this->ppu.bg[3].line));
    calculate_windows(this, 0);
    apply_mosaic(this);

    u32 bg_enables[4] = {0, 0, this->ppu.bg[2].enable, 0};
    for (u32 x = 0; x < 240; x++) {
        output_pixel(this, x, this->ppu.obj.enable, &bg_enables[0], line_output);
    }
}

static void draw_line4(struct GBA *this, u16 *line_output)
{
    draw_obj_line(this);
    memset(&this->ppu.bg[0].line, 0, sizeof(this->ppu.bg[0].line));
    memset(&this->ppu.bg[1].line, 0, sizeof(this->ppu.bg[1].line));
    draw_bg_line4(this);
    memset(&this->ppu.bg[3].line, 0, sizeof(this->ppu.bg[3].line));
    calculate_windows(this, 0);
    apply_mosaic(this);

    u32 bg_enables[4] = {0, 0, this->ppu.bg[2].enable, 0};
    for (u32 x = 0; x < 240; x++) {
        output_pixel(this, x, this->ppu.obj.enable, &bg_enables[0], line_output);
    }
}

static void draw_line5(struct GBA *this, u16 *line_output)
{
    draw_obj_line(this);
    memset(&this->ppu.bg[0].line, 0, sizeof(this->ppu.bg[0].line));
    memset(&this->ppu.bg[1].line, 0, sizeof(this->ppu.bg[1].line));
    draw_bg_line5(this);
    memset(&this->ppu.bg[3].line, 0, sizeof(this->ppu.bg[3].line));
    calculate_windows(this, 0);
    apply_mosaic(this);

    u32 bg_enables[4] = {0, 0, this->ppu.bg[2].enable, 0};
    for (u32 x = 0; x < 240; x++) {
        output_pixel(this, x, this->ppu.obj.enable, &bg_enables[0], line_output);
    }
}

static void process_button_IRQ(struct GBA *this)
{
    if (this->io.button_irq.enable) {
        u32 bits = GBA_get_controller_state(this->controller.pio);
        u32 or = 0;
        u32 and = 1;
        for (u32 i = 0; i < 10; i++) {
            u32 bit = 1 << i;
            if ((this->io.button_irq.buttons & bit) && (bits & bit)) {
                or = 1;
            }
            else {
                and = 0;
            }
        }
        u32 care_about = this->io.button_irq.condition ? and : or;
        u32 old_IF = this->io.IF;
        if (care_about) this->io.IF |= (1 << 12);
        else this->io.IF &= ~(1 << 12);
        //printf("\nCARE_ABOUT? %d BITS:%d BUTTONS:%d CONDITION:%d", care_about, bits, this->io.button_irq.buttons, this->io.button_irq.condition);
        if (old_IF != this->io.IF) {
            if (this->io.IF) printf("\nTRIGGERING THE INTERRUPT...");
            GBA_eval_irqs(this);
        }
    }
}

static void hblank(void *ptr, u64 key, u64 clock, u32 jitter)
{
    struct GBA *this = (struct GBA *)ptr;
    this->clock.ppu.hblank_active = key;

    if (key == 1) { // mid-scanline
        if (this->clock.ppu.y < 160) {
            // Copy debug data
            struct GBA_DBG_line *l = &this->dbg_info.line[this->clock.ppu.y];
            l->bg_mode = this->ppu.io.bg_mode;
            for (u32 bgn = 0; bgn < 4; bgn++) {
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
            memset(line_output, 0, 480);
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
        } else {
            calculate_windows_vflag(this);
        }

        // It's cleared at cycle 0 and set at cycle 1007
        u32 old_IF = this->io.IF;
        this->io.IF |= this->ppu.io.hblank_irq_enable << 1;
        if (old_IF != this->io.IF) {
            GBA_eval_irqs(this);
            DBG_EVENT(DBG_GBA_EVENT_SET_LINECOUNT_IRQ);
        }

        // Check if we have any DMA transfers that need to go...
        GBA_check_dma_at_hblank(this);
    }
    else { // end of scanline/new scanline
        // GBA_PPU_finish_scanline()
        u32 old_IF = this->io.IF;
        this->io.IF |= ((this->ppu.io.vcount_at == this->clock.ppu.y) && this->ppu.io.vcount_irq_enable) << 2;
        if (old_IF != this->io.IF) {
            DBG_EVENT(DBG_GBA_EVENT_SET_LINECOUNT_IRQ);
            GBA_eval_irqs(this);
        }


        this->clock.ppu.y++;
        if (this->dbg.events.view.vec) {
            debugger_report_line(this->dbg.interface, this->clock.ppu.y);
        }

        // ==160 check DMA at vblank
        // == 227, vblank_active = 0
        // == 228, new_frame() goes here. whcih would set y==0
        // THEN
        //  hblank(0)

        this->clock.ppu.scanline_start = this->clock.master_cycle_count;
        struct GBA_PPU_bg *bg;
        // IF Y == 0 reset mosaics goes here
        if (this->clock.ppu.y < 160) {
            struct GBA_DBG_line *dbgl = &this->dbg_info.line[this->clock.ppu.y];
            for (u32 i = 2; i < 4; i++) {
                bg = &this->ppu.bg[i];
                struct GBA_DBG_line_bg *dbgbg = &dbgl->bg[i];
                if ((this->clock.ppu.y == 1) || bg->x_written) {
                    bg->x_lerp = bg->x;
                    dbgbg->reset_x = 1;
                }
                dbgbg->x_lerp = bg->x_lerp;

                if ((this->clock.ppu.y == 1) || (bg->y_written)) {
                    bg->y_lerp = bg->y;
                    dbgbg->reset_y = 1;
                }
                dbgbg->y_lerp = bg->y_lerp;
                bg->x_written = bg->y_written = 0;
            }
        }
    }
}


void GBA_PPU_schedule_frame(void *ptr, u64 key, u64 clock, u32 jitter)
{
    struct GBA *this = (struct GBA *)ptr;
    i64 cur_clock = clock - jitter;
    i64 start_clock = cur_clock;
    for (u32 line = 0; line < 228; line++) {
        if (line == 160) {
            scheduler_only_add_abs(&this->scheduler, cur_clock, 1, this, &vblank, NULL);
        }
        if (line == 227) {
            scheduler_only_add_abs(&this->scheduler, cur_clock, 0, this, &vblank, NULL);
        }

        scheduler_only_add_abs(&this->scheduler, cur_clock+MASTER_CYCLES_BEFORE_HBLANK, 1, this, &hblank, NULL);
        scheduler_only_add_abs_w_tag(&this->scheduler, cur_clock+MASTER_CYCLES_PER_SCANLINE, 0, this, &hblank, NULL, 1);
        cur_clock += MASTER_CYCLES_PER_SCANLINE;

    }
    scheduler_only_add_abs_w_tag(&this->scheduler, start_clock+MASTER_CYCLES_PER_FRAME, 0, this, &GBA_PPU_schedule_frame, NULL, 2);

    // Do new-frame stuff
    struct GBA_PPU_bg *bg;

    for (u32 i = 0; i < 4; i++) {
        bg = &this->ppu.bg[i];
        bg->mosaic_y = 0;
        bg->last_y_rendered = 159;
    }
    this->ppu.mosaic.bg.y_counter = 0;
    this->ppu.mosaic.bg.y_current = 0;
    this->ppu.mosaic.obj.y_counter = 0;
    this->ppu.mosaic.obj.y_current = 0;
    memset(this->dbg_info.bg_scrolls, 0, sizeof(this->dbg_info.bg_scrolls));

    debugger_report_frame(this->dbg.interface);
    this->clock.ppu.y = 0;
    this->ppu.cur_output = ((u16 *)this->ppu.display->output[this->ppu.display->last_written ^ 1]);
    this->ppu.display->last_written ^= 1;
    this->clock.master_frame++;
    process_button_IRQ(this);
}


static u32 GBA_PPU_read_invalid(struct GBA *this, u32 addr, u32 sz, u32 access, u32 has_effect)
{
    printf("\nREAD UNKNOWN PPU ADDR:%08x sz:%d", addr, sz);
    return GBA_open_bus_byte(this, addr);
}

static void GBA_PPU_write_invalid(struct GBA *this, u32 addr, u32 sz, u32 access, u32 val)
{
    printf("\nWRITE UNKNOWN PPU ADDR:%08x sz:%d DATA:%08x", addr, sz, val);
    //dbg.var++;
    //if (dbg.var > 5) dbg_break("too many ppu write", this->clock.master_cycle_count);
}

u32 GBA_PPU_mainbus_read_palette(struct GBA *this, u32 addr, u32 sz, u32 access, u32 has_effect)
{
    this->waitstates.current_transaction++;
    if (sz == 4) {
        this->waitstates.current_transaction++;
        addr &= ~3;
    }    if (sz == 2) addr &= ~1;
    //if (addr < 0x05000400)
    return cR[sz](this->ppu.palette_RAM, addr & 0x3FF);

    //return GBA_PPU_read_invalid(this, addr, sz, access, has_effect);
}

u32 GBA_PPU_mainbus_read_VRAM(struct GBA *this, u32 addr, u32 sz, u32 access, u32 has_effect)
{
    this->waitstates.current_transaction++;
    if (sz == 4) {
        this->waitstates.current_transaction++;
        addr &= ~3;
    }    if (sz == 2) addr &= ~1;
    addr &= 0x1FFFF;
    if (addr < 0x18000)
        return cR[sz](this->ppu.VRAM, addr);
    else
        return cR[sz](this->ppu.VRAM, addr - 0x8000);
}

u32 GBA_PPU_mainbus_read_OAM(struct GBA *this, u32 addr, u32 sz, u32 access, u32 has_effect) {
    this->waitstates.current_transaction++;
    if (sz == 4) addr &= ~3;
    if (sz == 2) addr &= ~1;
    //if (addr < 0x07000400)
        return cR[sz](this->ppu.OAM, addr & 0x3FF);

    //return GBA_PPU_read_invalid(this, addr, sz, access, has_effect);
}

void GBA_PPU_mainbus_write_palette(struct GBA *this, u32 addr, u32 sz, u32 access, u32 val)
{
    this->waitstates.current_transaction++;
    if (sz == 4) {
        this->waitstates.current_transaction++;
        addr &= ~3;
    }
    if (sz == 2) addr &= ~1;
    if (sz == 1) { sz = 2; val = (val & 0xFF) | ((val << 8) & 0xFF00); }

    return cW[sz](this->ppu.palette_RAM, addr & 0x3FF, val);
}

void GBA_PPU_mainbus_write_VRAM(struct GBA *this, u32 addr, u32 sz, u32 access, u32 val)
{
    DBG_EVENT(DBG_GBA_EVENT_WRITE_VRAM);
    this->waitstates.current_transaction++;
    if (sz == 4) {
        this->waitstates.current_transaction++;
        addr &= ~3;
    }
    if (sz == 2) addr &= ~1;

    u32 vram_boundary = this->ppu.io.bg_mode >= 3 ? 0x14000 : 0x10000;
    u32 mask = sz == 4 ? 0xFFFFFFFF : (sz == 2 ? 0xFFFF : 0xFF);
    val &= mask;
    addr &= 0x1FFFF;
    if (addr >= vram_boundary) {
        if (sz != 1) {
            if (addr >= 0x18000) {
                addr &= ~0x8000;

                if (addr < vram_boundary) {
                    return;
                }
            }
            return cW[sz](this->ppu.VRAM, addr, val);
        }
        return;
    }
    else {
        if (sz == 1) {
            addr &= 0x1FFFE;
            sz = 2;
            val = (val << 8) | val;
        }
        return cW[sz](this->ppu.VRAM, addr, val);
    }

    GBA_PPU_write_invalid(this, addr, sz, access, val);
}

void GBA_PPU_mainbus_write_OAM(struct GBA *this, u32 addr, u32 sz, u32 access, u32 val)
{
    this->waitstates.current_transaction++;
    if (sz == 4) addr &= ~3;
    if (sz == 2) addr &= ~1;
    if (sz == 1) return;
    //if (addr < 0x07000400)
        return cW[sz](this->ppu.OAM, addr & 0x3FF, val);

    //GBA_PPU_write_invalid(this, addr, sz, access, val);
}

#define BG3PA 0x04000030
#define BG3PB 0x04000032
#define BG3PC 0x04000034
#define BG3PD 0x04000036

#define WIN0H 0x04000040
#define WIN1H 0x04000042
#define WIN0V 0x04000044
#define WIN1V 0x04000046

#define WININ 0x04000048
#define WINOUT 0x0400004A

#define MOSAIC 0x0400004C
#define BLDCNT 0x04000050
#define BLDALPHA 0x04000052
#define BLDY 0x04000054

u32 GBA_PPU_mainbus_read_IO(struct GBA *this, u32 addr, u32 sz, u32 access, u32 has_effect)
{
    u32 v = 0;
    switch(addr) {
        case 0x04000000: // DISPCTRL lo DISPCNT
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
            v |= (this->clock.ppu.y == this->ppu.io.vcount_at) << 2;
            v |= this->ppu.io.vblank_irq_enable << 3;
            v |= this->ppu.io.hblank_irq_enable << 4;
            v |= this->ppu.io.vcount_irq_enable << 5;
            return v;
        case 0x04000005: // DISPSTAT hi
            v = this->ppu.io.vcount_at;
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
            v |= bg->extrabits << 4;
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

        case WININ:
            v = this->ppu.window[0].active[1] << 0;
            v |= this->ppu.window[0].active[2] << 1;
            v |= this->ppu.window[0].active[3] << 2;
            v |= this->ppu.window[0].active[4] << 3;
            v |= this->ppu.window[0].active[0] << 4;
            v |= this->ppu.window[0].active[5] << 5;
            return v;
        case WININ+1:
            v = this->ppu.window[1].active[1] << 0;
            v |= this->ppu.window[1].active[2] << 1;
            v |= this->ppu.window[1].active[3] << 2;
            v |= this->ppu.window[1].active[4] << 3;
            v |= this->ppu.window[1].active[0] << 4;
            v |= this->ppu.window[1].active[5] << 5;
            return v;
        case WINOUT:
            v = this->ppu.window[GBA_WINOUTSIDE].active[1] << 0;
            v |= this->ppu.window[GBA_WINOUTSIDE].active[2] << 1;
            v |= this->ppu.window[GBA_WINOUTSIDE].active[3] << 2;
            v |= this->ppu.window[GBA_WINOUTSIDE].active[4] << 3;
            v |= this->ppu.window[GBA_WINOUTSIDE].active[0] << 4;
            v |= this->ppu.window[GBA_WINOUTSIDE].active[5] << 5;
            return v;
        case WINOUT+1:
            v = this->ppu.window[GBA_WINOBJ].active[1] << 0;
            v |= this->ppu.window[GBA_WINOBJ].active[2] << 1;
            v |= this->ppu.window[GBA_WINOBJ].active[3] << 2;
            v |= this->ppu.window[GBA_WINOBJ].active[4] << 3;
            v |= this->ppu.window[GBA_WINOBJ].active[0] << 4;
            v |= this->ppu.window[GBA_WINOBJ].active[5] << 5;
            return v;
        case BLDCNT:
            v = this->ppu.blend.targets_a[0] << 4;
            v |= this->ppu.blend.targets_a[1] << 0;
            v |= this->ppu.blend.targets_a[2] << 1;
            v |= this->ppu.blend.targets_a[3] << 2;
            v |= this->ppu.blend.targets_a[4] << 3;
            v |= this->ppu.blend.targets_a[5] << 5;
            v |= this->ppu.blend.mode << 6;
            return v;
        case BLDCNT+1:
            v = this->ppu.blend.targets_b[0] << 4;
            v |= this->ppu.blend.targets_b[1] << 0;
            v |= this->ppu.blend.targets_b[2] << 1;
            v |= this->ppu.blend.targets_b[3] << 2;
            v |= this->ppu.blend.targets_b[4] << 3;
            v |= this->ppu.blend.targets_b[5] << 5;
            return v;
        case BLDALPHA:
            return this->ppu.blend.eva_a;
        case BLDALPHA+1:
            return this->ppu.blend.eva_b;


        case 0x04000002:
        case 0x04000003:
        case 0x04000010:
        case 0x04000011:
        case 0x04000012:
        case 0x04000013:
        case 0x04000014:
        case 0x04000015:
        case 0x04000016:
        case 0x04000017:
        case 0x04000018:
        case 0x04000019:
        case 0x0400001A:
        case 0x0400001B:
        case 0x0400001C:
        case 0x0400001D:
        case 0x0400001E:
        case 0x0400001F:
        case 0x04000020:
        case 0x04000021:
        case 0x04000022:
        case 0x04000023:
        case 0x04000024:
        case 0x04000025:
        case 0x04000026:
        case 0x04000027:
        case 0x04000028:
        case 0x04000029:
        case 0x0400002A:
        case 0x0400002B:
        case 0x0400002C:
        case 0x0400002D:
        case 0x0400002E:
        case 0x0400002F:
        case 0x04000030:
        case 0x04000031:
        case 0x04000032:
        case 0x04000033:
        case 0x04000034:
        case 0x04000035:
        case 0x04000036:
        case 0x04000037:
        case 0x04000038:
        case 0x04000039:
        case 0x0400003A:
        case 0x0400003B:
        case 0x0400003C:
        case 0x0400003D:
        case 0x0400003E:
        case 0x0400003F:
        case 0x0400004E:
        case 0x0400004F:
        case 0x04000054:
        case 0x04000055:
        case 0x04000056:
        case 0x04000057:
        case 0x04000058:
        case 0x04000059:
        case 0x0400005A:
        case 0x0400005B:
        case 0x0400005C:
        case 0x0400005D:
        case 0x0400005E:
        case 0x0400005F:
        case WIN0H:
        case WIN0H+1:
        case WIN1H:
        case WIN1H+1:
        case WIN0V:
        case WIN0V+1:
        case WIN1V:
        case WIN1V+1:
        case MOSAIC:
        case MOSAIC+1:
            return GBA_open_bus_byte(this, addr);

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
    i32 v = this->ppu.bg[bgnum].x & 0x0FFFFFFF;
    val &= 0xFF;
    switch(which) {
        case 0: v = (v & 0x0FFFFF00) | val; break;
        case 1: v = (v & 0x0FFF00FF) | (val << 8); break;
        case 2: v = (v & 0x0F00FFFF) | (val << 16); break;
        case 3: v = (v & 0x00FFFFFF) | (val << 24); break;
    }
    if ((v >> 27) & 1) v |= 0xF0000000;
    this->ppu.bg[bgnum].x = v;
    this->ppu.bg[bgnum].x_written = 1;
    //printf("\nline:%d X%d update:%08x", this->clock.ppu.y, bgnum, v);
}

static void update_bg_y(struct GBA *this, u32 bgnum, u32 which, u32 val)
{
    u32 v = this->ppu.bg[bgnum].y & 0x0FFFFFFF;
    val &= 0xFF;
    switch(which) {
        case 0: v = (v & 0x0FFFFF00) | val; break;
        case 1: v = (v & 0x0FFF00FF) | (val << 8); break;
        case 2: v = (v & 0x0F00FFFF) | (val << 16); break;
        case 3: v = (v & 0x00FFFFFF) | (val << 24); break;
    }
    if ((v >> 27) & 1) v |= 0xF0000000;
    this->ppu.bg[bgnum].y = v;
    this->ppu.bg[bgnum].y_written = 1;
    //printf("\nline:%d Y%d update:%08x", this->clock.ppu.y, bgnum, v);
}

void GBA_PPU_mainbus_write_IO(struct GBA *this, u32 addr, u32 sz, u32 access, u32 val) {
    struct GBA_PPU *ppu = &this->ppu;
    if ((addr >= 0x04000020) && (addr < 0x04000040)) {
        DBG_EVENT(DBG_GBA_EVENT_WRITE_AFFINE_REGS);
    }
    switch(addr) {
        case 0x04000000: {// DISPCNT lo
            //printf("\nDISPCNT WRITE %04x", val);
            u32 new_mode = val & 7;
            if (new_mode >= 6) {
                printf("\nILLEGAL BG MODE:%d", new_mode);
                dbg_break("ILLEGAL BG MODE", this->clock.master_cycle_count);
            }
            else {
                //if (new_mode != ppu->io.bg_mode) printf("\nBG MODE:%d LINE:%d", val & 7, this->clock.ppu.y);
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
        case 0x04000005: this->ppu.io.vcount_at = val; return;
        case 0x04000006: return; // not used
        case 0x04000007: return; // not used

        case 0x04000008: // BG control
        case 0x0400000A:
        case 0x0400000C:
        case 0x0400000E: { // BGCNT lo
            u32 bgnum = (addr & 0b0110) >> 1;
            struct GBA_PPU_bg *bg = &this->ppu.bg[bgnum];
            bg->priority = val & 3;
            bg->extrabits = (val >> 4) & 3;
            bg->character_base_block = ((val >> 2) & 3) << 14;
            bg->mosaic_enable = (val >> 6) & 1;
            bg->bpp8 = (val >> 7) & 1;
            return; }
        case 0x04000009: // BG control // BGCNT hi
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

        case 0x04000020: this->ppu.bg[2].pa = (this->ppu.bg[2].pa & 0xFFFFFF00) | val; return;
        case 0x04000021: this->ppu.bg[2].pa = (this->ppu.bg[2].pa & 0xFF) | (val << 8) | (((val >> 7) & 1) * 0xFFFF0000); return;
        case 0x04000022: this->ppu.bg[2].pb = (this->ppu.bg[2].pb & 0xFFFFFF00) | val; return;
        case 0x04000023: this->ppu.bg[2].pb = (this->ppu.bg[2].pb & 0xFF) | (val << 8) | (((val >> 7) & 1) * 0xFFFF0000); return;
        case 0x04000024: this->ppu.bg[2].pc = (this->ppu.bg[2].pc & 0xFFFFFF00) | val; return;
        case 0x04000025: this->ppu.bg[2].pc = (this->ppu.bg[2].pc & 0xFF) | (val << 8) | (((val >> 7) & 1) * 0xFFFF0000); return;
        case 0x04000026: this->ppu.bg[2].pd = (this->ppu.bg[2].pd & 0xFFFFFF00) | val; return;
        case 0x04000027: this->ppu.bg[2].pd = (this->ppu.bg[2].pd & 0xFF) | (val << 8) | (((val >> 7) & 1) * 0xFFFF0000); return;
        case 0x04000028: update_bg_x(this, BG2, 0, val); return;
        case 0x04000029: update_bg_x(this, BG2, 1, val); return;
        case 0x0400002A: update_bg_x(this, BG2, 2, val); return;
        case 0x0400002B: update_bg_x(this, BG2, 3, val); return;
        case 0x0400002C: update_bg_y(this, BG2, 0, val); return;
        case 0x0400002D: update_bg_y(this, BG2, 1, val); return;
        case 0x0400002E: update_bg_y(this, BG2, 2, val); return;
        case 0x0400002F: update_bg_y(this, BG2, 3, val); return;

        case BG3PA:   this->ppu.bg[3].pa = (this->ppu.bg[3].pa & 0xFFFFFF00) | val; return;
        case BG3PA+1: this->ppu.bg[3].pa = (this->ppu.bg[3].pa & 0xFF) | (val << 8) | (((val >> 7) & 1) * 0xFFFF0000); return;
        case BG3PB:   this->ppu.bg[3].pb = (this->ppu.bg[3].pb & 0xFFFFFF00) | val; return;
        case BG3PB+1: this->ppu.bg[3].pb = (this->ppu.bg[3].pb & 0xFF) | (val << 8) | (((val >> 7) & 1) * 0xFFFF0000); return;
        case BG3PC:   this->ppu.bg[3].pc = (this->ppu.bg[3].pc & 0xFFFFFF00) | val; return;
        case BG3PC+1: this->ppu.bg[3].pc = (this->ppu.bg[3].pc & 0xFF) | (val << 8) | (((val >> 7) & 1) * 0xFFFF0000); return;
        case BG3PD:   this->ppu.bg[3].pd = (this->ppu.bg[3].pd & 0xFFFFFF00) | val; return;
        case BG3PD+1: this->ppu.bg[3].pd = (this->ppu.bg[3].pd & 0xFF) | (val << 8) | (((val >> 7) & 1) * 0xFFFF0000); return;
        case 0x04000038: update_bg_x(this, BG3, 0, val); return;
        case 0x04000039: update_bg_x(this, BG3, 1, val); return;
        case 0x0400003A: update_bg_x(this, BG3, 2, val); return;
        case 0x0400003B: update_bg_x(this, BG3, 3, val); return;
        case 0x0400003C: update_bg_y(this, BG3, 0, val); return;
        case 0x0400003D: update_bg_y(this, BG3, 1, val); return;
        case 0x0400003E: update_bg_y(this, BG3, 2, val); return;
        case 0x0400003F: update_bg_y(this, BG3, 3, val); return;

        case WIN0H:   this->ppu.window[0].right = val; return;
        case WIN0H+1: this->ppu.window[0].left = val; return;
        case WIN1H:   this->ppu.window[1].right = val; return;
        case WIN1H+1: this->ppu.window[1].left = val; return;
        case WIN0V:   this->ppu.window[0].bottom = val; return;
        case WIN0V+1: this->ppu.window[0].top = val; return;
        case WIN1V:   this->ppu.window[1].bottom = val; return;
        case WIN1V+1: this->ppu.window[1].top = val; return;

        case WININ:
            this->ppu.window[0].active[1] = (val >> 0) & 1;
            this->ppu.window[0].active[2] = (val >> 1) & 1;
            this->ppu.window[0].active[3] = (val >> 2) & 1;
            this->ppu.window[0].active[4] = (val >> 3) & 1;
            this->ppu.window[0].active[0] = (val >> 4) & 1;
            this->ppu.window[0].active[5] = (val >> 5) & 1;
            return;
        case WININ+1:
            this->ppu.window[1].active[1] = (val >> 0) & 1;
            this->ppu.window[1].active[2] = (val >> 1) & 1;
            this->ppu.window[1].active[3] = (val >> 2) & 1;
            this->ppu.window[1].active[4] = (val >> 3) & 1;
            this->ppu.window[1].active[0] = (val >> 4) & 1;
            this->ppu.window[1].active[5] = (val >> 5) & 1;
            return;
        case WINOUT:
            this->ppu.window[GBA_WINOUTSIDE].active[1] = (val >> 0) & 1;
            this->ppu.window[GBA_WINOUTSIDE].active[2] = (val >> 1) & 1;
            this->ppu.window[GBA_WINOUTSIDE].active[3] = (val >> 2) & 1;
            this->ppu.window[GBA_WINOUTSIDE].active[4] = (val >> 3) & 1;
            this->ppu.window[GBA_WINOUTSIDE].active[0] = (val >> 4) & 1;
            this->ppu.window[GBA_WINOUTSIDE].active[5] = (val >> 5) & 1;
            return;
        case WINOUT+1:
            this->ppu.window[GBA_WINOBJ].active[1] = (val >> 0) & 1;
            this->ppu.window[GBA_WINOBJ].active[2] = (val >> 1) & 1;
            this->ppu.window[GBA_WINOBJ].active[3] = (val >> 2) & 1;
            this->ppu.window[GBA_WINOBJ].active[4] = (val >> 3) & 1;
            this->ppu.window[GBA_WINOBJ].active[0] = (val >> 4) & 1;
            this->ppu.window[GBA_WINOBJ].active[5] = (val >> 5) & 1;
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

        case BLDCNT:
            this->ppu.blend.mode = (val >> 6) & 3;
            // sp, bg0, bg1, bg2, bg3, bd
            this->ppu.blend.targets_a[0] = (val >> 4) & 1;
            this->ppu.blend.targets_a[1] = (val >> 0) & 1;
            this->ppu.blend.targets_a[2] = (val >> 1) & 1;
            this->ppu.blend.targets_a[3] = (val >> 2) & 1;
            this->ppu.blend.targets_a[4] = (val >> 3) & 1;
            this->ppu.blend.targets_a[5] = (val >> 5) & 1;
            return;
        case BLDCNT+1:
            this->ppu.blend.targets_b[0] = (val >> 4) & 1;
            this->ppu.blend.targets_b[1] = (val >> 0) & 1;
            this->ppu.blend.targets_b[2] = (val >> 1) & 1;
            this->ppu.blend.targets_b[3] = (val >> 2) & 1;
            this->ppu.blend.targets_b[4] = (val >> 3) & 1;
            this->ppu.blend.targets_b[5] = (val >> 5) & 1;
            return;

        case BLDALPHA:
            this->ppu.blend.eva_a = val & 31;
            this->ppu.blend.use_eva_a = this->ppu.blend.eva_a;
            if (this->ppu.blend.use_eva_a > 16) this->ppu.blend.use_eva_a = 16;
            return;
        case BLDALPHA+1:
            this->ppu.blend.eva_b = val & 31;
            this->ppu.blend.use_eva_b = this->ppu.blend.eva_b;
            if (this->ppu.blend.use_eva_b > 16) this->ppu.blend.use_eva_b = 16;
            return;
        case 0x04000054:
            this->ppu.blend.bldy = val & 31;
            this->ppu.blend.use_bldy = this->ppu.blend.bldy;
            if (this->ppu.blend.use_bldy > 16) this->ppu.blend.use_bldy = 16;
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


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
        this->io.IF |= 1;
        GBA_eval_irqs(this);
    }
}

static void hblank(struct GBA *this, u32 val)
{
    this->clock.ppu.hblank_active = 1;
    if (val == 0) {
        if (this->ppu.io.vcount_at == this->clock.ppu.y) {
            this->io.IF |= 4;
            GBA_eval_irqs(this);
        }
    }
    if (val == 1) {
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
}

#define OUT_WIDTH 240


static void draw_sprite_rotated(struct GBA *this, u16 *ptr)
{
    printf("\nROTO SPRITE FAIL");
}

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

static void output_sprite_8bpp(struct GBA *this, u8 *tptr, i32 screen_x, u32 priority, u32 hflip)
{
    for (i32 tile_x = 0; tile_x < 8; tile_x++) {
        i32 sx = tile_x + screen_x;
        if ((sx >= 0) && (sx < 240)) {
            this->ppu.obj.drawing_cycles -= 1;
            struct GBA_PX *opx = &this->ppu.obj.line[sx];
            if (!opx->has) {
                u32 rpx = hflip ? tile_x : (7 - tile_x);
                u16 c = tptr[rpx];
                if (c != 0) {
                    opx->color = c;
                    opx->bpp8 = 1;
                    opx->priority = priority;
                    opx->has = 1;
                    opx->palette = 0;
                }
            }
            if (this->ppu.obj.drawing_cycles < 1) return;
        }
    }
}


static void output_sprite_4bpp(struct GBA *this, u8 *tptr, i32 screen_x, u32 priority, u32 hflip, u32 palette)
{
    for (i32 tile_x = 0; tile_x < 4; tile_x++) {
        i32 sx;
        if (hflip) sx = (7 - (tile_x * 2)) + screen_x;
        else sx = (tile_x * 2) + screen_x;
        u8 data = tptr[tile_x];
        for (u32 i = 0; i < 2; i++) {
            if ((sx >= 0) && (sx < 240)) {
                this->ppu.obj.drawing_cycles -= 1;
                struct GBA_PX *opx = &this->ppu.obj.line[sx];
                if (!opx->has) {
                    u32 c;
                    //if (hflip) {
                        c = data & 15;
                        data >>= 4;
                    //}
                    //else {*/
                    //c = (data >> 4) & 15;
                    //data <<= 4;
                    //}

                    if (c != 0) {

                        opx->color = c;
                        opx->bpp8 = 0;
                        opx->priority = priority;
                        opx->has = 1;
                        opx->palette = palette;
                    }
                }
                if (this->ppu.obj.drawing_cycles < 1) return;
            }
            if (hflip) sx--;
            else sx++;
        }
    }
}

static void draw_sprite_normal(struct GBA *this, u16 *ptr)
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
        if (bpp8) output_sprite_8bpp(this, tptr, screen_x, priority, hflip);
        else output_sprite_4bpp(this, tptr, screen_x, priority, hflip, palette);
        screen_x += 8;
        if (this->ppu.obj.drawing_cycles < 1) return;
    }
}


static void draw_obj_line(struct GBA *this)
{
    this->ppu.obj.drawing_cycles = this->ppu.io.hblank_free ? 954 : 1210;

    memset(this->ppu.obj.line, 0, sizeof(this->ppu.obj.line));
    // Each OBJ takes:
    // n*1 cycles per pixel
    // 10 + n*2 per pixel if rotated/scaled
    for (u32 i = 0; i < 128; i++) {
        u16 *ptr = ((u16 *)this->ppu.OAM) + (i * 4);
        u32 rotation = (ptr[0] >> 8) & 1;
        if (rotation) draw_sprite_rotated(this, ptr);
        else draw_sprite_normal(this, ptr);
    }
}

// Thanks tonc!
u32 se_index_fast(u32 tx, u32 ty, u32 bgcnt) {
    u32 n = tx + ty * 32;
    if (tx >= 32)
        n += 0x03E0;
    if (ty >= 32 && (bgcnt == 3))
        n += 0x0400;
    return n;
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
    assert(line_addr < 0x18000);
    u8 *ptr = ((u8 *)this->ppu.VRAM) + line_addr;

    if (bg->bpp8) {
        printf("\nBPP8!");
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
                    p->priority = 0;}
                else
                    p->has = 0;
                if (hflip) mx--;
                else mx++;
            }
        }
    }
}

static void draw_bg_line_normal(struct GBA *this, u32 bgnum)
{
    struct GBA_PPU_bg *bg = &this->ppu.bg[bgnum];
    memset(bg->line, 0, sizeof(bg->line));
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

static void draw_line0(struct GBA *this, u16 *line_output)
{
    ///printf("\nno line0...")
    if (this->ppu.obj.enable) draw_obj_line(this);
    if (this->ppu.bg[0].enable) draw_bg_line_normal(this, 0);
    if (this->ppu.bg[1].enable) draw_bg_line_normal(this, 1);
    if (this->ppu.bg[2].enable) draw_bg_line_normal(this, 2);
    if (this->ppu.bg[3].enable) draw_bg_line_normal(this, 3);
    for (u32 x = 0; x < 240; x++) {
        struct GBA_PX *sp = &this->ppu.obj.line[x];
        i32 sp_c = -1;
        if (sp->has) {
            sp_c = sp->color;
            if (sp->bpp8) {
                sp_c = this->ppu.palette_RAM[0x100 + sp_c];
            }
            else {
                sp_c = this->ppu.palette_RAM[0x100 + (sp_c + (sp->palette << 4))];
            }
            //c = 0x7FFF;
        }
        //if (this->clock.ppu.y == 32) c = 0x001F;
        struct GBA_PX *p[4] = {
                &this->ppu.bg[0].line[x],
                &this->ppu.bg[1].line[x],
                &this->ppu.bg[2].line[x],
                &this->ppu.bg[3].line[x],
        };
        if ((p[0]->has) && (sp_c == -1)) {
            sp_c = p[0]->color;
            if (p[0]->bpp8) sp_c = this->ppu.palette_RAM[sp_c];
            else sp_c = this->ppu.palette_RAM[(p[0]->palette << 4) | sp_c];
        }
        if (sp_c == -1) sp_c = 0;

        line_output[x] = sp_c;

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
            v |= (this->ppu.obj.window_enable) << 15;
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

void GBA_PPU_mainbus_write_IO(struct GBA *this, u32 addr, u32 sz, u32 access, u32 val) {
    struct GBA_PPU *ppu = &this->ppu;
    //printf("\nWRITE %08x", addr);
    switch(addr) {
        case 0x04000000: {// DISPCNT
            printf("\nDISPCNT WRITE %04x", val);
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
            ppu->bg[0].enable = (val >> 8) & 1;
            ppu->bg[1].enable = (val >> 9) & 1;
            ppu->bg[2].enable = (val >> 10) & 1;
            ppu->bg[3].enable = (val >> 11) & 1;
            ppu->obj.enable = (val >> 12) & 1;
            ppu->window[0].enable = (val >> 13) & 1;
            ppu->window[1].enable = (val >> 14) & 1;
            ppu->obj.window_enable = (val >> 15) & 1;
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
        case 0x04000008:
        case 0x0400000A:
        case 0x0400000C:
        case 0x0400000E: {
            u32 bgnum = (addr & 0b0110) >> 1;
            struct GBA_PPU_bg *bg = &this->ppu.bg[bgnum];
            bg->priority = val & 3;
            bg->character_base_block = ((val >> 2) & 3) << 14;
            bg->mosaic_enable = (val >> 6) & 1;
            bg->bpp8 = (val >> 7) & 1;
            bg->screen_base_block = ((val >> 8) & 31) << 11;
            if (bgnum >= 2) bg->display_overflow = (val >> 13) & 1;
            bg->screen_size = (val >> 14) & 3;
            calc_screen_size(this, bgnum, this->ppu.io.bg_mode);
            return; }
        case 0x04000010: // HOFS
        case 0x04000014:
        case 0x04000018:
        case 0x0400001C: {
            u32 bgnum = (addr >> 2) & 3;
            struct GBA_PPU_bg *bg = &this->ppu.bg[bgnum];
            bg->hscroll = val & 0xFFFF;
            return; }
        case 0x04000012:
        case 0x04000016:
        case 0x0400001A:
        case 0x0400001E: {
            u32 bgnum = (addr >> 2) & 3;
            struct GBA_PPU_bg *bg = &this->ppu.bg[bgnum];
            bg->vscroll = val & 0xFFFF;
            return; }
        }

    GBA_PPU_write_invalid(this, addr, sz, access, val);
}


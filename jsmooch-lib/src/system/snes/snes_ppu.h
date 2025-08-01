//
// Created by . on 4/21/25.
//

#ifndef JSMOOCH_EMUS_SNES_PPU_H
#define JSMOOCH_EMUS_SNES_PPU_H

#include <helpers/int.h>
#include "helpers/cvec.h"

union SNES_PPU_px {
    struct {
        u64 color : 16;
        u64 has : 1;
        u64 palette : 3;
        u64 priority : 4;
        u64 source : 3;
        u64 bpp : 2;
        u64 dbg_priority : 2;
        u64 color_math_flags : 4;
    };
    u64 v;
};

struct SNES_PPU {
    u16 *cur_output;
    u32 cur_pixel;
    struct cvec_ptr display_ptr;
    struct JSM_DISPLAY *display;

    u16 CGRAM[256];
    u16 VRAM[0x8000];

    struct SNES_PPU_WINDOW {
        u32 left, right;
    } window[2];

    struct {
        u32 first;
        struct {
            u32 main_enable, sub_enable, mask;

            u32 enable[2];
            u32 invert[2];
        } window;
        u32 main_enable, sub_enable;
        u32 priority[4];
        u32 range_overflow, time_overflow;
        union SNES_PPU_px line[256];

        struct SNES_PPU_sprite {
            i32 x, y;
            u32 tile_num;
            u32 name_select, name_select_add;
            u32 hflip, vflip;
            u32 priority, palette, pal_offset;
            u32 size;
        } items[128];
    } obj;

    struct {
        struct {
            u32 mask;
            u32 invert[2], enable[2];
            u32 main_mask, sub_mask;
        } window;
        u32 direct_color, blend_mode;

        u32 enable[7];
        u32 halve, math_mode, fixed_color;

    } color_math;

    struct SNES_PPU_BG {
        union SNES_PPU_px line[256];

        u32 enabled;
        u32 bpp;

        u32 screen_x;

        u32 cols, rows, col_mask, row_mask;
        u32 tile_px, tile_px_mask;
        u32 scroll_shift;
        u32 pixels_h, pixels_v;
        u32 pixels_h_mask, pixels_v_mask;
        u32 tiledata_index;

        u32 palette_offset, palette_base, palette_shift, palette_mask;
        u32 num_planes;
        u32 tile_bytes, tile_row_bytes;

        struct {
            u32 enable, counter, size;
        } mosaic;

        struct {
            u32 tile_size;
            u32 screen_size, screen_addr;
            u32 tiledata_addr;
            u32 hoffset, voffset;
        } io;

        struct {
            u32 main_enable, sub_enable;
            u32 enable[2], invert[2];
            u32 mask;
        } window;
        u32 main_enable, sub_enable;

        enum SNES_PPU_TILE_MODE {
            SPTM_BPP2,
            SPTM_BPP4,
            SPTM_BPP8,
            SPTM_inactive,
            SPTM_mode7
        } tile_mode;
        u32 priority[2];
    } bg[4];

    struct {
        u32 vram, hcounter, vcounter, counters;
        struct {
            u32 mdr;
            u32 bgofs;
        } ppu1;
        struct {
            u32 mdr;
            u32 bgofs;
        } ppu2;
        u32 oam;
        u32 mode7;
        u32 cgram_addr, cgram;
    } latch;

    struct {
        i32 hoffset, rhoffset, voffset, rvoffset;
        i32 a, b, c, d;
        i32 x, rx, y, ry;
        u32 hflip, vflip, repeat;
    } mode7;
    struct {
        u32 counter;
    } mosaic;

    struct {
        struct {
            u32 increment, addr, increment_step, increment_mode, mapping;
        } vram;

        u32 wram_addr;

        u32 hcounter, vcounter;
        u32 cgram_addr;

        struct {
            u32 addr, priority;
            //u32 base_addr;
        } oam;

        struct {
            u32 sz, name_select, tile_addr, interlace;
        } obj;
        u32 bg_priority, bg_mode;
        u32 screen_brightness, force_blank;
        struct {
            u32 size;
        } mosaic;
        u32 extbg, interlace, overscan, pseudo_hires;
    } io;

    struct {
        u64 sched_id;
        u32 still_sched;
    } hirq;

};

struct SNES;
struct SNES_memmap_block;
void SNES_PPU_init(struct SNES *);
void SNES_PPU_delete(struct SNES *);
void SNES_PPU_reset(struct SNES *);
void SNES_PPU_write(struct SNES *, u32 addr, u32 val, struct SNES_memmap_block *bl);
u32 SNES_PPU_read(struct SNES *, u32 addr, u32 old, u32 has_effect, struct SNES_memmap_block *bl);
void SNES_PPU_schedule_first(struct SNES *);
struct R5A22_DMA_CHANNEL;
u32 SNES_hdma_reload_ch(struct SNES *snes, struct R5A22_DMA_CHANNEL *ch);

i32 SNES_PPU_hpos(struct SNES *);

#endif //JSMOOCH_EMUS_SNES_PPU_H

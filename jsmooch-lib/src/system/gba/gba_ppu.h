//
// Created by . on 12/4/24.
//

#ifndef JSMOOCH_EMUS_GBA_PPU_H
#define JSMOOCH_EMUS_GBA_PPU_H

#include "helpers/int.h"
#include "helpers/physical_io.h"


struct GBA_PPU_window {
    u32 enable;

    u32 v_flag, h_flag;

    i32 left, right, top, bottom;

    u32 active[6]; // In OBJ, bg0, bg1, bg2, bg3, special FX order
    u8 inside[240];
};

struct GBA_PX {
    u32 color;
    u32 priority;
    u32 has;
    u32 translucent_sprite;
    u32 mosaic_sprite;
};

struct GBA_PPU_OBJ {
    u32 enable;
    struct GBA_PX line[240];
    i32 drawing_cycles;
};

struct GBA_PPU {
    u16 *cur_output;
    struct cvec_ptr display_ptr;
    struct JSM_DISPLAY *display;

    u16 palette_RAM[512];
    u8 VRAM[96 * 1024];
    u8 OAM[1024];

    struct {
        struct {
            u32 hsize, vsize;
            u32 enable;
            u32 y_counter, y_current;
        } bg;
        struct {
            u32 hsize, vsize;
            u32 enable;
            u32 y_counter, y_current;
        } obj;
    } mosaic;

    struct {
        u32 mode;
        u32 targets_a[6];
        u32 targets_b[6];
        u32 eva_a, eva_b;
        u32 use_eva_a, use_eva_b, use_bldy;
        u32 bldy;
    } blend;

    struct {
        u32 bg_mode;
        u32 force_blank;
        u32 frame, hblank_free, obj_mapping_1d;
        u32 vblank_irq_enable, hblank_irq_enable, vcount_irq_enable;
        u32 vcount_at;
    } io;

    struct GBA_PPU_bg {
        u32 enable;

        u32 bpp8;
        u32 htiles, vtiles;
        u32 htiles_mask, vtiles_mask;
        u32 hpixels, vpixels;
        u32 last_y_rendered;
        u32 hpixels_mask, vpixels_mask;

        u32 priority;
        u32 extrabits; // IO bits that are saved but useless
        u32 character_base_block, screen_base_block;
        u32 mosaic_enable, display_overflow;
        u32 screen_size;

        i32 pa, pb, pc, pd;
        i32 x, y;
        u32 x_written, y_written;
        i32 x_lerp, y_lerp;

        struct GBA_PX line[248];

        u32 mosaic_y;

        u32 hscroll, vscroll;
    } bg[4];
    struct GBA_PPU_window window[4]; // win0, win1, win-else and win-obj
    struct GBA_PPU_OBJ obj;
};

#define GBA_WIN0 0
#define GBA_WIN1 1
#define GBA_WINOBJ 2
#define GBA_WINOUTSIDE 3

struct GBA;
void GBA_PPU_init(struct GBA*);
void GBA_PPU_delete(struct GBA*);
void GBA_PPU_reset(struct GBA*);

u32 GBA_PPU_mainbus_read_palette(struct GBA*, u32 addr, u32 sz, u32 access, u32 has_effect);
u32 GBA_PPU_mainbus_read_VRAM(struct GBA*, u32 addr, u32 sz, u32 access, u32 has_effect);
u32 GBA_PPU_mainbus_read_OAM(struct GBA*, u32 addr, u32 sz, u32 access, u32 has_effect);
u32 GBA_PPU_mainbus_read_IO(struct GBA*, u32 addr, u32 sz, u32 access, u32 has_effect);
void GBA_PPU_mainbus_write_palette(struct GBA*, u32 addr, u32 sz, u32 access, u32 val);
void GBA_PPU_mainbus_write_VRAM(struct GBA*, u32 addr, u32 sz, u32 access, u32 val);
void GBA_PPU_mainbus_write_OAM(struct GBA*, u32 addr, u32 sz, u32 access, u32 val);
void GBA_PPU_mainbus_write_IO(struct GBA*, u32 addr, u32 sz, u32 access, u32 val);
void GBA_PPU_new_frame(void *ptr, u64 key, u64 clock, u32 jitter);
void GBA_PPU_new_scanline(struct GBA *);
void GBA_PPU_hblank(void *ptr, u64 key, u64 clock, u32 jitter);
void GBA_PPU_schedule_scanline(void *ptr, u64 key, u64 clock, u32 jitter);
#endif //JSMOOCH_EMUS_GBA_PPU_H

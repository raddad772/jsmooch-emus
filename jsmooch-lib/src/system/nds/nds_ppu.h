//
// Created by . on 1/18/25.
//

#ifndef JSMOOCH_EMUS_NDS_PPU_H
#define JSMOOCH_EMUS_NDS_PPU_H

#include "helpers/int.h"
#include "helpers/physical_io.h"

struct NDS_PPU_window {
    u32 enable;

    u32 v_flag, h_flag;

    i32 left, right, top, bottom;

    u32 active[6]; // In OBJ, bg0, bg1, bg2, bg3, special FX order
    u8 inside[240];
};

struct NDS_PX {
    u32 color;
    u32 priority;
    u32 has;
    u32 translucent_sprite;
    u32 mosaic_sprite;
};

struct NDS_PPU_OBJ {
    u32 enable;
    struct NDS_PX line[240];
    i32 drawing_cycles;
};


struct NDS_PPU {
    // 2 mostly-identical 2d engines (A and B) and a 3d engine
    u16 *cur_output;
    struct cvec_ptr display_ptr;
    struct JSM_DISPLAY *display;

    struct NDSENG2D {
        u32 num;

        struct {
            u8 *bg_vram[32];
            u8 *obj_vram[16];
            u8 *bg_extended_palette[4];
            u8 *obj_extended_palette;
            u8 bg_palette[512];
            u8 obj_palette[512];
            u8 oam[1024];
        } mem;

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

        struct NDS_PPU_bg {
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

            struct NDS_PX line[256+8];

            u32 mosaic_y;

            u32 hscroll, vscroll;
        } bg[4];
        struct NDS_PPU_window window[4]; // win0, win1, win-else and win-obj
        struct NDS_PPU_OBJ obj;
    } eng2d[2];

    struct NDSENG3D {
        struct {
            u8 *texture[4];
            u8 *palette[6];
        } slots;
    } eng3d;
};

struct NDS;
void NDS_PPU_init(struct NDS *);
void NDS_PPU_delete(struct NDS *);
void NDS_PPU_reset(struct NDS *);
void NDS_PPU_start_scanline(struct NDS*); // Called on scanline start
void NDS_PPU_hblank(struct NDS*); // Called at hblank time
void NDS_PPU_finish_scanline(struct NDS*); // Called on scanline end, to render and do housekeeping

u32 NDS_PPU_read_2d_bg_palette(struct NDS*, u32 eng_num, u32 addr, u32 sz);
u32 NDS_PPU_read_2d_obj_palette(struct NDS*, u32 eng_num, u32 addr, u32 sz);

void NDS_PPU_write_2d_bg_palette(struct NDS*, u32 eng_num, u32 addr, u32 sz, u32 val);
void NDS_PPU_write_2d_obj_palette(struct NDS*, u32 eng_num, u32 addr, u32 sz, u32 val);


#endif //JSMOOCH_EMUS_NDS_PPU_H

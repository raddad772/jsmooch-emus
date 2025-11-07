//
// Created by . on 1/18/25.
//

#ifndef JSMOOCH_EMUS_NDS_PPU_H
#define JSMOOCH_EMUS_NDS_PPU_H

#include "helpers_c/int.h"
#include "helpers_c/physical_io.h"
#include "system/nds/3d/nds_ge.h"

enum NDS_SCREEN_KINDS {
    SK_none = 0,
    SK_3donly = 20,
    SK_text = 1,
    SK_affine = 2,
    SK_extended = 3,
    SK_large = 4,
    SK_rotscale_16bit = 5,
    SK_rotscale_8bit = 6,
    SK_rotscale_direct = 7,
    SK_gba_mode_3,
    SK_gba_mode_4,
    SK_gba_mode_5
};

struct NDS_PPU_window {
    u32 enable;

    u32 v_flag, h_flag;

    i32 left, right, top, bottom;

    u32 active[6]; // In OBJ, bg0, bg1, bg2, bg3, special FX order
    u8 inside[256]; // for sprites
};

union NDS_PX {
    struct {
        u32 color : 18;
        u32 priority : 3; // =21
        u32 has : 1; // =22
        u32 sp_translucent : 1; //=23
        u32 sp_mosaic : 1; //=24
        u32 alpha : 4; // =28

        u32 dbg_mode : 2; // = 30
        u32 dbg_bpp : 2; // =32. 0=4bpp, 1=8bpp, 2=16bpp
    };
    u32 u;
};

struct NDS_PPU_OBJ {
    u32 enable;
    union NDS_PX line[256];
    i32 drawing_cycles;
};

#define NDS_WIN0 0
#define NDS_WIN1 1
#define NDS_WINOBJ 2
#define NDS_WINOUTSIDE 3

struct NDS_PPU {
    // 2 mostly-identical 2d engines (A and B) and a 3d engine
    u32 *cur_output;
    struct cvec_ptr display_ptr;
    struct JSM_DISPLAY *display;


    u16 line_a[256];
    u16 line_b[256];
    u32 doing_capture;

    struct {
        u32 vblank_irq_enable7, hblank_irq_enable7, vcount_irq_enable7;
        u32 vcount_at7;
        u32 vblank_irq_enable9, hblank_irq_enable9, vcount_irq_enable9;
        u32 vcount_at9;
        u32 display_block; // A B C or D
        u32 display_swap;

        union NDS_DISPCAPCNT {
            struct {
                u32 eva : 5; // 0-4
                u32 _un1: 3; // 5-7
                u32 evb: 5; // 8-12
                u32 _un2: 3; // 13-15
                u32 vram_write_block : 2;
                u32 vram_write_offset : 2;
                u32 capture_size : 2;
                u32 _un3: 2;
                u32 source_a : 1;
                u32 source_b : 1;
                u32 vram_read_offset : 2;
                u32 _un4: 1;
                u32 capture_source: 2;
                u32 capture_enable : 1;
            };
            u32 u;
        } DISPCAPCNT;
    } io;

    struct NDSENG2D {
        u32 num;
        u32 enable;

        struct {
            u8 *bg_vram[32];
            u8 *obj_vram[16];
            u8 *bg_extended_palette[4];
            u8 *obj_extended_palette;

        } memp;

        struct {
            u8 bg_palette[512];
            u8 obj_palette[512];
            u8 oam[1024];
        } mem;

        struct {
            u32 mode;
            u32 targets_a[6];
            u32 targets_b[6];
            u32 eva_a, eva_b;
            u32 use_eva_a, use_eva_b, use_bldy;
            u32 bldy;
        } blend;

        struct {
            u32 force_blank;
            u32 frame, hblank_free;
            u32 display_mode;
            u32 bg_mode;

            struct {
                u32 do_3d;
                u32 character_base, screen_base;
                u32 extended_palettes;
            } bg;

            struct {
                struct {
                    u32 boundary_1d, stride_1d;
                    u32 map_1d;
                } tile;
                struct {
                    u32 boundary_1d, stride_1d;
                    u32 map_1d, map_2d;
                    u32 dim_2d;
                } bitmap;
                u32 extended_palettes;
            } obj;
        } io;


        u32 line_px[256];

        struct NDS_PPU_bg {
            enum NDS_SCREEN_KINDS kind;
            u32 enable, num;

            u32 bpp8;
            u32 htiles, vtiles;
            u32 htiles_mask, vtiles_mask;
            u32 hpixels, vpixels;
            u32 last_y_rendered;
            u32 hpixels_mask, vpixels_mask;

            u32 priority;
            u32 extrabits; // IO bits that are saved but useless
            u32 character_base_block, screen_base_block;
            u32 mosaic_enable, display_overflow, ext_pal_slot;
            u32 screen_size;

            i32 pa, pb, pc, pd;
            i32 x, y;
            u32 x_written, y_written;
            i32 x_lerp, y_lerp;
            i32 fx, fy;

            union NDS_PX line[256+8];

            u32 mosaic_y;

            u32 hscroll, vscroll;

            u32 do3d; // Just for BG0 on Engine A, lol.
        } bg[4];

        struct {
            struct {
                u32 hsize, vsize;
                u32 enable;
            } bg;
            struct {
                u32 hsize, vsize;
                u32 enable;
            } obj;
        } mosaic;

        struct NDS_PPU_window window[4]; // win0, win1, win-else and win-obj
        struct NDS_PPU_OBJ obj;
    } eng2d[2];

    struct NDSENG3D {
        struct {
            u8 *texture[4];
            u8 *palette[6];
        } slots;
    } eng3d;
    struct {
        struct {
            u32 y_counter, y_current;
        } bg;
        struct {
            u32 y_counter, y_current;
        } obj;
    } mosaic;
};

struct NDS;
void NDS_PPU_init(struct NDS *);
void NDS_PPU_delete(struct NDS *);
void NDS_PPU_reset(struct NDS *);
void NDS_PPU_start_scanline(void *ptr, u64 key, u64 clock, u32 jitter);
void NDS_PPU_hblank(void *ptr, u64 key, u64 clock, u32 jitter);
void NDS_PPU_vblank(void *ptr, u64 key, u64 clock, u32 jitter);
void NDS_PPU_finish_scanline(void *ptr, u64 key, u64 clock, u32 jitter);

u32 NDS_PPU_read_2d_bg_palette(struct NDS*, u32 eng_num, u32 addr, u32 sz);
u32 NDS_PPU_read_2d_obj_palette(struct NDS*, u32 eng_num, u32 addr, u32 sz);

void NDS_PPU_write_2d_bg_palette(struct NDS*, u32 eng_num, u32 addr, u32 sz, u32 val);
void NDS_PPU_write_2d_obj_palette(struct NDS*, u32 eng_num, u32 addr, u32 sz, u32 val);

void NDS_PPU_write9_io(struct NDS *, u32 addr, u32 sz, u32 access, u32 val);
u32 NDS_PPU_read9_io(struct NDS *, u32 addr, u32 sz, u32 access, u32 has_effect);
void NDS_PPU_write7_io(struct NDS *, u32 addr, u32 sz, u32 access, u32 val);
u32 NDS_PPU_read7_io(struct NDS *, u32 addr, u32 sz, u32 access, u32 has_effect);
#endif //JSMOOCH_EMUS_NDS_PPU_H

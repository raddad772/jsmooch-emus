//
// Created by . on 1/18/25.
//

#ifndef JSMOOCH_EMUS_NDS_PPU_H
#define JSMOOCH_EMUS_NDS_PPU_H

#include "helpers/int.h"
#include "helpers/physical_io.h"

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
    u8 inside[256];
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
    struct NDS_PX line[256];
    i32 drawing_cycles;
};

#define NDS_WIN0 0
#define NDS_WIN1 1
#define NDS_WINOBJ 2
#define NDS_WINOUTSIDE 3

struct NDS_PPU {
    // 2 mostly-identical 2d engines (A and B) and a 3d engine
    u16 *cur_output;
    struct cvec_ptr display_ptr;
    struct JSM_DISPLAY *display;

    struct {
        u32 vblank_irq_enable7, hblank_irq_enable7, vcount_irq_enable7;
        u32 vcount_at7;
        u32 vblank_irq_enable9, hblank_irq_enable9, vcount_irq_enable9;
        u32 vcount_at9;
        u32 display_block; // A B C or D
        u32 display_swap;
    } io;

    struct NDSENG2D {
        u32 num;
        u32 enable;

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
            u32 frame, hblank_free;
            u32 tile_obj_map_1d;
            u32 bitmap_obj_map_1d;
            u32 bitmap_obj_2d_dim;
            u32 character_base, screen_base;
            u32 bitmap_obj_1d_boundary, tile_obj_1d_boundary;
            u32 bitmap_obj_1d_stride, tile_obj_1d_stride;
            u32 bg_extended_palettes, obj_extended_palettes;
            u32 display_mode;
        } io;

        u16 line_px[256];

        struct NDS_PPU_bg {
            enum NDS_SCREEN_KINDS kind;
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

        struct {
            u32 texture_mapping;
            u32 polygon_attr_shading;
            u32 alpha_test;
            u32 alpha_blending;
            u32 anti_aliasing;
            u32 edge_marking;
            u32 fog_color_alpha_mode;
            u32 fog_master_enable;
            u32 fog_depth_shift;
            u32 color_buffer_rdlines_underflow;
            u32 polygon_vertex_ram_overflow;
            u32 rear_plane_mode;
        } io;

        struct {
            u32 on_next_vblank;
            u32 translucent_poly_y_sorting;
            u32 depth_buffering;
        } swap_buffers;

        u32 geometry_enable;
        u32 render_enable;
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
void NDS_PPU_start_scanline(struct NDS*); // Called on scanline start
void NDS_PPU_hblank(struct NDS*); // Called at hblank time
void NDS_PPU_finish_scanline(struct NDS*); // Called on scanline end, to render and do housekeeping

u32 NDS_PPU_read_2d_bg_palette(struct NDS*, u32 eng_num, u32 addr, u32 sz);
u32 NDS_PPU_read_2d_obj_palette(struct NDS*, u32 eng_num, u32 addr, u32 sz);

void NDS_PPU_write_2d_bg_palette(struct NDS*, u32 eng_num, u32 addr, u32 sz, u32 val);
void NDS_PPU_write_2d_obj_palette(struct NDS*, u32 eng_num, u32 addr, u32 sz, u32 val);

void NDS_PPU_write9_io(struct NDS *, u32 addr, u32 sz, u32 access, u32 val);
u32 NDS_PPU_read9_io(struct NDS *, u32 addr, u32 sz, u32 access, u32 has_effect);
void NDS_PPU_write7_io(struct NDS *, u32 addr, u32 sz, u32 access, u32 val);
u32 NDS_PPU_read7_io(struct NDS *, u32 addr, u32 sz, u32 access, u32 has_effect);
#endif //JSMOOCH_EMUS_NDS_PPU_H

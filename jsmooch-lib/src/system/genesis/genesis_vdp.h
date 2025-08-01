//
// Created by . on 6/1/24.
//

#ifndef JSMOOCH_EMUS_GENESIS_VDP_H
#define JSMOOCH_EMUS_GENESIS_VDP_H


#include "helpers/int.h"
#include "helpers/physical_io.h"

#include "genesis_bus.h"

enum vertical_scroll_modes {
    VSM_fullscreen = 0,
    VSM_pixel16 = 1
};

enum horizontal_scroll_modes {
    HSM_fullscreen = 0,
    HSM_invalid = 1,
    HSM_pixel8 = 2,
    HSM_perpixel = 3
};

enum interlace_modes{
    IM_none = 0,
    IM_normal_res = 1,
    IM_double_res = 3
};

enum slot_kinds {
    slot_hscroll_data=1,
    slot_layer_a_mapping,
    slot_layer_a_pattern,
    slot_layer_b_mapping,
    slot_layer_b_pattern,
    slot_layer_b_pattern_first_trigger,
    slot_layer_b_pattern_trigger,
    slot_sprite_mapping,
    slot_sprite_pattern,
    slot_external_access,
    slot_refresh_cycle
};

enum FIFO_target {
    ft_VRAM,
    ft_VSRAM,
    ft_CRAM
};

struct genesis_vdp {
    struct genesis_vdp_sprite_pixel {
        u32 has_px, color, priority;
    } sprite_line_buf[320];

    u16 *cur_output;
    u32 cur_pixel;
    struct cvec_ptr display_ptr;
    struct JSM_DISPLAY *display;

    struct {
        u32 bus_locked;
        u32 interlace_field;
        u32 vblank;
        u32 h40;
        u32 fast_h40;

        u32 x_scan_stride;

        i32 z80_irq_clocks;

        u32 plane_a_table_addr, plane_b_table_addr, window_table_addr;
        u32 sprite_table_addr;

        u32 bg_color;

        u32 blank_left_8_pixels;
        u32 counter_latch;
        u16 counter_latch_value;
        u32 enable_overlay, enable_display;
        u32 vram_mode;
        u32 cell30; // 30 cell mode (PAL)
        u32 mode5;
        enum vertical_scroll_modes vscroll_mode; // 1 = 16-pixel columns, 0 = one word for whole screen
        enum horizontal_scroll_modes hscroll_mode;
        enum interlace_modes interlace_mode;
        u32 enable_shadow_highlight;

        u32 hscroll_addr;

        u32 foreground_width, foreground_height;
        u32 foreground_width_mask, foreground_height_mask;
        u32 foreground_width_mask_pixels, foreground_height_mask_pixels;

        u32 enable_th_interrupts;

        u32 window_h_pos;
        u32 window_v_pos;
        u32 window_RIGHT;
        u32 window_DOWN;
    } io;

    struct {
        struct {
            u8 counter;
            u32 enable;
            u32 pending;
            u8 reload;
        } hblank;

        struct {
            u32 enable;
            u32 pending;
        } vblank;
    } irq;

    struct {
        u32 address; // in words
        u32 latch;
        u32 ready;
        u32 pending;
        u32 increment;
        u32 target;
    } command;

    struct {
        u16 len;
        u32 source_address;
        u32 active, enable;
        u32 locked;
        u32 fill_pending, fill_value;
        u32 mode;
    } dma;

    u32 sprite_collision, sprite_overflow;
    u32 cycle;

    // there's...h32, h40, vblank/off
    enum slot_kinds slot_array[4][212];

    u32 sc_slot;
    u32 sc_array, sc_skip;

    u16 CRAM[64]; // 64 colors total on screen + background
    u16 VSRAM[40]; // one entry for each of up to 40 tiles
    u16 VRAM[32768]; // 64K VRAM

    // new_slot = (head + len++) % 5
    // "final" slot is for next queued command after 4
    struct genesis_vdp_fifo {
        struct genesis_vdp_fifo_slot {
            enum FIFO_target target;
            u32 UDS, LDS;
            u32 addr;
            u16 val;
        } slots[5];
        u32 len;
        u32 head;

        struct genesis_vdp_prefetch {
            u32 UDS, LDS;
            u32 addr;
            u16 val;
        } prefetch;
    } fifo;

    struct {
        u32 screen_x;
        u32 screen_y;
        u32 sprite_mappings;
        u32 sprite_patterns;
        u32 dot_overflow;
    } line;

    char term_out[16384]; // YES This is memory unsafe.
    char *term_out_ptr;

    struct {
        u32 hscroll[3]; // HSCROLL for planes A and B, de-negativized. and for window...
        u32 vscroll_latch[2]; // per-line vscroll latch
        u32 fine_x[2];
        int column;
    } fetcher;

    struct {
        struct genesis_vdp_pixel_buf {
            u32 has;
            u32 color;
            u32 priority;
            u32 bg_table_x, bg_table_y; // absolute X & Y output, as seen in the background table
        } buf[32];
        u32 head;
        u32 tail;
        i32 num;
    } ringbuf[2];

    struct {
        struct genesis_vdp_debug_row {
            u32 h40;
            u32 hscroll[2];
            u32 vscroll[2][21];
            u32 hscroll_mode, vscroll_mode;
            u32 video_mode;
            u32 pixel_timing[320];
        } output_rows[240];

        u32 px_displayed[2][3][32768];
    } debug;

    struct {
        u32 h40;
    } latch;

    struct {
        u32 left_col, right_col;
        u32 top_row, bottom_row;
        u32 nt_base;
    } window;
};

struct genesis;
void genesis_VDP_init(struct genesis*);
void genesis_VDP_delete(struct genesis*);
u16 genesis_VDP_mainbus_read(struct genesis*, u32 addr, u16 old, u16 mask, u32 has_effect);
void genesis_VDP_mainbus_write(struct genesis*, u32 addr, u16 val, u16 mask);
void genesis_VDP_cycle(struct genesis*);
u8 genesis_VDP_z80_read(struct genesis*, u32 addr, u8 old, u32 has_effect);
void genesis_VDP_z80_write(struct genesis*, u32 addr, u8 val);
void genesis_VDP_reset(struct genesis*);
void genesis_VDP_vblank(struct genesis*, u32 new_value);
void genesis_VDP_schedule_first(struct genesis *);

#endif //JSMOOCH_EMUS_GENESIS_VDP_H

//
// Created by . on 6/1/24.
//

#ifndef JSMOOCH_EMUS_GENESIS_BUS_H
#define JSMOOCH_EMUS_GENESIS_BUS_H

#include "helpers/int.h"
#include "helpers/debug.h"
#include "helpers/physical_io.h"
#include "helpers/cvec.h"
#include "component/cpu/z80/z80.h"
#include "component/cpu/m68000/m68000.h"
#include "component/audio/sn76489/sn76489.h"
#include "component/audio/ym2612/ym2612.h"

#include "genesis_clock.h"
#include "genesis_cart.h"

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
    slot_hscroll_data,
    slot_layer_a_mapping,
    slot_layer_a_pattern,
    slot_layer_b_mapping,
    slot_layer_b_pattern,
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


struct genesis {
    struct Z80 z80;
    struct M68k m68k;
    struct genesis_clock clock;
    struct genesis_cart cart;

    struct ym2612 ym2612;
    struct SN76489 psg;

    struct genesis_vdp {
        u16 *cur_output;
        struct physical_io_device *display;

        struct {
            u32 interlace_field;
            u32 vblank;
            u32 h32, h40;
            u32 fast_h40;

            i32 z80_irq_clocks;

            i32 irq_vcounter;

            u32 plane_a_table_addr, plane_b_table_addr, window_table_addr;
            u32 sprite_table_addr;

            u32 bg_color, bg_palette_line;

            u32 blank_left_8_pixels;
            u32 enable_virq;
            u32 enable_line_irq;
            u32 enable_dma;
            u32 freeze_hv_on_level2_irq;
            u32 enable_display, enable_display2;
            u32 cell30; // 30 cell mode (PAL)
            u32 mode5;
            enum vertical_scroll_modes vscroll_mode; // 1 = 16-pixel columns, 0 = one word for whole screen
            enum horizontal_scroll_modes hscroll_mode;
            enum interlace_modes interlace_mode;
            u32 line_irq_counter;
            u32 enable_shadow_highlight;

            u32 hscroll_addr;
            u32 auto_increment;

            u32 foreground_width, foreground_height;

            u32 enable_th_interrupts;

            u32 window_h_pos;
            u32 window_v_pos;
            u32 window_draw_L_to_R;
            u32 window_draw_top_to_bottom;

            struct {
                u32 latch;
                u32 val;
            } command;
        } io;

        struct {
            i32 len; // in words
            u32 source_addr; // in words
            u32 dest_addr; // in words
            u32 direction;
            u32 active;
            u32 fill_value;
        } dma;

        u32 cycle;

        struct {
            u32 latch;
            u32 val;
        } command;

        // there's...h32, h40, vblank/off
        enum slot_kinds slot_array[3][428];

        u32 sc_count, sc_slot;
        u32 sc_array;

        u16 CRAM[64];
        u16 VSRAM[20];
        u16 VRAM[32768];

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
            u32 has_extra;
        } fifo;
    } vdp;

    u16 RAM[32768]; // RAM is stored little-endian for some reason
    u8 ARAM[8192]; // Z80 RAM!

    struct {
        struct cvec* IOs;
        u32 described_inputs;
        i64 cycles_left;
    } jsm;

    struct {
        struct {
            u32 reset_line;
            u32 reset_line_count;
            u32 bank_address_register;
            u32 bus_request;
            u32 bus_ack;
        } z80;

        struct {
            u32 open_bus_data;
            u32 VDP_FIFO_stall;
        } m68k;
    } io;
};

void genesis_cycle_m68k(struct genesis* this);
void genesis_cycle_z80(struct genesis* this);
void gen_test_dbg_break(struct genesis* this);
u16 genesis_mainbus_read(struct genesis* this, u32 addr, u32 UDS, u32 LDS, u16 old, u32 has_effect);
void genesis_z80_interrupt(struct genesis* this, u32 level);
void genesis_m68k_vblank_irq(struct genesis* this, u32 level);
void genesis_m68k_line_count_irq(struct genesis* this, u32 level);
u8 genesis_z80_bus_read(struct genesis* this, u16 addr, u8 old, u32 has_effect);
#endif //JSMOOCH_EMUS_GENESIS_BUS_H

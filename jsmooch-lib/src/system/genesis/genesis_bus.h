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

#include "component/controller/genesis3/genesis3.h"

#include "genesis.h"
#include "genesis_clock.h"
#include "genesis_cart.h"
#include "genesis_controllerport.h"

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
    slot_layer_a_mapping=2,
    slot_layer_a_pattern=3,
    slot_layer_b_mapping=4,
    slot_layer_b_pattern=5,
    slot_sprite_mapping=6,
    slot_sprite_pattern=7,
    slot_external_access=8,
    slot_refresh_cycle=9
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
        struct cvec_ptr display_ptr;
        struct JSM_DISPLAY *display;

        struct {
            u32 interlace_field;
            u32 vblank;
            u32 h32, h40;
            u32 fast_h40;

            u32 x_scan_stride;

            i32 z80_irq_clocks;

            i32 irq_vcounter;

            u32 plane_a_table_addr, plane_b_table_addr, window_table_addr;
            u32 sprite_table_addr;
            u32 sprite_generator_addr;

            u32 bg_color;

            u32 blank_left_8_pixels;
            u32 enable_virq;
            u32 enable_line_irq;
            u32 enable_dma;
            u32 counter_latch;
            u16 counter_latch_value;
            u32 enable_overlay, enable_display;
            u32 vram_mode;
            u32 cell30; // 30 cell mode (PAL)
            u32 mode5;
            enum vertical_scroll_modes vscroll_mode; // 1 = 16-pixel columns, 0 = one word for whole screen
            enum horizontal_scroll_modes hscroll_mode;
            enum interlace_modes interlace_mode;
            u32 line_irq_counter;
            u32 enable_shadow_highlight;

            u32 hscroll_addr;

            u32 foreground_width, foreground_height;

            u32 enable_th_interrupts;

            u32 window_h_pos;
            u32 window_v_pos;
            u32 window_draw_L_to_R;
            u32 window_draw_top_to_bottom;

        } io;

        struct {
            u32 address; // in words
            u32 direction;
            u32 active;
            u32 fill_value;
            u32 latch;
            u32 ready;
            u32 pending;
            u32 val;
            u32 target;
            u32 increment;
            u32 delay;
        } command;

        struct {
            i32 len; // in words
            u32 mode;
            u32 wait;
            u32 delay;
            u32 active;
            u32 source;
            u16 data;
            u32 read;
        } dma;

        u32 cycle;

        // there's...h32, h40, vblank/off
        enum slot_kinds slot_array[4][212];

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

            struct genesis_vdp_prefetch {
                u32 UDS, LDS;
                u32 addr;
                u16 val;
            } prefetch;
        } fifo;

        struct {
            u32 hscroll;
            u32 screen_x;
            u32 screen_y;
        } line;

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
            u32 VDP_prefetch_stall;
            u32 stuck;
        } m68k;

        struct genesis_controller_port controller_port1;
        struct genesis_controller_port controller_port2;

    } io;

    struct genesis_controller_3button controller1;
    struct genesis_controller_3button controller2;

    DBG_START
        DBG_CPU_REG_START(m68k)
            *D[8], *A[8],
            *PC, *USP, *SSP, *SR,
            *supervisor, *trace,
            *IMASK, *CSR, *IR, *IRC
        DBG_CPU_REG_END(m68k)

        DBG_CPU_REG_START(z80)
            *A, *B, *C, *D, *E, *HL, *F,
            *AF_, *BC_, *DE_, *HL_,
            *PC, *SP,
            *IX, *IY,
            *EI, *HALT, *CE
        DBG_CPU_REG_END(z80)

        DBG_IMAGE_VIEWS_START
            MDBG_IMAGE_VIEW(nametables)
            MDBG_IMAGE_VIEW(tiles)
        DBG_IMAGE_VIEWS_END

        /*DBG_WAVEFORM_START
            DBG_WAVEFORM_MAIN
            DBG_WAVEFORM_CHANS(4)
        DBG_WAVEFORM_END*/
    DBG_END
};

void genesis_cycle_m68k(struct genesis*);
void genesis_cycle_z80(struct genesis*);
void gen_test_dbg_break(struct genesis*, const char *where);
u16 genesis_mainbus_read(struct genesis*, u32 addr, u32 UDS, u32 LDS, u16 old, u32 has_effect);
void genesis_z80_interrupt(struct genesis*, u32 level);
void genesis_m68k_vblank_irq(struct genesis*, u32 level);
void genesis_m68k_line_count_irq(struct genesis*, u32 level);
u8 genesis_z80_bus_read(struct genesis*, u16 addr, u8 old, u32 has_effect);
#endif //JSMOOCH_EMUS_GENESIS_BUS_H

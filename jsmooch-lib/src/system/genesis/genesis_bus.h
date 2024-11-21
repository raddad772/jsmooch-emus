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
#include "component/controller/genesis6/genesis6.h"

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
        struct genesis_vdp_sprite_pixel {
            u32 has_px, color, priority;
        } sprite_line_buf[320];

        u16 *cur_output;
        u16 *cur_pixel;
        struct cvec_ptr display_ptr;
        struct JSM_DISPLAY *display;

        struct {
            u32 interlace_field;
            u32 vblank;
            u32 h32, h40;
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

        u32 sc_count, sc_slot;
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
            u32 hscroll;
            u32 screen_x;
            u32 screen_y;
        } line;

        char term_out[16384]; // YES This is memory unsafe.
        char *term_out_ptr;

        struct {
            u32 hscroll[3]; // HSCROLL for planes A and B, de-negativized. and for window...
            u32 fine_x[2];
            int column;
        } fetcher;

        struct {
            struct genesis_vdp_pixel_buf {
                u32 has[2];
                u32 color[2];
                u32 priority[2];
            } buf[32];
            u32 head;
            u32 tail[2];
            i32 num[2];
        } ringbuf;


        struct genesis_vdp_debug_row {
            u32 h40;
            u32 hscroll[2];
            u32 vscroll[2][21];
            u32 hscroll_mode, vscroll_mode;
            u32 video_mode;
            u32 pixel_timing[320];
        } debug_info[240];
        i32 last_r;

        u32 hscroll_debug;

        struct {
            u32 left_col, right_col;
            u32 top_row, bottom_row;
            u32 nt_base;
        } window;
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

    struct genesis_controller_6button controller1;
    struct genesis_controller_6button controller2;

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

        DBG_EVENT_VIEW

        DBG_IMAGE_VIEWS_START
            MDBG_IMAGE_VIEW(nametables[3])
            MDBG_IMAGE_VIEW(palette)
            MDBG_IMAGE_VIEW(sprites)
            MDBG_IMAGE_VIEW(tiles)
        DBG_IMAGE_VIEWS_END

        DBG_WAVEFORM_START(psg)
            DBG_WAVEFORM_MAIN
            DBG_WAVEFORM_CHANS(4)
        DBG_WAVEFORM_END(psg)

        DBG_WAVEFORM_START(ym2612)
            DBG_WAVEFORM_MAIN
            DBG_WAVEFORM_CHANS(6)
        DBG_WAVEFORM_END(ym2612)

    DBG_END

    struct {
        double master_cycles_per_audio_sample;
        double next_sample_cycle;
        struct audiobuf *buf;
    } audio;

};

void genesis_cycle_m68k(struct genesis*);
void genesis_cycle_z80(struct genesis*);
void gen_test_dbg_break(struct genesis*, const char *where);
u16 genesis_mainbus_read(struct genesis*, u32 addr, u32 UDS, u32 LDS, u16 old, u32 has_effect);
void genesis_z80_interrupt(struct genesis*, u32 level);
u8 genesis_z80_bus_read(struct genesis*, u16 addr, u8 old, u32 has_effect);
void genesis_bus_update_irqs(struct genesis* this);

#endif //JSMOOCH_EMUS_GENESIS_BUS_H

//
// Created by . on 6/1/24.
//

#ifndef JSMOOCH_EMUS_GENESIS_BUS_H
#define JSMOOCH_EMUS_GENESIS_BUS_H

//#define TRACE_SONIC1

#include "helpers_c/int.h"
#include "helpers_c/debug.h"
#include "helpers_c/physical_io.h"
#include "helpers_c/cvec.h"
#include "component/cpu/z80/z80.h"
#include "component/cpu/m68000/m68000.h"
#include "component/audio/sn76489/sn76489.h"
#include "component/audio/ym2612/ym2612.h"
#include "helpers_c/scheduler.h"

#include "component/controller/genesis3/genesis3.h"
#include "component/controller/genesis6/genesis6.h"

#include "genesis.h"
#include "genesis_clock.h"
#include "genesis_cart.h"
#include "genesis_controllerport.h"
#include "genesis_vdp.h"


//#define GENSCHED_SWITCH
struct genesis;
struct gensched_item;
typedef void (*gensched_callback)(struct genesis *, struct gensched_item *);

struct gensched_item {
    u16 next_index;
    u8 clk_add_z80, clk_add_m68k, clk_add_vdp;
#ifdef GENSCHED_SWITCH
    u8 kind;
#else
    gensched_callback callback;
#endif
};

#define NUM_GENSCHED 300
#define GENSCHED_MUL 20

struct genesis {
    struct Z80 z80;
    struct M68k m68k;
    struct genesis_clock clock;
    struct genesis_cart cart;

    struct {
        u64 z80_cycles, m68k_cycles, vdp_cycles, ym2612_cycles, psg_cycles;
    } timing;

    struct scheduler_t scheduler;
    u32 PAL;
    struct gensched_item scheduler_lookup[NUM_GENSCHED * 2];
    u16 scheduler_index;

    struct ym2612 ym2612;
    struct SN76489 psg;

    struct genesis_vdp vdp;

    u16 RAM[32768]; // RAM is stored little-endian for some reason
    u8 ARAM[8192]; // Z80 RAM!

    struct {
        struct cvec* IOs;
        u32 described_inputs;
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
        u32 SRAM_enabled;
    } io;

    struct genesis_controller_6button controller1;
    struct genesis_controller_6button controller2;

    struct {
        u32 num_symbols;
        struct SYMDO {
            char name[50];
            u32 addr;
        } symbols[20000];
    } debugging;

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
            MDBG_IMAGE_VIEW(ym_info)
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
        double master_cycles_per_audio_sample, master_cycles_per_min_sample, master_cycles_per_max_sample;
        double next_sample_cycle_max, next_sample_cycle_min, next_sample_cycle;
        double next_debug_cycle;
        struct audiobuf *buf;
        u64 cycles;
    } audio;

    struct {
        struct {
            u32 enable_A;
            u32 enable_B;
            u32 enable_sprites;
            u32 ex_trace;

        } vdp;
        u32 JP;
    } opts;
};

void genesis_cycle_m68k(struct genesis*);
void genesis_cycle_z80(struct genesis*);
void gen_test_dbg_break(struct genesis*, const char *where);
u16 genesis_mainbus_read(struct genesis*, u32 addr, u32 UDS, u32 LDS, u16 old, u32 has_effect);
void genesis_z80_interrupt(struct genesis*, u32 level);
u8 genesis_z80_bus_read(struct genesis*, u16 addr, u8 old, u32 has_effect);
void genesis_bus_update_irqs(struct genesis* this);

#endif //JSMOOCH_EMUS_GENESIS_BUS_H

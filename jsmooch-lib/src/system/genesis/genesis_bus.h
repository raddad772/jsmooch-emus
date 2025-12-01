//
// Created by . on 6/1/24.
//

#pragma once

//#define TRACE_SONIC1

#include "helpers/int.h"
#include "helpers/debug.h"
#include "helpers/physical_io.h"
#include "helpers/cvec.h"
#include "component/cpu/z80/z80.h"
#include "component/cpu/m68000/m68000.h"
#include "component/audio/sn76489/sn76489.h"
#include "component/audio/ym2612/ym2612.h"
#include "helpers/scheduler.h"

#include "component/controller/genesis3/genesis3.h"
#include "component/controller/genesis6/genesis6.h"

#include "genesis.h"
#include "genesis_clock.h"
#include "genesis_cart.h"
#include "genesis_controllerport.h"
#include "genesis_vdp.h"

namespace genesis {
//#define GENSCHED_SWITCH
struct core;
struct gensched_item;
typedef void (*gensched_callback)(core*, gensched_item *);

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

struct core : jsm_system {
    explicit core(jsm::systems in_kind);
    Z80::core z80{false};
    M68k::core m68k{true};
    clock clock;
    cart cart{};

    struct {
        u64 z80_cycles{}, m68k_cycles{}, vdp_cycles{}, ym2612_cycles{}, psg_cycles{};
    } timing{};

    scheduler_t scheduler;
    u32 PAL{};
    gensched_item scheduler_lookup[NUM_GENSCHED * 2]{};
    u16 scheduler_index{};

    YM2612::core ym2612;
    SN76489 psg{};

    VDP::core vdp;

    u16 RAM[32768]{}; // RAM is stored little-endian for some reason
    u8 ARAM[8192]{}; // Z80 RAM!

    struct {
        bool described_inputs{false};
    } jsm{};

    struct {
        struct {
            bool reset_line{};
            u32 reset_line_count{};
            u32 bank_address_register{};
            bool bus_request{};
            bool bus_ack{};
        } z80{};

        struct {
            u32 open_bus_data{};
            bool VDP_FIFO_stall{};
            bool VDP_prefetch_stall{};
            bool stuck{};
        } m68k{};

        controller_port controller_port1{};
        controller_port controller_port2{};
        u32 SRAM_enabled{};
    } io{};

    c6button controller1{};
    c6button controller2{};

    struct {
        u32 num_symbols{};
        struct SYMDO {
            char name[50]{};
            u32 addr{};
        } symbols[20000]{};
    } debugging{};

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
        double master_cycles_per_audio_sample{}, master_cycles_per_min_sample{}, master_cycles_per_max_sample{};
        double next_sample_cycle_max{}, next_sample_cycle_min{}, next_sample_cycle{};
        double next_debug_cycle{};
        audiobuf *buf{};
        u64 cycles{};
    } audio{};

    struct {
        struct {
            u32 enable_A{};
            u32 enable_B{};
            u32 enable_sprites{};
            u32 ex_trace{};

        } vdp{};
        u32 JP{};
    } opts{};

    void cycle_m68k();
    void cycle_z80();
    void test_dbg_break(const char *where);
    u16 mainbus_read(u32 addr, u32 UDS, u32 LDS, u16 old, bool has_effect);
    void z80_interrupt(u32 level);
    u8 z80_bus_read(u16 addr, u8 old, bool has_effect);
    void update_irqs();
};

}
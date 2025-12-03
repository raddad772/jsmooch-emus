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

struct SYMDO {
    char name[50]{};
    u32 addr{};
};

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
        bool SRAM_enabled{};
    } io{};

    c6button controller1{};
    c6button controller2{};

    struct {
        u32 num_symbols{};
        SYMDO symbols[20000]{};
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
    } v_opts{};

    void z80_reset_line(bool enabled);
    void cycle_m68k();
    void cycle_z80();
    void test_dbg_break(const char *where);
    u16 mainbus_read(u32 addr, u8 UDS, u8 LDS, u16 old, bool has_effect);
    void mainbus_write(u32 addr, u8 UDS, u8 LDS, u16 val);
    void z80_interrupt(bool level);
    u8 z80_bus_read(u16 addr, u8 old, bool has_effect);
    u16 read_version_register(u32 mask) const;
    void update_irqs();

private:
    void create_scheduling_lookup_table();
    void setup_debug_waveform(debug_waveform *dw);
    u16 mainbus_read_a1k(u32 addr, u16 old, u16 mask, bool has_effect);
    void mainbus_write_a1k(u32 addr, u16 val, u16 mask);
    void z80_mainbus_write(u32 addr, u8 val);
    u8 z80_mainbus_read(u32 addr, u8 old, bool has_effect);
    u8 z80_ym2612_read(u32 addr, u8 old, bool has_effect);
    void write_z80_bank_address_register(u32 val);
    void z80_bus_write(u16 addr, u8 val, bool is_m68k);
    SYMDO *get_at_addr(u32 addr);
    void read_opts();
    void populate_opts();
    static void block_step(void *ptr, u64 key, u64 clock, u32 jitter);
    void load_symbols();
    void schedule_first();
    void setup_crt(JSM_DISPLAY &d);
    void setup_audio();

    void serialize_console(serialized_state &state);
    void serialize_z80(serialized_state &state);
    void serialize_m68k(serialized_state &state);
    void serialize_debug(serialized_state &state);
    void serialize_clock(serialized_state &state);
    void serialize_vdp(serialized_state &state);
    void serialize_cartridge(serialized_state &state);
    void serialize_ym2612(serialized_state &state);
    void serialize_sn76489(serialized_state &state);
    void deserialize_console(serialized_state &state);
    void deserialize_z80(serialized_state &state);
    void deserialize_m68k(serialized_state &state);
    void deserialize_debug(serialized_state &state);
    void deserialize_clock(serialized_state &state);
    void deserialize_vdp(serialized_state &state);
    void deserialize_cartridge(serialized_state &state);
    void deserialize_ym2612(serialized_state &state);
    void deserialize_sn76489(serialized_state &state);

public:
    void play() final;
    void pause() final;
    void stop() final;
    void get_framevars(framevars& out) final;
    void reset() final;
    void killall();
    u32 finish_frame() final;
    u32 finish_scanline() final;
    u32 step_master(u32 howmany) final;
    void load_BIOS(multi_file_set& mfs) final;
    void enable_tracing();
    void disable_tracing();
    void describe_io() final;
    void save_state(serialized_state &state) final;
    void load_state(serialized_state &state, deserialize_ret &ret) final;
    void set_audiobuf(audiobuf *ab) final;
    void setup_debugger_interface(debugger_interface &intf) final;
    //void sideload(multi_file_set& mfs) final;

};

}
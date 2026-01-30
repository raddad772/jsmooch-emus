//
// Created by . on 2/13/25.
//

#pragma once

#include "helpers/sys_interface.h"
#include "component/cpu/z80/z80.h"

/*
 * 56 top overscan
 * 208 visible
 * 56 bottom overscan
 * = 320 lines
 *
 * IRQ fires at beginning of visible scanline 0 in hblank area
 * X timing is 32- 128 - 32 = 192. so 192x320 4:3
 * y=56{}, x=0 is interrupt
 * 2 pixels per 1 CPU clock
 *
 *
 * character ROM addressing works as so,
 * A10-A7 are from IO latch,
 * A6-A0 come from CPU
 *
 * bitmap is 1=off{}, 0=on
 * MSB on the left so shift out hi bits
 * pixel data goes into shift register
 */
namespace galaksija {
struct core : jsm_system {
    core();
    u32 timer_reload_ticks(u32 reload);
    void sample_audio(u32 num_cycles);
    void setup_audio();
    void setup_keyboard();
    u8 read_kb(u32 addr);
    u8 read_IO(u16 addr, u8 open_bus, bool has_effect);
    void write_IO(u16 addr, u8 val);
    u8 mainbus_read(u16 addr, u8 open_bus, bool has_effect);
    void mainbus_write(u16 addr, u8 val);
    void new_frame();
    void new_scanline();
    void reload_shift_register();
    void cpu_cycle();
    void cycle();
    Z80::core z80;

    u8 ROMA[4096]{};
    u8 ROMB[4096]{};
    bool ROMB_present{};
    u8 RAM[6 * 1024]{};

    u8 ROM_char[2048]{};

    struct {
        double master_cycles_per_audio_sample{};
        double next_sample_cycle{};
        audiobuf *buf{};
    } audio{};

    struct {
        u8 latch{};
        u32 open_bus{};
    } io{};

    struct {
        u64 master_cycle_count{};
        u64 master_frame{};
        u32 z80_divider{};
    } clock{};

    struct {
        u8 *cur_output{};
        cvec_ptr<physical_io_device> display_ptr{};
        JSM_DISPLAY *display{};
        u32 x{}, y{};
        u64 scanline_start{};

        u8 shift_register{};
        i32 shift_count{};
    } crt{};

    bool described_inputs;

    DBG_START
        DBG_EVENT_VIEW

        DBG_CPU_REG_START1 *A{}, *B{}, *C{}, *D{}, *E{}, *HL{}, *F{}, *AF_{}, *BC_{}, *DE_{}, *HL_{}, *PC{}, *SP{}, *IX{}, *IY{}, *EI{}, *HALT{}, *CE{} DBG_CPU_REG_END1

        DBG_WAVEFORM_START1
            DBG_WAVEFORM_MAIN
            DBG_WAVEFORM_CHANS(2)
        DBG_WAVEFORM_END1
    DBG_END

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

};

}

//
// Created by Dave on 2/4/2024.
//

#pragma once

#include "helpers/audiobuf.h"
#include "helpers/sys_interface.h"

#include "component/audio/nes_apu/nes_apu.h"

#include "nes_clock.h"
#include "nes_cart.h"
#include "nes_cpu.h"
#include "nes_ppu.h"
#include "nes_bus.h"

jsm_system *NES_new();
void NES_delete(jsm_system* system);

#define NES_INPUTS_CHASSIS 0
#define NES_INPUTS_CARTRIDGE 1
#define NES_INPUTS_PLAYER1 0
#define NES_INPUTS_PLAYER2 1

#define NES_OUTPUTS_DISPLAY 0

enum NES_TIMINGS {
    NES_NTSC,
    NES_PAL,
    NES_DENDY
};

struct NES {
    NES_clock clock{};
    NES_APU apu{};
    r2A03 cpu;
    NES_PPU ppu;

    NES();
    ~NES();

    u32 described_inputs=0;
    i32 cycles_left=0;
    u32 display_enabled=0;
    std::vector<physical_io_device> *IOs{};

    NES_bus bus;
    NES_cart cart;

    struct {
        double master_cycles_per_audio_sample{};
        double next_sample_cycle{};
        audiobuf *buf{};
    } audio{};

    DBG_START
        DBG_CPU_REG_START1 *A, *X, *Y, *P, *S, *PC DBG_CPU_REG_END1
        DBG_EVENT_VIEW

        DBG_IMAGE_VIEWS_START
        MDBG_IMAGE_VIEW(nametables)
        DBG_IMAGE_VIEWS_END

        DBG_WAVEFORM_START1
            DBG_WAVEFORM_MAIN
            DBG_WAVEFORM_CHANS(5)
        DBG_WAVEFORM_END1

    DBG_END

    struct NESDBGDATA {
        struct DBGNESROW {
            struct {
                u32 bg_hide_left_8{}, bg_enable{}, emph_bits{}, bg_pattern_table{};
                u32 x_scroll{}, y_scroll{};
            } io{};
        } rows[240]{};

    } dbg_data{};

    void serialize(serialized_state &state) const;
    void deserialize(serialized_state &state);
};

struct NESJ : jsm_system {
private:
    void setup_audio(std::vector<physical_io_device> &inIOs);
    void sample_audio();

public:
    NESJ() {
        has.save_state = true;
        has.load_BIOS = false;
        has.set_audiobuf = true;
    }

    void play() final;
    void pause() final;
    void stop() final;
    void get_framevars(framevars& out) final;
    void reset() final;
    void killall();
    u32 finish_frame() final;
    u32 finish_scanline() final;
    u32 step_master(u32 howmany) final;
    //void load_BIOS(multi_file_set& mfs) final;
    void enable_tracing();
    void disable_tracing();
    void describe_io() final;
    void save_state(serialized_state &state) final;
    void load_state(serialized_state &state, deserialize_ret &ret) final;
    void set_audiobuf(audiobuf *ab) final;
    void setup_debugger_interface(debugger_interface &intf) final;
    NES nes{};
};


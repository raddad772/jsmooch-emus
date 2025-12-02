//
// Created by . on 11/25/25.
//

#pragma once

#include "helpers/int.h"
#include "helpers/debug.h"
#include "helpers/physical_io.h"
#include "helpers/cvec.h"
#include "helpers/sys_interface.h"

#include "component/cpu/m6502/m6502.h"
#include "component/cpu/m6502/m6502_opcodes.h"
#include "component/cpu/m6502/m6502_disassembler.h"
#include "component/audio/m6581/m6581.h"
#include "component/misc/m6526/m6526.h"

#include "c64_keyboard.h"
#include "c64_clock.h"
#include "c64_mem.h"
#include "vic2.h"


namespace C64 {

struct core : jsm_system {
    explicit core(jsm::regions in_region);
    //clock clock{};
    mem mem;
    M6581::core sid;
    VIC2::core vic2;
    M6502::core cpu{M6502::decoded_opcodes};
    //scheduler_t scheduler;
    i64 cycles_deficit{};
    M6526::core cia1{}, cia2{};
    keyboard keyboard;

    void update_IRQ(u32 device_num, u8 level);
    void update_NMI(u32 device_num, u8 level);

    void run_block();
    void run_cpu();

    u32 IRQ_F{};
    u32 NMI_F{};

    struct {
        buf buf{};
        u16 addr{};
        bool present;
    } sideload_data{};

    u8 open_bus{};
    jsm::systems kind{};
    jsm::regions region{};
    jsm::display_standards display_standard{};
    u64 master_clock{};

    u8 read_io1(u8 addr, u8 old, bool has_effect);
    u8 read_io2(u8 addr, u8 old, bool has_effect);

    void write_io1(u8 addr, u8 val);
    void write_io2(u8 addr, u8 val);
    void schedule_first();
    i32 reset_PC_to{-1};
    void complete_sideload();

private:

    void load_prg(multi_file_set &mfs);
public:

    // Debug/glue code
    DBG_START
        DBG_CPU_REG_START1 *A, *X, *Y, *P, *S, *PC DBG_CPU_REG_END1
        DBG_MEMORY_VIEW

        DBG_EVENT_VIEW
        DBG_LOG_VIEW

        DBG_IMAGE_VIEWS_START
            MDBG_IMAGE_VIEW(sysinfo)
        DBG_IMAGE_VIEWS_END

        DBG_WAVEFORM_START1
            DBG_WAVEFORM_CHANS(3)
            DBG_WAVEFORM_MAIN
        DBG_WAVEFORM_END1
    DBG_END

    struct {
        double master_cycles_per_audio_sample{},  master_cycles_per_max_sample{}, master_cycles_per_min_sample{};
        double next_sample_cycle_max{}, next_sample_cycle_min{}, next_sample_cycle{};
        audiobuf *buf{};
        u64 cycles{};
    } audio{};

    struct {
        bool described_inputs{false};
    } jsm{};

    void setup_crt(JSM_DISPLAY &d);
    void setup_audio(std::vector<physical_io_device> &inIOs);
    void setup_keyboard();
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
    void sideload(multi_file_set& mfs) final;

};

}


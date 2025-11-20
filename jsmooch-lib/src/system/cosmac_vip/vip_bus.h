#pragma once

#include "helpers/int.h"
#include "helpers/cvec.h"
#include "helpers/scheduler.h"
#include "helpers/sys_interface.h"

#include "component/cpu/rca1802/rca1802.h"
#include "component/gpu/cdp1861/cdp1861.h"
#include "vip_clock.h"

namespace VIP {
struct core : jsm_system {
    explicit core(jsm::systems in_kind);
    jsm::systems kind;
    RCA1802::core cpu{};
    CDP1861::core pixie{};
    clock clock{};

    scheduler_t scheduler;

    void do_cycle();

    void write_main_bus(u16 addr, u8 val);
    u8 read_main_bus(u16 addr, u8 old, bool has_effect);

private:
    void schedule_first();
    u16 u6b{};

    u8 RAM[0x1000]{};
    u8 ROM[0x200]{};
    u8 chip8_interpreter[0x200]{};
    u16 RAM_mask{0x7FF};
    void service_N();

public:
    // Everything here down is frontend & debugger stuff
    struct {
        u32 described_inputs{};
    } jsm{};

    DBG_START
        DBG_CPU_REG_START(cpu)
                    *R[16], *N, *I, *T, *IE, *P, *X, *D, *B, *DF, *Q, *EF, *DMA_IN, *DMA_OUT
        DBG_CPU_REG_END(cpu)

        DBG_MEMORY_VIEW

        DBG_EVENT_VIEW

        DBG_IMAGE_VIEWS_START
            MDBG_IMAGE_VIEW(video)
        DBG_IMAGE_VIEWS_END

        DBG_WAVEFORM_START1
            DBG_WAVEFORM_MAIN
        DBG_WAVEFORM_END1
        DBG_LOG_VIEW
    DBG_END

    struct {
        double master_cycles_per_audio_sample{},  master_cycles_per_max_sample{};
        double next_sample_cycle_max{}, next_sample_cycle{};
        audiobuf *buf{};
        u64 cycles{};
    } audio{};

    void play() override;
    void pause() override;
    void stop() override;
    void get_framevars(framevars& out) override;
    void reset() override;
    void killall();
    u32 finish_frame() override;
    u32 finish_scanline() override;
    u32 step_master(u32 howmany) override;
    //void load_BIOS(multi_file_set& mfs) override;
    void enable_tracing();
    void disable_tracing();
    void describe_io() override;
    void save_state(serialized_state &state) override;
    void load_state(serialized_state &state, deserialize_ret &ret) override;
    void set_audiobuf(audiobuf *ab) override;
    void setup_debugger_interface(debugger_interface &intf) override;
    void load_BIOS(multi_file_set& mfs) override;
};
}
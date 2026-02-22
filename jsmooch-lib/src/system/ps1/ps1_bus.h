//
// Created by . on 2/11/25.
//

#pragma once

#include "helpers/physical_io.h"
#include "helpers/better_irq_multiplexer.h"
#include "helpers/debugger/debuggerdefs.h"
#include "helpers/sys_interface.h"

#include "component/cpu/r3000/r3000.h"

#include "helpers/scheduler.h"
#include "helpers/debugger/debugger.h"

#include "ps1_clock.h"
#include "gpu/ps1_gpu.h"
#include "ps1_spu.h"
#include "ps1_dma.h"
#include "peripheral/ps1_sio.h"
#include "peripheral/ps1_digital_pad.h"
#include "ps1_timers.h"
#include "cdrom/ps1_cdrom.h"
#include "ps1_mdec.h"

namespace PS1 {

static constexpr char IRQnames[11][50] = { "VBlank", "GPU", "CDROM", "DMA", "TMR0", "TMR1", "TMR2", "SIO0", "SIO1", "SPU", "LightPen" };
enum IRQ {
    IRQ_VBlank = 0,
    IRQ_GPU = 1,
    IRQ_CDROM = 2,
    IRQ_DMA = 3,
    IRQ_TMR0 = 4,
    IRQ_TMR1 = 5,
    IRQ_TMR2 = 6,
    IRQ_SIO0 = 7,
    IRQ_SIO1 = 8,
    IRQ_SPU = 9,
    IRQ_PIOLightpen = 10
};

enum DMA_ports {
    DP_MDEC_in,
    DP_MDEC_out,
    DP_GPU,
    DP_cdrom,
    DP_SPU,
    DP_PIO,
    DP_OTC
};

struct MEM {
    u8 scratchpad[1024]{};

    //u32 scraptchad{}, MRAM{}, VRAM{}, BIOS{};

    u8 MRAM[2 * 1024 * 1024]{};
    u8 BIOS[512 * 1024]{};
    u8 BIOS_unpatched[512 * 1024]{};
    u32 cache_isolated{};

};


struct core : jsm_system {
    core();
    u32 mainbus_read(u32 addr, u8 sz, bool has_effect);
    void mainbus_write(u32 addr, u8 sz, u32 val);
    void update_timer_irqs();
    u64 calculate_timer1_hblank(u32 diff);
    void timers_write(u32 addr, u8 sz, u32 val);
    void setup_dotclock();
    void dotclock_change();
    void set_irq(IRQ from, u32 level);
    void schedule_frame(u64 start_clock, u32 is_first);
    void amidog_print_console();
    void BIOS_patch_reset();
    void BIOS_patch(u32 addr, u32 val);
    void setup_IRQs();
    void copy_vram();
    void skip_BIOS();
    void sideload_EXE(buf *w);
    void setup_audio();
    void setup_cdrom();
    void sample_audio();
    [[nodiscard]] u64 clock_current() const;

    IRQ_multiplexer_b IRQ_multiplexer;
    CLOCK clock;
    scheduler_t scheduler;
    R3000::core cpu;
    SIO::SIO0 sio0;
    SIO::SIO1 sio1;
    TIMER timers[3]{};
    CDROM::core cdrom;
    MDEC mdec;

    u32 already_scheduled{};
    i64 cycles_left{};

    buf sideloaded{};

    GPU::core gpu;
    SPU::core spu;

    struct {
        bool described_inputs{};
        i64 cycles_left{};
    } jsm{};
    MEM mem{};
    DMA dma;

    u64 dotclock();
    u32 timers_read(u32 addr, u8 sz);
    u64 time_til_next_hblank(u64 clk);

    struct {
        bool cached_isolated{};
        SIO::digital_gamepad controller1;
        u32 spu_delay{};
    } io;

    struct {
        double master_cycles_per_audio_sample{},  master_cycles_per_min_sample{}, master_cycles_per_max_sample{}, master_cycles_per_med_sample{};
        double next_sample_cycle_max{}, next_sample_cycle_min{}, next_sample_cycle{}, next_sample_cycle_med{};
        audiobuf *buf{};
        bool nosolo{};
        u64 cycles{};
        double cycles_per_frame{};
    } audio{};

    void setup_debug_waveform(debug::waveform2::wf *dw);

    DBG_START
        cvec_ptr<debugger_view> console_view{};
        DBG_LOG_VIEW
        DBG_IMAGE_VIEWS_START
            MDBG_IMAGE_VIEW(sysinfo)
            MDBG_IMAGE_VIEW(vram)
        DBG_IMAGE_VIEWS_END

        DBG_WAVEFORM2_START1
            DBG_WAVEFORM2_MAIN
            DBG_WAVEFORM2_BRANCH(channels, 24)
            DBG_WAVEFORM2_BRANCH(cd, 2)
            DBG_WAVEFORM2_BRANCH(reverb, 8)
        DBG_WAVEFORM2_END1
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
    //void save_state(serialized_state &state) final;
    //void load_state(serialized_state &state, deserialize_ret &ret) final;
    void set_audiobuf(audiobuf *ab) final;
    void setup_debugger_interface(debugger_interface &intf) final;
    void sideload(multi_file_set& mfs) final;
};

}

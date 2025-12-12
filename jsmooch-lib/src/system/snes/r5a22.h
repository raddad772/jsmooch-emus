//
// Created by . on 4/21/25.
//

#pragma once
#include "helpers/int.h"
#include "component/cpu/wdc65816/wdc65816.h"
#include "snes_controller_port.h"
#include "snes_bus.h"

namespace SNES {
struct core;

namespace R5A22 {

struct DMA_CHANNEL {
    SNES::core *snes{};
    bool hdma_is_finished() const;
    bool hdma_is_active() const;
    u32 hdma_reload();
    u32 hdma_setup();
    u32 dma_run();
    void hdma_transfer();
    void hdma_advance();
    void dma_transfer(u32 addrA, u32 index, u32 hdma_mode);
    void clear_values();
    u32 dma_enable{}, status{}, hdma_enable{}, direction{}, indirect{}, unused{}, reverse_transfer{}, fixed_transfer{};
    u32 transfer_mode{}, target_address{}, source_address{}, dma_pause{};
    u32 source_bank{}, transfer_size{}, indirect_bank{}, indirect_address{};
    u32 hdma_address{}, line_counter{}, hdma_completed{}, hdma_do_transfer{};
    u32 took_cycles{}, index{}, unknown_byte{};
    DMA_CHANNEL *next{};
    u32 num{};
};

struct core {
    explicit core(SNES::core *parent, u64 *master_clock);
    void reset();
    void setup_tracing(jsm_debug_read_trace *strct);
    void hblank(u32 which);
    u32 reg_read(u32 addr, u32 old, bool has_effect, memmap_block *bl);
    void reg_write(u32 addr, u32 val, memmap_block *bl);
    void cycle_cpu();
    void cycle_alu();
    u32 dma_run();

private:
    void dma_reset();
    u32 dma_reg_read(u32 addr, u32 old, bool has_effect);
    void dma_reg_write(u32 addr, u32 val);

public:
    void schedule_first();
    void dma_start();
    [[nodiscard]] u32 hdma_is_enabled() const;
    void update_irq();
    void update_nmi();

    SNES::core *snes;
    WDC65816::core cpu;
    controller::port controller_port1{}, controller_port2{};
    u32 ROMspeed{};

    struct {
        u32 dma_pending{}, hdma_pending{}, dma_running{}, hdma_running{}, dma_active{};
        u32 irq_line{}, hirq_line{};
        struct {
            u32 counter{};
            u64 sch_id{};
            u32 still_sched{};
        } auto_joypad{};

        u32 nmi_flag{};
        u32 irq_flag{};
        u32 hirq_status{}, virq_status{};
    } status{};

    struct {
        struct {
            u64 sched_id{};
            u32 sched_still{};
        } setup{};
    } hdma{};

    struct {
        u32 wrmpya{}, wrmpyb{}, wrdiva{}, wrdivb{}, rddiv{}, rdmpy{}, htime{}, vtime{};
        u32 auto_joypad_poll{}, hirq_enable{}, virq_enable{}, irq_enable{};
        u32 hcounter{}, vcounter{};
        u32 nmi_enable{};
        u32 pio{};
        u32 joy1{}, joy2{}, joy3{}, joy4{};
    } io{};

    struct {
        u32 counters{};
    } latch{};

    struct {
        i32 mpyctr{}, divctr{}, shift{};
    } alu{};

    struct {
        DMA_CHANNEL channels[8]{};
        u32 hdma_enabled{};

        struct {
            u64 id{};
            u32 still{};
        } sched{};

    } dma{};
};


void cycle(void *ptr, u64 key, u64 clock, u32 jitter);

}
}

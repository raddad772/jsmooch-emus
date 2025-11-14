//
// Created by . on 4/21/25.
//

#pragma once
#include "helpers/int.h"
#include "component/cpu/wdc65816/wdc65816.h"
#include "snes_controller_port.h"

struct SNES;

struct R5A22_DMA_CHANNEL {
    explicit R5A22_DMA_CHANNEL(SNES *in_snes) : snes(in_snes) {}
    SNES *snes;
    u32 hdma_is_finished();
    u32 hdma_is_active();
    u32 hdma_reload_ch();
    u32 hdma_setup_ch();
    u32 dma_run_ch();
    void dma_transfer(u32 addrA, u32 index, u32 hdma_mode);
    u32 dma_enable{}, status{}, hdma_enable{}, direction{}, indirect{}, unused{}, reverse_transfer{}, fixed_transfer{};
    u32 transfer_mode{}, target_address{}, source_address{}, dma_pause{};
    u32 source_bank{}, transfer_size{}, indirect_bank{}, indirect_address{};
    u32 hdma_address{}, line_counter{}, hdma_completed{}, hdma_do_transfer{};
    u32 took_cycles{}, index{}, unknown_byte{};
    R5A22_DMA_CHANNEL *next{};
    u32 num{};
};

struct R5A22 {
    explicit R5A22(SNES *parent) : snes(parent) {}
    SNES *snes;
    WDC65816 cpu;
    SNES_controller_port controller_port[2]{};
    u32 ROMspeed{};
    u32 hdma_is_enabled();
    void update_irq();

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
        R5A22_DMA_CHANNEL channels[8]{};
        u32 hdma_enabled{};

        struct {
            u64 id{};
            u32 still{};
        } sched{};

    } dma{};
};



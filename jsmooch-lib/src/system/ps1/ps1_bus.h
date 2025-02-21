//
// Created by . on 2/11/25.
//

#ifndef JSMOOCH_EMUS_PS1_BUS_H
#define JSMOOCH_EMUS_PS1_BUS_H

#include "component/cpu/r3000/r3000.h"
#include "helpers/physical_io.h"
#include "gpu/ps1_gpu.h"

enum PS1_IRQ {
    PS1IRQ_VBlank,
    PS1IRQ_GPU,
    PS1IRQ_CDROM,
    PS1IRQ_DMA,
    PS1IRQ_TMR0,
    PS1IRQ_TMR1,
    PS1IRQ_TMR2,
    PS1IRQ_PadMemCardByteRecv,
    PS1IRQ_SIO,
    PS1IRQ_SPU,
    PS1IRQ_PIOLightpen
};

enum PS1_DMA_ports {
    PS1DP_MDEC_in,
    PS1DP_MDEC_out,
    PS1DP_GPU,
    PS1DP_cdrom,
    PS1DP_SPU,
    PS1DP_PIO,
    PS1DP_OTC
};

struct PS1_clock {
    u64 master_cycle_count;
    u64 master_frame;
    u32 waitstates;
    i64 cycles_left_this_frame;

    struct {
        u32 x, y;
        u64 scanline_start;
        u32 hblank_active, vblank_active;
    } crt;
};

enum PS1_DMA_e {
    PS1D_to_ram,
    PS1D_from_ram,
    PS1D_increment,
    PS1D_decrement,
    PS1D_manual,
    PS1D_request,
    PS1D_linked_list
};

struct PS1 {
    struct R3000 cpu;
    struct PS1_clock clock;
    i64 cycles_left;

    struct buf sideloaded;

    struct PS1_GPU gpu;

    struct {
        struct cvec* IOs;
        u32 described_inputs;
        i64 cycles_left;
    } jsm;

    struct PS1_MEM {
        u8 scratchpad[1024];

        //u32 scraptchad, MRAM, VRAM, BIOS;

        u8 MRAM[2 * 1024 * 1024];
        u8 BIOS[512 * 1024];
        u8 BIOS_unpatched[512 * 1024];
        u32 cache_isolated;

    } mem;

    struct PS1_DMA {
        u32 control;
        u32 irq_enable, irq_enable_ch;
        u32 irq_flags_ch;
        u32 irq_force;
        u32 unknown1;
        struct PS1_DMA_channel {
            u32 num, enable, trigger, chop;
            u32 chop_dma_size, chop_cpu_size;
            u32 unknown;
            u32 base_addr;
            u32 block_size, block_count;
            enum PS1_DMA_e step, sync, direction;
        } channels[7];
    } dma;

    struct {
        u32 cached_isolated;
    } io;
};


void PS1_bus_init(struct PS1 *);
void PS1_bus_reset(struct PS1 *);
u32 PS1_mainbus_read(void *ptr, u32 addr, u32 sz, u32 has_effect);
void PS1_mainbus_write(void *ptr, u32 addr, u32 sz, u32 val);
u32 PS1_mainbus_fetchins(void *ptr, u32 addr, u32 sz);
#endif //JSMOOCH_EMUS_PS1_BUS_H

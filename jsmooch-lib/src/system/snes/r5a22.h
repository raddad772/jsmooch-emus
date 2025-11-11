//
// Created by . on 4/21/25.
//

#ifndef JSMOOCH_EMUS_R5A22_H
#define JSMOOCH_EMUS_R5A22_H
#include "helpers/int.h"
#include "component/cpu/wdc65816/wdc65816.h"
#include "snes_controller_port.h"

struct R5A22 {
    struct WDC65816 cpu;
    struct SNES_controller_port controller_port[2];
    u32 ROMspeed;

    struct {
        u32 dma_pending, hdma_pending, dma_running, hdma_running, dma_active;
        u32 irq_line, hirq_line;
        struct {
            u32 counter;
            u64 sch_id;
            u32 still_sched;
        } auto_joypad;

        u32 nmi_flag;
        u32 irq_flag;
        u32 hirq_status, virq_status;
    } status;

    struct {
        struct {
            u64 sched_id;
            u32 sched_still;
        } setup;
    } hdma;

    struct {
        u32 wrmpya, wrmpyb, wrdiva, wrdivb, rddiv, rdmpy, htime, vtime;
        u32 auto_joypad_poll, hirq_enable, virq_enable, irq_enable;
        u32 hcounter, vcounter;
        u32 nmi_enable;
        u32 pio;
        u32 joy1, joy2, joy3, joy4;
    } io;

    struct {
        u32 counters;
    } latch;

    struct {
        i32 mpyctr, divctr, shift;
    } alu;

    struct {
        struct R5A22_DMA_CHANNEL {
            u32 dma_enable, status, hdma_enable, direction, indirect, unused, reverse_transfer, fixed_transfer;
            u32 transfer_mode, target_address, source_address, dma_pause;
            u32 source_bank, transfer_size, indirect_bank, indirect_address;
            u32 hdma_address, line_counter, hdma_completed, hdma_do_transfer;
            u32 took_cycles, index, unknown_byte;
            struct R5A22_DMA_CHANNEL *next;
            u32 num;
        } channels[8];
        u32 hdma_enabled;

        struct {
            u64 id;
            u32 still;
        } sched;

    } dma;
};

struct SNES;
struct SNES_memmap_block;
void R5A22_init(struct R5A22 *, u64 *master_cycle_count);
void R5A22_delete(struct R5A22 *);
void R5A22_reset(struct R5A22 *);
//void R5A22_cycle(struct SNES *);
void R5A22_reg_write(struct SNES *, u32 addr, u32 val, SNES_memmap_block *bl);
u32 R5A22_reg_read(struct SNES *, u32 addr, u32 old, u32 has_effect, SNES_memmap_block *bl);
void R5A22_setup_tracing(struct R5A22 *, jsm_debug_read_trace *strct);
void R5A22_set_IRQ_level(struct R5A22 *, u32 level);
void R5A22_set_NMI_level(struct R5A22 *, u32 level);
void R5A22_schedule_first(struct SNES *);
void R5A22_update_irq(struct SNES *);
void R5A22_update_nmi(struct SNES *);
void R5A22_hblank(struct SNES *, u32 which);
void SNES_latch_ppu_counters(struct SNES *snes);
void R5A22_cycle(void *ptr, u64 key, u64 clock, u32 jitter);

#endif //JSMOOCH_EMUS_R5A22_H

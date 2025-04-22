//
// Created by . on 4/21/25.
//

#ifndef JSMOOCH_EMUS_R5A22_H
#define JSMOOCH_EMUS_R5A22_H
#include "helpers/int.h"
#include "component/cpu/wdc65816/wdc65816.h"

struct R5A22 {
    struct WDC65816 cpu;
    u32 ROMspeed;

    struct {
        u32 nmi_hold, nmi_line, nmi_transition;

        u32 irq_lock, irq_hold, irq_line, irq_transition;

        u32 dma_pending, hdma_pending, dma_running, hdma_running, dma_active;
        u32 auto_joypad_counter;
    } status;
};

struct SNES;
struct SNES_memmap_block;
void R5A22_init(struct R5A22 *, u64 *master_cycle_count);
void R5A22_delete(struct R5A22 *);
void R5A22_reset(struct R5A22 *);
void R5A22_reg_write(struct SNES *, u32 addr, u32 val, struct SNES_memmap_block *bl);
u32 R5A22_reg_read(struct SNES *, u32 addr, u32 old, u32 has_effect, struct SNES_memmap_block *bl);
void R5A22_setup_tracing(struct R5A22 *, struct jsm_debug_read_trace *strct);
void R5A22_set_IRQ_level(struct R5A22 *, u32 level);
void R5A22_set_NMI_level(struct R5A22 *, u32 level);

#endif //JSMOOCH_EMUS_R5A22_H

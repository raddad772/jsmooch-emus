//
// Created by . on 4/21/25.
//

#include <string.h>
#include "snes_bus.h"
#include "r5a22.h"

void R5A22_init(struct R5A22 *this, u64 *master_cycle_count)
{
    memset(this, 0, sizeof(*this));
    WDC65816_init(&this->cpu, master_cycle_count);
    for (u32 i = 0; i < 7; i++) {
        this->dma.channels[i].next = &this->dma.channels[i+1];
    }
}

static void dma_reset(struct R5A22 *this)
{
    for (u32 i = 0; i < 8; i++) {
        struct R5A22_DMA_CHANNEL *ch = &this->dma.channels[i];
        memset(ch, 0, sizeof(*ch));
        if (i < 7) ch->next = &this->dma.channels[i+1];
    }
}

void R5A22_reset(struct R5A22 *this)
{
    WDC65816_reset(&this->cpu);
    this->cpu.regs.TCU = 0;
    this->cpu.pins.D = WDC65816_OP_RESET;
    dma_reset(this);
    this->ROMspeed = 8;
    this->status.auto_joypad_counter = 33;
}

void R5A22_delete(struct R5A22 *this)
{
    WDC65816_delete(&this->cpu);
}

void R5A22_reg_write(struct SNES *this, u32 addr, u32 val, struct SNES_memmap_block *bl)
{
    printf("\nWARN R5A22 WRITE NOT DONE");
}

u32 R5A22_reg_read(struct SNES *this, u32 addr, u32 old, u32 has_effect, struct SNES_memmap_block *bl)
{
    printf("\nWARN R5A22 REG READ NOT DONE");
    return 0;
}

void R5A22_setup_tracing(struct R5A22* this, struct jsm_debug_read_trace *strct)
{
    WDC65816_setup_tracing(&this->cpu, strct);
}

static void R5A22_cycle(void *ptr, u64 key, u64 clock, u32 jitter)
{
    struct SNES *snes = (struct SNES *)ptr;
    u64 cur = clock - jitter;



    scheduler_only_add_abs(&snes->scheduler, cur + snes->clock.cpu.divider, 0, snes, &R5A22_cycle, NULL);
}

void R5A22_schedule_first(struct SNES *snes)
{
    scheduler_only_add_abs(&snes->scheduler, 8, 0, snes, &R5A22_cycle, NULL);
}

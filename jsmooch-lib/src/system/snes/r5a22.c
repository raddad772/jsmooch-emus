//
// Created by . on 4/21/25.
//

#include <printf.h>
#include "r5a22.h"

void R5A22_init(struct R5A22 *this, u64 *master_cycle_count)
{
    WDC65816_init(&this->cpu, master_cycle_count);
}

void R5A22_reset(struct R5A22 *this)
{
    WDC65816_reset(&this->cpu);
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

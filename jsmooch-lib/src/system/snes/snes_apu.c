//
// Created by . on 4/23/25.
//

#include "snes_apu.h"

#include "snes_bus.h"

void SNES_APU_init(struct SNES *snes)
{
    SPC700_init(&snes->apu.cpu, &snes->clock.master_cycle_count);

}

void SNES_APU_delete(struct SNES *snes)
{
    SPC700_delete(&snes->apu.cpu);
}

void SNES_APU_reset(struct SNES *snes)
{
    SPC700_reset(&snes->apu.cpu);
}

static void SNES_APU_cycle(void *ptr, u64 key, u64 clock, u32 jitter)
{
    struct SNES *snes = (struct SNES *)ptr;
    u64 cur = clock - jitter;

    SPC700_cycle(&snes->apu.cpu, 1);

    // TODO: skip - amounts when scheduling
    /*i32 num = 0 - snes->apu.cpu.cycles;
    if (num < 1) num = 1;
    snes->clock.apu.cycle.next += ((long double)num * snes->clock.apu.cycle.stride);
    snes->apu.cpu.cycles = 0;*/
    scheduler_only_add_abs(&snes->scheduler, (i64)snes->clock.apu.cycle.next, 0, snes, &SNES_APU_cycle, NULL);
}

void SNES_APU_schedule_first(struct SNES *snes)
{
    snes->clock.apu.cycle.next = snes->clock.apu.cycle.stride;

    scheduler_only_add_abs(&snes->scheduler, (u64)snes->clock.apu.cycle.next, 0, snes, &SNES_APU_cycle, NULL);
}

u32 SNES_APU_read(struct SNES *snes, u32 addr, u32 old, u32 has_effect)
{
    return snes->apu.cpu.io.CPUO[addr & 3];
}

void SNES_APU_write(struct SNES *snes, u32 addr, u32 val)
{
    snes->apu.cpu.io.CPUI[addr & 3] = val;
}

//
// Created by . on 5/1/25.
//

#include "gba_timers.h"
#include "gba_bus.h"

namespace GBA {

TIMER::TIMER(core *parent, int num_in):
    gba(parent),
    num(num_in) {
    int a = 4;
    if (num_in == 0)
        a++;
}

    bool TIMER::enabled() const {
    return gba->clock_current() >= enable_at;
}

u32 TIMER::read() const
{
    const u64 current_time = gba->clock.master_cycle_count + gba->waitstates.current_transaction;
    if (!enabled() || cascade) return val_at_stop;

    // Timer is enabled, so, check how many cycles we have had...
    u64 ticks_passed = ((current_time - 1) - enable_at) >> shift;
    u32 v = val_at_stop + ticks_passed;
    return v;
}

void TIMER::cascade_timer_step(u64 current_time)
{
    val_at_stop = (val_at_stop + 1) & 0xFFFF;
    if (val_at_stop == 0) {
        overflow(current_time);
    }
}

void TIMER::overflow(u64 current_time) {
    enable_at = current_time;
    val_at_stop = reload;
    reload_ticks = timer_reload_ticks(reload) << shift;
    overflow_at = enable_at + reload_ticks;
    if (!cascade)
        sch_id = gba->scheduler.add_or_run_abs(static_cast<i64>(overflow_at), num, gba, &timer_overflow, &sch_scheduled_still);

    if (irq_on_overflow) {
        const u32 old_IF = gba->io.IF;
        gba->io.IF |= 8 << num;
        if (old_IF != gba->io.IF) {
            gba->eval_irqs();
        }
    }

    if (gba->apu.fifo[0].timer_id == num) gba->apu.sound_FIFO(0);
    if (gba->apu.fifo[1].timer_id == num) gba->apu.sound_FIFO(1);

    if (num < 3) {
        // Check for cascade!
        TIMER &tp1 = gba->timer[num+1];
        if (tp1.cascade) {
            tp1.cascade_timer_step(current_time);
        }
    }
}

void TIMER::timer_overflow(void *ptr, u64 timer_num, u64 current_clock, u32 jitter)
{
    auto *gba = static_cast<GBA::core *>(ptr);
    gba->timer[timer_num].overflow(gba->clock_current());
}

void TIMER::write_cnt(u32 val, u64 clock, u32 jitter)
{
    u64 cur_clock = clock - jitter;
    //printf("\nCNT%d  enable:%d  cascade:%d", tn, (val >> 7) & 1, (val >> 2) & 1);

    //u32 old_enable = GBA_timer_enabled(tn);

    // Delete currently-scheduled overflow
    val_at_stop = read();
    if (sch_scheduled_still) {
        gba->scheduler.delete_if_exist(sch_id);
        sch_id = 0;
    }
    enable_at = 0xFFFFFFFFFFFFFFFF;
    overflow_at = 0xFFFFFFFFFFFFFFFF;

    // Setup timer settings
    divider.io = val & 3;
    switch(val & 3) {
        case 0: shift = 0; break;
        case 1: shift = 6; break;
        case 2: shift = 8; break;
        case 3: shift = 10; break;
        default: NOGOHERE;
    }
    const u32 old_enable = enabled();
    const u32 new_enable = ((val >> 7) & 1);
    cascade = (val >> 2) & 1;
    irq_on_overflow = (val >> 6) & 1;

    // Schedule new overflow if necessary
    if (!old_enable && new_enable) {
        val_at_stop = reload;
    }
    if (new_enable && !cascade) {
        enable_at = cur_clock + 1;
        reload_ticks = timer_reload_ticks(val_at_stop) << shift;
        overflow_at = cur_clock + reload_ticks;
        sch_id = gba->scheduler.add_or_run_abs(static_cast<i64>(overflow_at), num, gba, &timer_overflow, &sch_scheduled_still);
    }
}
}
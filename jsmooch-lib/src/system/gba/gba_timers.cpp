//
// Created by . on 5/1/25.
//

#include "gba_timers.h"
#include "gba_bus.h"

namespace GBA {

bool TIMER::enabled() const {
    return bus->clock_current() >= enable_at;
}

u32 TIMER::read() const
{
    const u64 current_time = bus->clock.master_cycle_count + bus->waitstates.current_transaction;
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
        sch_id = bus->scheduler.add_or_run_abs(static_cast<i64>(overflow_at), num, this, &timer_overflow, &sch_scheduled_still);

    if (irq_on_overflow) {
        u32 old_IF = bus->io.IF;
        bus->io.IF |= 8 << num;
        if (old_IF != bus->io.IF) {
            bus->eval_irqs();
        }
    }

    if (bus->apu.fifo[0].timer_id == num) bus->apu.sound_FIFO(0);
    if (bus->apu.fifo[1].timer_id == num) bus->apu.sound_FIFO(1);

    if (num < 3) {
        // Check for cascade!
        TIMER &tp1 = bus->timer[num+1];
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

void TIMER::write_cnt(void *ptr, u64 tn_and_val, u64 clock, u32 jitter)
{
    u64 cur_clock = clock - jitter;
    u32 tn = (tn_and_val >> 24);
    u32 val = (tn_and_val & 0xFF);
    auto *gba = static_cast<GBA::core *>(ptr);
    auto &t = gba->timer[tn];

    //printf("\nCNT%d  enable:%d  cascade:%d", tn, (val >> 7) & 1, (val >> 2) & 1);

    //u32 old_enable = GBA_timer_enabled(tn);

    // Delete currently-scheduled overflow
    t.val_at_stop = t.read();
    if (t.sch_scheduled_still) {
        gba->scheduler.delete_if_exist(t.sch_id);
        t.sch_id = 0;
    }
    t.enable_at = 0xFFFFFFFFFFFFFFFF;
    t.overflow_at = 0xFFFFFFFFFFFFFFFF;

    // Setup timer settings
    t.divider.io = val & 3;
    switch(val & 3) {
        case 0: t.shift = 0; break;
        case 1: t.shift = 6; break;
        case 2: t.shift = 8; break;
        case 3: t.shift = 10; break;
        default: NOGOHERE;
    }
    const u32 old_enable = t.enabled();
    const u32 new_enable = ((val >> 7) & 1);
    t.cascade = (val >> 2) & 1;
    t.irq_on_overflow = (val >> 6) & 1;

    // Schedule new overflow if necessary
    if (!old_enable && new_enable) {
        t.val_at_stop = t.reload;
    }
    if (new_enable && !t.cascade) {
        t.enable_at = cur_clock + 1;
        t.reload_ticks = timer_reload_ticks(t.val_at_stop) << t.shift;
        t.overflow_at = cur_clock + t.reload_ticks;
        t.sch_id = gba->scheduler.add_or_run_abs(static_cast<i64>(t.overflow_at), tn, gba, &timer_overflow, &t.sch_scheduled_still);
    }
}
}
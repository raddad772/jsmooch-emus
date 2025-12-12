//
// Created by . on 1/20/25.
//

#include "nds_timers.h"
#include "nds_bus.h"
#include "nds_irq.h"

namespace NDS {

u32 timer_reload_ticks(u32 reload)
{
    // So it overflows at 0x100
    if (reload == 0xFFFF) return 0x10000;
    return 0x10000 - reload;
}

bool timer7_t::enabled() {
    return bus->clock.current7() >= enable_at;
}

bool timer9_t::enabled() {
    return bus->clock.current9() >= enable_at;
}

void timer7_t::cascade_step(u64 current_time)
{
    //printf("\nCASCADE TIMER STEP!");
    val_at_stop = (val_at_stop + 1) & 0xFFFF;
    if (val_at_stop == 0) {
        overflow(current_time);
    }
}

void timer9_t::cascade_step(u64 current_time)
{
    val_at_stop = (val_at_stop + 1) & 0xFFFF;
    if (val_at_stop == 0) {
        overflow(current_time);
    }
}

void timer7_t::overflow(u64 current_time) {
    //printf("\nOVERFLOW: %d", tn);
    enable_at = current_time;
    val_at_stop = reload;
    reload_ticks = timer_reload_ticks(reload) << shift;
    overflow_at = enable_at + reload_ticks;
    if (!cascade)
        sch_id = bus->scheduler.add_or_run_abs(overflow_at, num, bus, &timer7_t::overflow_callback, &sch_scheduled_still);

    if (irq_on_overflow) {
        //printf("\nIRQ!");
        bus->update_IF7(IRQ_TIMER0 + num);
    }

    if (num < 3) {
        // Check for cascade!
        timer7_t &tp1 = bus->timer7[num+1];
        if (tp1.cascade) {
            tp1.cascade_step(current_time);
        }
    }
}

void timer9_t::overflow(u64 current_time)
{
    enable_at = current_time;
    val_at_stop = reload;
    reload_ticks = timer_reload_ticks(reload) << shift;
    overflow_at = enable_at + reload_ticks;
    if (!cascade)
        sch_id = bus->scheduler.add_or_run_abs(overflow_at, num, bus, &timer9_t::overflow_callback, &sch_scheduled_still);

    if (irq_on_overflow) {
        bus->update_IF9(IRQ_TIMER0 + num);
    }

    if (num < 3) {
        // Check for cascade!
        timer9_t &tp1 = bus->timer9[num+1];
        if (tp1.cascade) {
            tp1.cascade_step(current_time);
        }
    }
}

u32 timer7_t::read()
{
    u64 current_time = bus->clock.master_cycle_count7 + bus->waitstates.current_transaction;
    if (!enabled() || cascade) return val_at_stop;

    // Timer is enabled, so, check how many cycles we have had...
    u64 ticks_passed = (((current_time - 1) - enable_at) >> shift);
    u32 v = val_at_stop + ticks_passed;
    return v;
}

u32 timer9_t::read()
{
    u64 current_time = bus->clock.master_cycle_count9 + bus->waitstates.current_transaction;
    if (!enabled() || cascade) return val_at_stop;

    // Timer is enabled, so, check how many cycles we have had...
    u64 ticks_passed = (((current_time - 1) - enable_at) >> shift);
    u32 v = val_at_stop + ticks_passed;
    return v;
}

void timer7_t::overflow_callback(void *ptr, u64 timer_num, u64 current_clock, u32 jitter)
{
    auto *th = static_cast<core *>(ptr);
    th->timer7[timer_num].overflow(th->clock.current7());
}

void timer9_t::overflow_callback(void *ptr, u64 timer_num, u64 current_clock, u32 jitter)
{
    auto *th = static_cast<core *>(ptr);
    th->timer9[timer_num].overflow(th->clock.current9());
}


void timer7_t::write_cnt(u32 val)
{
    val_at_stop = read();
    if (sch_scheduled_still) {
        bus->scheduler.delete_if_exist(sch_id);
        sch_id = 0;
    }
    enable_at = 0xFFFFFFFFFFFFFFFF;
    overflow_at = 0xFFFFFFFFFFFFFFFF;

    divider.io = val & 3;
    switch(val & 3) {
        case 0: shift = 0; break;
        case 1: shift = 6; break;
        case 2: shift = 8; break;
        case 3: shift = 10; break;
    }
    bool old_enable = enabled();
    bool new_enable = ((val >> 7) & 1);
    cascade = (val >> 2) & 1;
    irq_on_overflow = (val >> 6) & 1;

    // Schedule new overflow if necessary
    if (!old_enable && new_enable) {
        val_at_stop = reload;
    }
    if (new_enable && !cascade) {
        u64 cur_clock = bus->clock.current7();
        enable_at = cur_clock + 1;
        reload_ticks = timer_reload_ticks(val_at_stop) << shift;
        overflow_at = cur_clock + reload_ticks;
        sch_id = bus->scheduler.add_or_run_abs(overflow_at, num, bus, &timer7_t::overflow_callback, &sch_scheduled_still);
    }
}


void timer9_t::write_cnt(u32 val)
{
    val_at_stop = read();
    if (sch_scheduled_still) {
        bus->scheduler.delete_if_exist(sch_id);
        sch_id = 0;
    }
    enable_at = 0xFFFFFFFFFFFFFFFF;
    overflow_at = 0xFFFFFFFFFFFFFFFF;

    divider.io = val & 3;
    switch(val & 3) {
        case 0: shift = 0; break;
        case 1: shift = 6; break;
        case 2: shift = 8; break;
        case 3: shift = 10; break;
    }
    bool old_enable = enabled();
    bool new_enable = ((val >> 7) & 1);
    cascade = (val >> 2) & 1;
    irq_on_overflow = (val >> 6) & 1;

    // Schedule new overflow if necessary
    if (!old_enable && new_enable) {
        val_at_stop = reload;
    }
    if (new_enable && !cascade) {
        u64 cur_clock = bus->clock.current9();
        enable_at = cur_clock + 1;
        reload_ticks = timer_reload_ticks(val_at_stop) << shift;
        overflow_at = cur_clock + reload_ticks;
        sch_id = bus->scheduler.add_or_run_abs(overflow_at, num, bus, &timer9_t::overflow_callback, &sch_scheduled_still);
    }
}
}
//
// Created by . on 1/20/25.
//

#include "nds_timers.h"
#include "nds_bus.h"
#include "nds_irq.h"

static void timer_overflow(void *ptr, u64 timer_num, u64 current_clock, u32 jitter);

static u32 timer_reload_ticks(u32 reload)
{
    // So it overflows at 0x100
    if (reload == 0xFFFF) return 0x10000;
    return 0x10000 - reload;
}

u32 NDS_timer7_enabled(NDS *this, u32 tn) {
    return NDS_clock_current7(this) >= this->timer7[tn].enable_at;
}

u32 NDS_timer9_enabled(NDS *this, u32 tn) {
    return NDS_clock_current9(this) >= this->timer9[tn].enable_at;
}

static void overflow_timer7(NDS *this, u32 tn, u64 current_time);
static void overflow_timer9(NDS *this, u32 tn, u64 current_time);

static void cascade_timer7_step(NDS *this, u32 tn, u64 current_time)
{
    //printf("\nCASCADE TIMER STEP!");
    struct NDS_TIMER *t = &this->timer7[tn];
    t->val_at_stop = (t->val_at_stop + 1) & 0xFFFF;
    if (t->val_at_stop == 0) {
        overflow_timer7(this, tn, current_time);
    }
}

static void cascade_timer9_step(NDS *this, u32 tn, u64 current_time)
{
    //printf("\nCASCADE TIMER STEP!");
    struct NDS_TIMER *t = &this->timer9[tn];
    t->val_at_stop = (t->val_at_stop + 1) & 0xFFFF;
    if (t->val_at_stop == 0) {
        overflow_timer9(this, tn, current_time);
    }
}

static void overflow_timer7(NDS *this, u32 tn, u64 current_time) {
    struct NDS_TIMER *t = &this->timer7[tn];
    //printf("\nOVERFLOW: %d", tn);
    t->enable_at = current_time;
    t->val_at_stop = t->reload;
    t->reload_ticks = timer_reload_ticks(t->reload) << t->shift;
    t->overflow_at = t->enable_at + t->reload_ticks;
    if (!t->cascade)
        t->sch_id = scheduler_add_or_run_abs(&this->scheduler, t->overflow_at, tn, this, &timer_overflow, &t->sch_scheduled_still);

    if (t->irq_on_overflow) {
        //printf("\nIRQ!");
        NDS_update_IF7(this, IRQ_TIMER0 + tn);
    }

    if (tn < 3) {
        // Check for cascade!
        struct NDS_TIMER *tp1 = &this->timer7[tn+1];
        if (tp1->cascade) {
            cascade_timer7_step(this, tn+1, current_time);
        }
    }
}

static void overflow_timer9(NDS *this, u32 tn, u64 current_time)
{
    struct NDS_TIMER *t = &this->timer9[tn];
    t->enable_at = current_time;
    t->val_at_stop = t->reload;
    t->reload_ticks = timer_reload_ticks(t->reload) << t->shift;
    t->overflow_at = t->enable_at + t->reload_ticks;
    if (!t->cascade)
        t->sch_id = scheduler_add_or_run_abs(&this->scheduler, t->overflow_at, 0x10 + tn, this, &timer_overflow, &t->sch_scheduled_still);

    if (t->irq_on_overflow) {
        NDS_update_IF9(this, IRQ_TIMER0 + tn);
    }

    if (tn < 3) {
        // Check for cascade!
        struct NDS_TIMER *tp1 = &this->timer9[tn+1];
        if (tp1->cascade) {
            cascade_timer9_step(this, tn+1, current_time);
        }
    }
}

u32 NDS_read_timer7(NDS *this, u32 tn)
{
    struct NDS_TIMER *t = &this->timer7[tn];
    u64 current_time = this->clock.master_cycle_count7 + this->waitstates.current_transaction;
    if (!NDS_timer7_enabled(this, tn) || t->cascade) return t->val_at_stop;

    // Timer is enabled, so, check how many cycles we have had...
    u64 ticks_passed = (((current_time - 1) - t->enable_at) >> t->shift);
    u32 v = t->val_at_stop + ticks_passed;
    return v;
}

u32 NDS_read_timer9(NDS *this, u32 tn)
{
    struct NDS_TIMER *t = &this->timer9[tn];
    u64 current_time = this->clock.master_cycle_count9 + this->waitstates.current_transaction;
    if (!NDS_timer9_enabled(this, tn) || t->cascade) return t->val_at_stop;

    // Timer is enabled, so, check how many cycles we have had...
    u64 ticks_passed = (((current_time - 1) - t->enable_at) >> t->shift);
    u32 v = t->val_at_stop + ticks_passed;
    return v;
}

static void timer_overflow(void *ptr, u64 timer_num, u64 current_clock, u32 jitter)
{
    struct NDS *this = (NDS *)ptr;
    if (timer_num & 0x10) overflow_timer9(this, timer_num & 0x0F, NDS_clock_current7(this));
    else overflow_timer7(this, timer_num, NDS_clock_current7(this));
}

void NDS_timer7_write_cnt(NDS *this, u32 tn, u32 val)
{
    struct NDS_TIMER *t = &this->timer7[tn];

    t->val_at_stop = NDS_read_timer7(this, tn);
    if (t->sch_scheduled_still) {
        scheduler_delete_if_exist(&this->scheduler, t->sch_id);
        t->sch_id = 0;
    }
    t->enable_at = 0xFFFFFFFFFFFFFFFF;
    t->overflow_at = 0xFFFFFFFFFFFFFFFF;

    t->divider.io = val & 3;
    switch(val & 3) {
        case 0: t->shift = 0; break;
        case 1: t->shift = 6; break;
        case 2: t->shift = 8; break;
        case 3: t->shift = 10; break;
    }
    u32 old_enable = NDS_timer7_enabled(this, tn);
    u32 new_enable = ((val >> 7) & 1);
    t->cascade = (val >> 2) & 1;
    t->irq_on_overflow = (val >> 6) & 1;

    // Schedule new overflow if necessary
    if (!old_enable && new_enable) {
        t->val_at_stop = t->reload;
    }
    if (new_enable && !t->cascade) {
        u64 cur_clock = NDS_clock_current7(this);
        t->enable_at = cur_clock + 1;
        t->reload_ticks = timer_reload_ticks(t->val_at_stop) << t->shift;
        t->overflow_at = cur_clock + t->reload_ticks;
        t->sch_id = scheduler_add_or_run_abs(&this->scheduler, t->overflow_at, tn, this, &timer_overflow, &t->sch_scheduled_still);
    }
}


void NDS_timer9_write_cnt(NDS *this, u32 tn, u32 val)
{
    struct NDS_TIMER *t = &this->timer9[tn];

    t->val_at_stop = NDS_read_timer9(this, tn);
    if (t->sch_scheduled_still) {
        scheduler_delete_if_exist(&this->scheduler, t->sch_id);
        t->sch_id = 0;
    }
    t->enable_at = 0xFFFFFFFFFFFFFFFF;
    t->overflow_at = 0xFFFFFFFFFFFFFFFF;

    t->divider.io = val & 3;
    switch(val & 3) {
        case 0: t->shift = 0; break;
        case 1: t->shift = 6; break;
        case 2: t->shift = 8; break;
        case 3: t->shift = 10; break;
    }
    u32 old_enable = NDS_timer9_enabled(this, tn);
    u32 new_enable = ((val >> 7) & 1);
    t->cascade = (val >> 2) & 1;
    t->irq_on_overflow = (val >> 6) & 1;

    // Schedule new overflow if necessary
    if (!old_enable && new_enable) {
        t->val_at_stop = t->reload;
    }
    if (new_enable && !t->cascade) {
        u64 cur_clock = NDS_clock_current9(this);
        t->enable_at = cur_clock + 1;
        t->reload_ticks = timer_reload_ticks(t->val_at_stop) << t->shift;
        t->overflow_at = cur_clock + t->reload_ticks;
        t->sch_id = scheduler_add_or_run_abs(&this->scheduler, t->overflow_at, tn | 0x10, this, &timer_overflow, &t->sch_scheduled_still);
    }
}

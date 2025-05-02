//
// Created by . on 5/1/25.
//

#include "gba_timers.h"
#include "gba_bus.h"

u32 GBA_timer_enabled(struct GBA *this, u32 tn) {
    return GBA_clock_current(this) >= this->timer[tn].enable_at;
}

static inline u32 timer_reload_ticks(u32 reload)
{
    // So it overflows at 0x100
    if (reload == 0xFFFF) return 0x10000;
    return 0x10000 - reload;
}


u32 GBA_read_timer(struct GBA *this, u32 tn)
{
    struct GBA_TIMER *t = &this->timer[tn];
    u64 current_time = this->clock.master_cycle_count + this->waitstates.current_transaction;
    if (!GBA_timer_enabled(this, tn) || t->cascade) return t->val_at_stop;

    // Timer is enabled, so, check how many cycles we have had...
    u64 ticks_passed = (((current_time - 1) - t->enable_at) >> t->shift) % (timer_reload_ticks(t->reload));
    u32 v = t->reload + ticks_passed;
    return v;
}

static void overflow_timer(struct GBA *this, u32 tn, u64 current_time);

static void cascade_timer_step(struct GBA *this, u32 tn, u64 current_time)
{
    struct GBA_TIMER *t = &this->timer[tn];
    t->val_at_stop = (t->val_at_stop + 1) & 0xFFFF;
    if (t->val_at_stop == 0) {
        overflow_timer(this, tn, current_time);
    }
}

static void timer_overflow(void *ptr, u64 timer_num, u64 current_clock, u32 jitter);

static void overflow_timer(struct GBA *this, u32 tn, u64 current_time) {
    struct GBA_TIMER *t = &this->timer[tn];
    //printf("\nOVERFLOW: %d", tn);
    t->enable_at = current_time;
    t->reload_ticks = timer_reload_ticks(t->reload) << t->shift;
    t->overflow_at = t->enable_at + t->reload_ticks;
    t->sch_id = scheduler_add_or_run_abs(&this->scheduler, t->overflow_at, tn, this, &timer_overflow, &t->sch_scheduled_still);

    t->val_at_stop = t->reload;
    if (t->irq_on_overflow) {
        u32 old_IF = this->io.IF;
        this->io.IF |= 1 << 1 << (3 + tn);
        if (old_IF != this->io.IF) {
            GBA_eval_irqs(this);
        }
    }

    if (this->apu.fifo[0].timer_id == tn) GBA_APU_sound_FIFO(this, 0);
    if (this->apu.fifo[1].timer_id == tn) GBA_APU_sound_FIFO(this, 1);

    if (tn < 3) {
        // Check for cascade!
        struct GBA_TIMER *tp1 = &this->timer[tn+1];
        if (GBA_timer_enabled(this, tn + 1) && tp1->cascade) {
            cascade_timer_step(this, tn+1, current_time);
        }
    }
}



static void timer_overflow(void *ptr, u64 timer_num, u64 current_clock, u32 jitter)
{
    struct GBA *this = (struct GBA *)ptr;
    overflow_timer(this, timer_num, GBA_clock_current(this));
}

void GBA_timer_write_cnt(struct GBA *this, u32 tn, u32 val)
{
    struct GBA_TIMER *t = &this->timer[tn];
    u32 old_enable = GBA_timer_enabled(this, tn);
    t->divider.io = val & 3;
    switch(val & 3) {
        case 0: t->shift = 0; break;
        case 1: t->shift = 6; break;
        case 2: t->shift = 8; break;
        case 3: t->shift = 10; break;
    }
    u32 new_enable = ((val >> 7) & 1);
    if (old_enable && !new_enable) { // turn off
        t->val_at_stop = GBA_read_timer(this, tn);
        if (t->sch_scheduled_still) {
            scheduler_delete_if_exist(&this->scheduler, t->sch_id);
            t->sch_id = 0;
        }
        t->enable_at = 0xFFFFFFFFFFFFFFFF; // the infinite future!
        t->overflow_at = 0xFFFFFFFFFFFFFFFF;
    }
    u32 old_cascade = t->cascade;
    t->cascade = (val >> 2) & 1;
    if (old_cascade && !t->cascade && (old_enable == new_enable == 1)) { // update overflow time
        if (t->sch_scheduled_still) {
            scheduler_delete_if_exist(&this->scheduler, t->sch_id);
            t->sch_id = 0;
        }
        t->enable_at = GBA_clock_current(this);
        t->overflow_at = t->enable_at + (timer_reload_ticks(t->val_at_stop) << t->shift);
        t->sch_id = scheduler_add_or_run_abs(&this->scheduler, t->overflow_at, tn, this, &timer_overflow, &t->sch_scheduled_still);
    }
    if (!old_enable && new_enable) { // turn on
        t->enable_at = GBA_clock_current(this) + 1;
        t->reload_ticks = timer_reload_ticks(t->reload) << t->shift;
        t->overflow_at = t->enable_at + t->reload_ticks;
        t->sch_id = scheduler_add_or_run_abs(&this->scheduler, t->overflow_at, tn, this, &timer_overflow, &t->sch_scheduled_still);
        t->val_at_stop = t->reload;
    }
    t->irq_on_overflow = (val >> 6) & 1;
}

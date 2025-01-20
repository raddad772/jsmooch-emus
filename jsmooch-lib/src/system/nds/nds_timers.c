//
// Created by . on 1/20/25.
//

#include "nds_timers.h"
#include "nds_bus.h"
#include "nds_irq.h"

static u32 timer_reload_ticks(u32 reload)
{
    // So it overflows at 0x100
    if (reload == 0xFFFF) return 0x10000;
    return 0x10000 - reload;
}

u32 NDS_timer7_enabled(struct NDS *this, u32 tn) {
    return NDS_clock_current7(this) >= this->timer7[tn].enable_at;
}

u32 NDS_timer9_enabled(struct NDS *this, u32 tn) {
    return NDS_clock_current9(this) >= this->timer7[tn].enable_at;
}

static void overflow_timer7(struct NDS *this, u32 tn, u64 current_time);
static void overflow_timer9(struct NDS *this, u32 tn, u64 current_time);

static void cascade_timer7_step(struct NDS *this, u32 tn, u64 current_time)
{
    //printf("\nCASCADE TIMER STEP!");
    struct NDS_TIMER *t = &this->timer7[tn];
    t->val_at_stop = (t->val_at_stop + 1) & 0xFFFF;
    if (t->val_at_stop == 0) {
        overflow_timer7(this, tn, current_time);
    }
}

static void cascade_timer9_step(struct NDS *this, u32 tn, u64 current_time)
{
    //printf("\nCASCADE TIMER STEP!");
    struct NDS_TIMER *t = &this->timer9[tn];
    t->val_at_stop = (t->val_at_stop + 1) & 0xFFFF;
    if (t->val_at_stop == 0) {
        overflow_timer9(this, tn, current_time);
    }
}

static void overflow_timer7(struct NDS *this, u32 tn, u64 current_time) {
    struct NDS_TIMER *t = &this->timer7[tn];
    //printf("\nOVERFLOW: %d", tn);
    if (!t->cascade) {
        t->enable_at = current_time;
        t->reload_ticks = timer_reload_ticks(t->reload) << t->shift;
        t->overflow_at = t->enable_at + t->reload_ticks;
        //printf("\nNew overflow in %lld cycles", t->overflow_at - t->enable_at);
    }
    t->val_at_stop = t->reload;
    if (t->irq_on_overflow) {
        //printf("\nIRQ!");
        NDS_update_IF7(this, 3 + tn, 1);
    }

    if (tn < 3) {
        // Check for cascade!
        struct NDS_TIMER *tp1 = &this->timer7[tn+1];
        if (NDS_timer7_enabled(this, tn + 1) && tp1->cascade) {
            cascade_timer7_step(this, tn+1, current_time);
        }
    }
}

static void overflow_timer9(struct NDS *this, u32 tn, u64 current_time)
{
    struct NDS_TIMER *t = &this->timer9[tn];
    if (!t->cascade) {
        t->enable_at = current_time;
        t->reload_ticks = timer_reload_ticks(t->reload) << t->shift;
        t->overflow_at = t->enable_at + t->reload_ticks;
    }
    t->val_at_stop = t->reload;
    if (t->irq_on_overflow) {
        NDS_update_IF9(this, 3 + tn, 1);
    }

    if (tn < 3) {
        // Check for cascade!
        struct NDS_TIMER *tp1 = &this->timer9[tn+1];
        if (NDS_timer9_enabled(this, tn + 1) && tp1->cascade) {
            cascade_timer9_step(this, tn+1, current_time);
        }
    }
}

void NDS_tick_timers7(struct NDS *this, u32 num_ticks) {
    for (u32 ticks = 0; ticks < num_ticks; ticks++) {
        u64 current_time = this->clock.master_cycle_count7 + ticks;
        // Check for overflow...
        for (u32 tn = 0; tn < 4; tn++) {
            struct NDS_TIMER *t = &this->timer7[tn];
            if (!t->cascade && (current_time >= t->overflow_at)) {
                overflow_timer7(this, tn, current_time);
            }
        }
    }
}

void NDS_tick_timers9(struct NDS *this, u32 num_ticks) {
    for (u32 ticks = 0; ticks < num_ticks; ticks++) {
        u64 current_time = this->clock.master_cycle_count9 + ticks;
        // Check for overflow...
        for (u32 tn = 0; tn < 4; tn++) {
            struct NDS_TIMER *t = &this->timer9[tn];
            if (!t->cascade && (current_time >= t->overflow_at)) {
                overflow_timer9(this, tn, current_time);
            }
        }
    }
}

u32 NDS_read_timer7(struct NDS *this, u32 tn)
{
    struct NDS_TIMER *t = &this->timer7[tn];
    u64 current_time = this->clock.master_cycle_count7 + this->waitstates.current_transaction;
    if (!NDS_timer7_enabled(this, tn) || t->cascade) return t->val_at_stop;

    // Timer is enabled, so, check how many cycles we have had...
    u64 ticks_passed = (((current_time - 1) - t->enable_at) >> t->shift) % (timer_reload_ticks(t->reload));
    u32 v = t->reload + ticks_passed;
    return v;
}

u32 NDS_read_timer9(struct NDS *this, u32 tn)
{
    struct NDS_TIMER *t = &this->timer9[tn];
    u64 current_time = this->clock.master_cycle_count9 + this->waitstates.current_transaction;
    if (!NDS_timer9_enabled(this, tn) || t->cascade) return t->val_at_stop;

    // Timer is enabled, so, check how many cycles we have had...
    u64 ticks_passed = (((current_time - 1) - t->enable_at) >> t->shift) % (timer_reload_ticks(t->reload));
    u32 v = t->reload + ticks_passed;
    return v;
}

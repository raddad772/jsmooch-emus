//
// Created by . on 3/4/25.
//

#ifndef JSMOOCH_EMUS_PS1_TIMERS_H
#define JSMOOCH_EMUS_PS1_TIMERS_H

#include "helpers/int.h"

struct PS1_TIMERS {
    struct {
        u64 sched_id;
        u64 sch_cycle;
        u32 still_sched;
        u32 at;
    } overflow;

    struct {
        u64 cycle;
        u64 value;
    } start;

    u32 on_system_clock;


    u32 target;
    u32 running;

    union {
        struct {
            u32 sync_enable : 1; // bit 0
            u32 sync_mode : 2; // bits 1-2
            u32 reset_when : 1; // bit 3. 0 = FFFF, 1 = counter=target
            u32 irq_on_target : 1; // bit 4
            u32 irq_on_ffff : 1; // bit 5
            u32 irq_repeat : 1; // bit 6
            u32 irq_toggle : 1; // bit 7
            u32 clock_source : 2; // bit 8-9
            u32 irq_request : 1; // bit 10 interrupt level, read-only. 0=yes, 1=no, set to 1 on write?
            u32 reached_target : 1; // bit 11
            u32 reached_ffff : 1; // bit 12
        };
        u32 u;
    } mode;
};

struct PS1;
u64 PS1_dotclock(PS1 *);
void PS1_timers_hblank(PS1 *, u64 key);
u32 PS1_timers_read(PS1 *, u32 addr, u32 sz);
void PS1_timers_write(PS1 *, u32 addr, u32 sz, u32 val);
void PS1_timers_vblank(PS1 *, u64 key);
void PS1_disable_timer(PS1 *, u32 timer_num);
void PS1_enable_timer(PS1 *, u32 timer_num);
void PS1_timers_reset(PS1 *);

#endif //JSMOOCH_EMUS_PS1_TIMERS_H

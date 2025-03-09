//
// Created by . on 3/4/25.
//

#include <stdlib.h>

#include "ps1_timers.h"
#include "../ps1_bus.h"

static void write_timer(struct PS1 *this, u32 timer_num, u32 val, u32 sz);

static void deschedule(struct PS1 *this, struct PS1_TIMERS *t)
{
    if (t->overflow.still_sched) {
        scheduler_delete_if_exist(&this->scheduler, t->overflow.sched_id);
    }
}

void PS1_timers_reset(struct PS1 *this)
{
    for (u32 i = 0; i < 3; i++) {
        struct PS1_TIMERS *t = &this->timers[i];
        t->on_system_clock = 1;
        t->mode.u = 0;
        deschedule(this, t);
        t->start.cycle = 0;
        t->start.value = 0;
        t->running = 1;
    }
}

void PS1_timers_vblank(struct PS1 *this, u64 key) {
    struct PS1_TIMERS *t = &this->timers[1];
    if (key == 0) { // vblank off
        if (t->mode.sync_enable) {
            switch (t->mode.sync_mode) {
                case 0: // 0 = Pause counter during Hblank(s)
                    PS1_disable_timer(this, 1);
                    break;
                case 1: // 1 = Reset counter to 0000h at Hblank(s)
                    write_timer(this, 1, 0, 4);
                    break;
                case 2: // 2 = Reset counter to 0000h at Hblank(s) and pause outside of Hblank
                    write_timer(this, 1, 0, 4);
                    PS1_enable_timer(this, 1);
                    break;
                case 3: // 3 = Pause until Hblank occurs once, then switch to Free Run
                    break;
            }
        }
    }
    else { // vblank on
        if (t->mode.sync_enable) {
            switch (t->mode.sync_mode) {
                case 0: // 0 = Pause counter during Hblank(s)
                    PS1_enable_timer(this, 1);
                    break;
                case 1: // 1 = Reset counter to 0000h at Hblank(s)
                    break;
                case 2: // 2 = Reset counter to 0000h at Hblank(s) and pause outside of Hblank
                    write_timer(this, 1, 0, 4);
                    PS1_disable_timer(this, 1);
                    break;
                case 3: // 3 = Pause until vblank occurs once, then switch to Free Run
                    PS1_enable_timer(this, 1);
                    t->mode.sync_enable = 0;
                    break;
            }
        }
    }
}

void PS1_timers_hblank(struct PS1 *this, u64 key)
{
    struct PS1_TIMERS *t = &this->timers[0];
    if (key == 0) { // hblank off
        if (t->mode.sync_enable) {
            switch (t->mode.sync_mode) {
                case 0: // 0 = Pause counter during Hblank(s)
                    PS1_disable_timer(this, 0);
                    break;
                case 1: // 1 = Reset counter to 0000h at Hblank(s)
                    write_timer(this, 0, 0, 4);
                    break;
                case 2: // 2 = Reset counter to 0000h at Hblank(s) and pause outside of Hblank
                    write_timer(this, 0, 0, 4);
                    PS1_enable_timer(this, 0);
                    break;
                case 3: // 3 = Pause until Hblank occurs once, then switch to Free Run
                    break;
            }
        }
    }
    else { // hblank on
        if (t->mode.sync_enable) {
            switch (t->mode.sync_mode) {
                case 0: // 0 = Pause counter during Hblank(s)
                    PS1_enable_timer(this, 0);
                    break;
                case 1: // 1 = Reset counter to 0000h at Hblank(s)
                    break;
                case 2: // 2 = Reset counter to 0000h at Hblank(s) and pause outside of Hblank
                    write_timer(this, 0, 0, 4);
                    PS1_disable_timer(this, 0);
                    break;
                case 3: // 3 = Pause until Hblank occurs once, then switch to Free Run
                    PS1_enable_timer(this, 0);
                    t->mode.sync_enable = 0;
                    break;
            }
        }
    }
}

u64 PS1_dotclock(struct PS1 *this)
{
    return (u64)((float)this->clock.master_cycle_count * this->clock.dot.ratio.cpu_to_dotclock);
}

static u32 read_timer_clk(struct PS1_TIMERS *t, u64 clk)
{
    u32 r = t->start.value;
    if (t->running) r += clk - t->start.cycle;
    assert(r<0x10000);
    return r;
}

static u64 get_clock_source(struct PS1 *this, u32 timer_num)
{
    struct PS1_TIMERS *t = &this->timers[timer_num];
    if (timer_num == 1) printf("\nTimer %d on system clock: %d. master count:%lld", timer_num, t->on_system_clock, this->clock.master_cycle_count);
    if (t->on_system_clock) return this->clock.master_cycle_count;
    switch(timer_num) {
        case 0:
            return PS1_dotclock(this);
        case 1:
            return this->clock.hblank_clock;
        case 2:
            return this->clock.master_cycle_count >> 3;
        default:
            NOGOHERE;
    }
}

static u32 read_timer(struct PS1 *this, u32 timer_num) {
    struct PS1_TIMERS *t = &this->timers[timer_num];
    u32 v = read_timer_clk(t, get_clock_source(this, timer_num));
    if (timer_num == 1) printf("\nREAD timer %d: %04x", timer_num, v);
    return v;
}

static void timer_irq_down(void *ptr, u64 key, u64 clock, u32 jitter)
{
    struct PS1 *this = (struct PS1 *)ptr;
    struct PS1_TIMERS *t = &this->timers[key];
    t->mode.irq_request = 1;
    PS1_set_irq(this, PS1IRQ_TMR0+key, 0);
}

static void update_irqs(struct PS1 *this)
{
    for (u32 i = 0; i < 3; i++) {
        struct PS1_TIMERS *t = &this->timers[i];
        u32 b = t->mode.irq_on_ffff && t->mode.reached_ffff;
        b |= t->mode.irq_on_target && t->mode.reached_target;
        //u32 old_request = t->mode.irq_request;
        t->mode.irq_request &= b ^ 1;
        u32 irq_num = PS1IRQ_TMR0 + i;
        if ((this->IRQ_multiplexer.irqs[irq_num].input == 0) && b) {
            // Set up
            PS1_set_irq(this, PS1IRQ_TMR0 + i, b);
            printf("\nTIMER%d IRQ SET!", i);

            // Schedule down-set
            scheduler_only_add_abs(&this->scheduler, PS1_clock_current(this) + 100, i, this, &timer_irq_down, NULL);
        }
        else
            PS1_set_irq(this, PS1IRQ_TMR0 + i, 0);
    }
}

u32 PS1_timers_read(struct PS1 *this, u32 addr, u32 sz)
{
    u32 timer_num = (addr >> 4) & 3;
    printf("\nREAD FROM %08x: %d", addr, timer_num);
    switch(addr & 0x1FFFFFCF) {
        case 0x1F801100: // current counter value
            return read_timer(this, timer_num) & 0xFFFF;
        case 0x1F801104: { // timer0...2 mode
            u32 v = this->timers[timer_num].mode.u;
            this->timers[timer_num].mode.reached_target = 0;
            this->timers[timer_num].mode.reached_ffff = 0;
            update_irqs(this);
            return v; }
        case 0x1F801108: // timer0...2 target value
            return this->timers[timer_num].target;
    }

    printf("\nUnhandled timers read %08x (%d)", addr, sz);
    return 0;
}

// Determine ticks until overflow
// Schedule if it's not at the same time
void PS1_disable_timer(struct PS1 *this, u32 timer_num)
{
    struct PS1_TIMERS *t = &this->timers[timer_num];
    if (!t->running) return;
    t->running = 0;
    t->start.value = read_timer_clk(t, get_clock_source(this, timer_num));
    if (t->overflow.still_sched) {
        scheduler_delete_if_exist(&this->scheduler, t->overflow.sched_id);
    }
}

static inline u64 time_til_next_hblank(struct PS1 *this, u64 clk)
{

    // Determine which cycle of current line
    u64 line_cycle = (clk - this->clock.frame_cycle_start) % this->clock.timing.scanline.cycles;

    if (line_cycle >= this->clock.timing.scanline.hblank.start_on_cycle) { // We're in the hblank portion of a line,
        // Account for rest of scanline
        return (this->clock.timing.scanline.cycles - line_cycle) + this->clock.timing.scanline.hblank.start_on_cycle;
    }
    else { // We're not in the hblank portion of a line.
        return this->clock.timing.scanline.hblank.start_on_cycle - line_cycle;
    }
}

static u64 calculate_timer1_hblank(struct PS1 *this, u32 diff)
{
    // This routine answers one question:
    // How many CPU cycles until x hblanks have happened?

    u64 clk = PS1_clock_current(this);
    // First, determine time til next hblank
    u64 nd = time_til_next_hblank(this, clk);
    assert(diff!=0);
    diff--;
    nd += this->clock.timing.scanline.cycles * diff;
    return nd;
}

static void reschedule_timer(struct PS1 *this, u32 timer_num);

static void timer_overflow(void *ptr, u64 timer_num, u64 current_clock, u32 jitter)
{
    u64 clk = current_clock - jitter;
    struct PS1 *this = (struct PS1 *)ptr;
    struct PS1_TIMERS *t = &this->timers[timer_num];

    u32 value = t->overflow.at & 0xFFFF;

    if (value == 0xFFFF) {
        t->mode.reached_ffff = 1;
    }
    if (value == t->target) {
        t->mode.reached_target = 1;
    }
    update_irqs(this);

    // Now determine if we reset...
    if ((value == 0xFFFF) || ((value == t->target) && t->mode.reset_when)) {
        t->start.value = 0;
    }
    if (t->on_system_clock) t->start.cycle = PS1_clock_current(this) + 2;
    else t->start.cycle = get_clock_source(this, timer_num);

    // Schedule next overflow...
    reschedule_timer(this, timer_num);
}

static void reschedule_timer(struct PS1 *this, u32 timer_num)
{
    struct PS1_TIMERS *t = &this->timers[timer_num];

    if (!t->running) {
        deschedule(this, t);
        return;
    }

    // TODO: calculate out complicated hblank/vblank stuff. it's doable.

    // Overflow is either on 0xFFFF or target, whichever is next...
    u32 overflow_at = 0xFFFF;
    u64 clk = get_clock_source(this, timer_num);
    u32 current_value  = read_timer_clk(t, clk) & 0xFFFF;

    if (current_value < t->target) overflow_at = t->target;
    t->overflow.at = overflow_at;


    if (current_value == overflow_at) overflow_at += t->target + 0x10000;

    u32 diff = overflow_at - current_value;

    // Now convert to our clock cycles...
    if (!t->on_system_clock) {
        switch(timer_num) {
            case 0: // on the dot clock...
                diff = (u64)((float)diff * this->clock.dot.ratio.cpu_to_dotclock);
                break;
            case 1: // on hblank...we shouldn't be here.
                diff = calculate_timer1_hblank(this, diff);
                break;
            case 2: // diff is currently in >>=3-ness.
                diff <<= 3;
                break;
            default:
                NOGOHERE;
        }
    }

    u64 on_cycle = PS1_clock_current(this) + diff;

    u32 do_sched = 1;
    if (t->overflow.still_sched) {
        if (t->overflow.sch_cycle != on_cycle) {
            deschedule(this, t);
        }
        else { // it's already scheduled for the proper time
            do_sched = 0;
        }
    }
    if (do_sched) {
        t->overflow.sch_cycle = on_cycle;
        t->overflow.sched_id = scheduler_only_add_abs(&this->scheduler, on_cycle, timer_num, this, &timer_overflow, &t->overflow.still_sched);
    }
}

void PS1_enable_timer(struct PS1 *this, u32 timer_num)
{
    struct PS1_TIMERS *t = &this->timers[timer_num];
    if (t->running) return;

    t->running = 1;

    u64 clk = get_clock_source(this, timer_num);
    if (t->on_system_clock) t->start.cycle = PS1_clock_current(this) + 2;
    else t->start.cycle = clk;
    reschedule_timer(this, timer_num);
}

static void setup_timer0(struct PS1 *this)
{
    struct PS1_TIMERS *t = &this->timers[0];
    // Need to determine if this timer is on sysclk or not,
    t->on_system_clock = (t->mode.clock_source & 1) ^ 1;

    // If it is running or not currently,
    t->running = !(t->mode.sync_enable && t->mode.sync_mode == 3); // if so, we pause until hblank occurs...
    if (t->on_system_clock) t->start.cycle = PS1_clock_current(this) + 2;
    else t->start.cycle = PS1_dotclock(this);

    // Determine overflow timing
    reschedule_timer(this, 0);
}

static void setup_timer1(struct PS1 *this)
{
    struct PS1_TIMERS *t = &this->timers[1];
    // Need to determine if this timer is on sysclk or not,
    t->on_system_clock = (t->mode.clock_source & 1) ^ 1;

    // If it is running or not currently,
    t->running = !(t->mode.sync_enable && t->mode.sync_mode == 3); // if so, we pause until vblank occurs...
    if (t->on_system_clock) t->start.cycle = PS1_clock_current(this) + 2;
    else t->start.cycle = this->clock.hblank_clock;

    // Determine overflow timing
    reschedule_timer(this, 1);
}

static void setup_timer2(struct PS1 *this)
{
    struct PS1_TIMERS *t = &this->timers[2];
    // Need to determine if this timer is on sysclk or not,
    t->on_system_clock = t->mode.clock_source < 2;

    // If it is running or not currently,
    if (t->mode.sync_enable) {
        t->running = (t->mode.sync_mode == 1) || (t->mode.sync_mode == 2);
    }
    else t->running = 1;

    if (t->on_system_clock) t->start.cycle = PS1_clock_current(this) + 2;
    else t->start.cycle = PS1_clock_current(this) >> 3;

    // Determine overflow timing
    reschedule_timer(this, 2);
}


static void do_timer_setup(struct PS1 *this, u32 timer_num)
{
    switch(timer_num) {
        case 0:
            setup_timer0(this);
            return;
        case 1:
            setup_timer1(this);
            return;
        case 2:
            setup_timer2(this);
            return;
    }
}

static void reset_timer(struct PS1 *this, u32 timer_num, u32 reset_to)
{
    struct PS1_TIMERS *t = &this->timers[timer_num];
    t->start.cycle = PS1_clock_current(this) + 2;
    t->start.value = reset_to;
}

static void write_timer(struct PS1 *this, u32 timer_num, u32 val, u32 sz)
{
    this->timers[timer_num].start.value = val & 0xFFFF;
    reschedule_timer(this, timer_num);
}

static void write_target(struct PS1 *this, u32 timer_num, u32 val, u32 sz)
{
    this->timers[timer_num].target = val & 0xFFFF;
    reschedule_timer(this, timer_num);
}

static void write_mode(struct PS1 *this, u32 timer_num, u32 val, u32 sz)
{
    val &= 0b1111111111;
    struct PS1_TIMERS *t = &this->timers[timer_num];
    u32 old_mode = t->mode.u;
    t->mode.u = (t->mode.u & 0b1111110000000000) | val;

    // Reset to 0
    this->timers[timer_num].start.value = 0;

    if (old_mode != t->mode.u)
        do_timer_setup(this, timer_num);
    else
        reschedule_timer(this, timer_num);

    t->mode.irq_request = 1;
    update_irqs(this);
}

void PS1_timers_write(struct PS1 *this, u32 addr, u32 sz, u32 val)
{
    u32 timer_num = (addr >> 4) & 3;
    switch(addr & 0x1FFFFFCF) {
        case 0x1F801100: // current counter value
            write_timer(this, timer_num, val, sz);
            return;
        case 0x1F801104: // timer0...2 mode
            write_mode(this, timer_num, val, sz);
            return;
        case 0x1F801108: // timer0...2 target value
            write_target(this, timer_num, val, sz);
            return;

    }
    printf("\nUnhandled timers write %08x: %08x (%d)", addr, val, sz);
}

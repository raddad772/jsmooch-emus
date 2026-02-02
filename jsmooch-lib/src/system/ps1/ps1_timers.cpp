//
// Created by . on 3/4/25.
//

#include <cstdlib>
#include <cassert>

#include "ps1_bus.h"
#include "ps1_timers.h"

namespace PS1 {
void TIMER::deschedule()
{
    if (overflow.still_sched) {
        bus->scheduler.delete_if_exist(overflow.sched_id);
    }
}

void TIMER::reset()
{
    on_system_clock = 1;
    mode.u = 0;
    deschedule();
    start.cycle = 0;
    start.value = 0;
    running = 1;
}

void TIMER::vblank(u64 key) {
    if (key == 0) { // vblank off
        if (mode.sync_enable) {
            switch (mode.sync_mode) {
                case 0: // 0 = Pause counter during Hblank(s)
                    disable();
                    break;
                case 1: // 1 = Reset counter to 0000h at Hblank(s)
                    write(0, 4);
                    break;
                case 2: // 2 = Reset counter to 0000h at Hblank(s) and pause outside of Hblank
                    write(0, 4);
                    enable();
                    break;
                case 3: // 3 = Pause until Hblank occurs once, then switch to Free Run
                    break;
            }
        }
    }
    else { // vblank on
        if (mode.sync_enable) {
            switch (mode.sync_mode) {
                case 0: // 0 = Pause counter during Hblank(s)
                    enable();
                    break;
                case 1: // 1 = Reset counter to 0000h at Hblank(s)
                    break;
                case 2: // 2 = Reset counter to 0000h at Hblank(s) and pause outside of Hblank
                    write(0, 4);
                    disable();
                    break;
                case 3: // 3 = Pause until vblank occurs once, then switch to Free Run
                    enable();
                    mode.sync_enable = 0;
                    break;
            }
        }
    }
}

void TIMER::hblank(u64 key)
{
    if (key == 0) { // hblank off
        if (mode.sync_enable) {
            switch (mode.sync_mode) {
                case 0: // 0 = Pause counter during Hblank(s)
                    disable();
                    break;
                case 1: // 1 = Reset counter to 0000h at Hblank(s)
                    write(0, 4);
                    break;
                case 2: // 2 = Reset counter to 0000h at Hblank(s) and pause outside of Hblank
                    write(0, 4);
                    enable();
                    break;
                case 3: // 3 = Pause until Hblank occurs once, then switch to Free Run
                    break;
            }
        }
    }
    else { // hblank on
        if (mode.sync_enable) {
            switch (mode.sync_mode) {
                case 0: // 0 = Pause counter during Hblank(s)
                    enable();
                    break;
                case 1: // 1 = Reset counter to 0000h at Hblank(s)
                    break;
                case 2: // 2 = Reset counter to 0000h at Hblank(s) and pause outside of Hblank
                    write(0, 4);
                    disable();
                    break;
                case 3: // 3 = Pause until Hblank occurs once, then switch to Free Run
                    enable();
                    mode.sync_enable = 0;
                    break;
            }
        }
    }
}

u64 core::dotclock()
{
    return (u64)(static_cast<float>(clock.master_cycle_count) * clock.dot.ratio.cpu_to_dotclock);
}

u32 TIMER::read_clk(u64 clk) const
{
    u32 r = start.value;
    if (running) r += clk - start.cycle;
    assert(r<0x10000);
    return r;
}

u64 TIMER::get_clock_source()
{
    if (num == 1) printf("\nTimer %d on system clock: %d. master count:%lld", num, on_system_clock, bus->clock.master_cycle_count);
    if (on_system_clock) return bus->clock.master_cycle_count;
    switch(num) {
        case 0:
            return bus->dotclock();
        case 1:
            return bus->clock.hblank_clock;
        case 2:
            return bus->clock.master_cycle_count >> 3;
        default:
            NOGOHERE;
    }
    return 0;
}

u32 TIMER::read() {
    u32 v = read_clk(get_clock_source());
    if (num == 1) printf("\nREAD timer %d: %04x", num, v);
    return v;
}

void timer_irq_down(void *ptr, u64 key, u64 clock, u32 jitter)
{
    auto *th = static_cast<core *>(ptr);
    auto *t = &th->timers[key];
    t->mode.irq_request = 1;
    th->set_irq(static_cast<IRQ>(static_cast<u64>(IRQ_TMR0)+key), 0);
}

void core::update_timer_irqs()
{
    for (u32 i = 0; i < 3; i++) {
        auto &t = timers[i];
        u32 b = t.mode.irq_on_ffff && t.mode.reached_ffff;
        b |= t.mode.irq_on_target && t.mode.reached_target;
        //u32 old_request = t.mode.irq_request;
        t.mode.irq_request &= b ^ 1;
        u32 irq_num = IRQ_TMR0 + i;
        if ((IRQ_multiplexer.irqs[irq_num].input == 0) && b) {
            // Set up
            set_irq(static_cast<IRQ>(IRQ_TMR0 + i), b);
            printf("\nTIMER%d IRQ SET!", i);

            // Schedule down-set
            scheduler.only_add_abs(clock_current() + 100, i, this, &timer_irq_down, nullptr);
        }
        else
            set_irq(static_cast<IRQ>(IRQ_TMR0 + i), 0);
    }
}

u32 core::timers_read(u32 addr, u32 sz)
{
    u32 timer_num = (addr >> 4) & 3;
    printf("\nREAD FROM %08x: %d", addr, timer_num);
    switch(addr & 0x1FFFFFCF) {
        case 0x1F801100: // current counter value
            return timers[timer_num].read() & 0xFFFF;
        case 0x1F801104: { // timer0...2 mode
            u32 v = timers[timer_num].mode.u;
            timers[timer_num].mode.reached_target = 0;
            timers[timer_num].mode.reached_ffff = 0;
            update_timer_irqs();
            return v; }
        case 0x1F801108: // timer0...2 target value
            return timers[timer_num].target;
    }

    printf("\nUnhandled timers read %08x (%d)", addr, sz);
    return 0;
}

// Determine ticks until overflow
// Schedule if it's not at the same time
void TIMER::disable()
{
    if (!running) return;
    running = 0;
    start.value = read_clk(get_clock_source());
    if (overflow.still_sched) {
        bus->scheduler.delete_if_exist(overflow.sched_id);
    }
}

u64 core::time_til_next_hblank(u64 clk)
{

    // Determine which cycle of current line
    u64 line_cycle = (clk - clock.frame_cycle_start) % clock.timing.scanline.cycles;

    if (line_cycle >= clock.timing.scanline.hblank.start_on_cycle) { // We're in the hblank portion of a line,
        // Account for rest of scanline
        return (clock.timing.scanline.cycles - line_cycle) + clock.timing.scanline.hblank.start_on_cycle;
    }
    else { // We're not in the hblank portion of a line.
        return clock.timing.scanline.hblank.start_on_cycle - line_cycle;
    }
}

u64 core::calculate_timer1_hblank(u32 diff)
{
    // This routine answers one question:
    // How many CPU cycles until x hblanks have happened?

    u64 clk = clock_current();
    // First, determine time til next hblank
    u64 nd = time_til_next_hblank(clk);
    assert(diff!=0);
    diff--;
    nd += clock.timing.scanline.cycles * diff;
    return nd;
}

void timer_overflow(void *ptr, u64 timer_num, u64 current_clock, u32 jitter)
{
    u64 clk = current_clock - jitter;
    auto *th = static_cast<core *>(ptr);
    auto *t = &th->timers[timer_num];

    u32 value = t->overflow.at & 0xFFFF;

    if (value == 0xFFFF) {
        t->mode.reached_ffff = 1;
    }
    if (value == t->target) {
        t->mode.reached_target = 1;
    }
    th->update_timer_irqs();

    // Now determine if we reset...
    if ((value == 0xFFFF) || ((value == t->target) && t->mode.reset_when)) {
        t->start.value = 0;
    }
    if (t->on_system_clock) t->start.cycle = th->clock_current() + 2;
    else t->start.cycle = t->get_clock_source();

    // Schedule next overflow...
    t->reschedule();
}

void TIMER::reschedule()
{
    if (!running) {
        deschedule();
        return;
    }

    // TODO: calculate out complicated hblank/vblank stuff. it's doable.

    // Overflow is either on 0xFFFF or target, whichever is next...
    u32 overflow_at = 0xFFFF;
    u64 clk = get_clock_source();
    u32 current_value  = read_clk(clk) & 0xFFFF;

    if (current_value < target) overflow_at = target;
    overflow.at = overflow_at;


    if (current_value == overflow_at) overflow_at += target + 0x10000;

    u32 diff = overflow_at - current_value;

    // Now convert to our clock cycles...
    if (!on_system_clock) {
        switch(num) {
            case 0: // on the dot clock...
                diff = static_cast<u64>((float) diff * bus->clock.dot.ratio.cpu_to_dotclock);
                break;
            case 1: // on hblank...we shouldn't be here.
                diff = bus->calculate_timer1_hblank(diff);
                break;
            case 2: // diff is currently in >>=3-ness.
                diff <<= 3;
                break;
            default:
                NOGOHERE;
        }
    }

    u64 on_cycle = bus->clock_current() + diff;

    bool do_sched = true;
    if (overflow.still_sched) {
        if (overflow.sch_cycle != on_cycle) {
            deschedule();
        }
        else { // it's already scheduled for the proper time
            do_sched = false;
        }
    }
    if (do_sched) {
        overflow.sch_cycle = on_cycle;
        overflow.sched_id = bus->scheduler.only_add_abs(on_cycle, num, this, &timer_overflow, &overflow.still_sched);
    }
}

void TIMER::enable()
{
    if (running) return;

    running = 1;

    u64 clk = get_clock_source();
    if (on_system_clock) start.cycle = bus->clock_current() + 2;
    else start.cycle = clk;
    reschedule();
}

void TIMER::setup() {
    switch (num) {
        case 0:
            // Need to determine if this timer is on sysclk or not,
            on_system_clock = (mode.clock_source & 1) ^ 1;

            // If it is running or not currently,
            running = !(mode.sync_enable && mode.sync_mode == 3); // if so, we pause until hblank occurs...
            if (on_system_clock) start.cycle = bus->clock_current() + 2;
            else start.cycle = bus->dotclock();

            // Determine overflow timing
            reschedule();
            break;
        case 1:
            // Need to determine if this timer is on sysclk or not,
            on_system_clock = (mode.clock_source & 1) ^ 1;

            // If it is running or not currently,
            running = !(mode.sync_enable && mode.sync_mode == 3); // if so, we pause until vblank occurs...
            if (on_system_clock) start.cycle = bus->clock_current() + 2;
            else start.cycle = bus->clock.hblank_clock;

            // Determine overflow timing
            reschedule();
            break;
        case 2:
            // Need to determine if this timer is on sysclk or not,
            on_system_clock = mode.clock_source < 2;

            // If it is running or not currently,
            if (mode.sync_enable) {
                running = (mode.sync_mode == 1) || (mode.sync_mode == 2);
            }
            else running = 1;

            if (on_system_clock) start.cycle = bus->clock_current() + 2;
            else start.cycle = bus->clock_current() >> 3;

            // Determine overflow timing
            reschedule();
            break;
    }
}


void TIMER::reset(u32 reset_to)
{
    start.cycle = bus->clock_current() + 2;
    start.value = reset_to;
}

void TIMER::write(u32 val, u32 sz)
{
    start.value = val & 0xFFFF;
    reschedule();
}

void TIMER::write_target(u32 val, u32 sz)
{
    target = val & 0xFFFF;
    reschedule();
}

void TIMER::write_mode(u32 val, u32 sz)
{
    val &= 0b1111111111;
    u32 old_mode = mode.u;
    mode.u = (mode.u & 0b1111110000000000) | val;

    // Reset to 0
    start.value = 0;

    if (old_mode != mode.u)
        setup();
    else
        reschedule();

    mode.irq_request = 1;
    bus->update_timer_irqs();
}

void core::timers_write(u32 addr, u32 sz, u32 val)
{
    u32 timer_num = (addr >> 4) & 3;
    assert(timer_num < 3);
    auto &t = timers[timer_num];
    switch(addr & 0x1FFFFFCF) {
        case 0x1F801100: // current counter value
            t.write(val, sz);
            return;
        case 0x1F801104: // timer0...2 mode
            t.write_mode(val, sz);
            return;
        case 0x1F801108: // timer0...2 target value
            t.write_target(val, sz);
            return;

    }
    printf("\nUnhandled timers write %08x: %08x (%d)", addr, val, sz);
}
}
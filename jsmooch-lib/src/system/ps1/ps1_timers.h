//
// Created by . on 3/4/25.
//

#pragma once

#include "helpers/int.h"
namespace PS1 {
struct core;
struct TIMER {
    u32 read_clk(u64 clk) const;
    u64 get_clock_source();
    void setup();
    void reset(u32 reset_to);
    void enable();
    void disable();
    void reschedule();
    void reset();
    void vblank(u64 key);
    void hblank(u64 key);
    void deschedule();
    void write(u32 val, u8 sz);
    void write_target(u32 val, u8 sz);
    void write_mode(u32 val, u8 sz);
    u32 read();
    core *bus; // TODO: fill this in!
    u32 num; // TODO: fill this in!
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

}
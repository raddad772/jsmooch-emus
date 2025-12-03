//
// Created by . on 5/1/25.
//

#pragma once
#include "helpers/int.h"

namespace GBA {

struct core;
struct TIMER {
    explicit TIMER(core *parent, int num_in);
    int num{};
    struct {
        u32 io{};
        u32 mask{};
        u32 counter{};
    } divider{};

    core *gba;

    u32 shift{};

    u64 enable_at{0xFFFFFFFFFFFFFFFF};
    u64 overflow_at{0xFFFFFFFFFFFFFFFF}; // cycle # we'll overflow at
    u64 sch_id{};
    u32 sch_scheduled_still{};
    u32 cascade{};
    u16 val_at_stop{};
    u32 irq_on_overflow{};
    u16 reload{};
    u64 reload_ticks{};

    bool enabled() const;
    u32 read() const;
    void overflow(u64 current_time);
    void cascade_timer_step(u64 current_time);
    static void timer_overflow(void *ptr, u64 timer_num, u64 current_clock, u32 jitter);
    void write_cnt(u32 val, u64 clock, u32 jitter);
} ;


static inline u32 timer_reload_ticks(u32 reload)
{
    if (reload == 0xFFFF) return 0x10000;
    return 0x10000 - reload;
}

}

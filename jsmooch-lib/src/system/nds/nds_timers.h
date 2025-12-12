#pragma once
#include "helpers/int.h"
namespace NDS {


struct core;
struct timer7_t {
    explicit timer7_t(core *parent, u32 num_in) : bus(parent), num(num_in) {}
    bool enabled();
    u32 num;
    core *bus;

    static void overflow_callback(void *ptr, u64 timer_num, u64 current_clock, u32 jitter);
    void cascade_step(u64 current_time);
    void overflow(u64 current_time);
    u32 read();
    void write_cnt(u32 val);

    struct {
        u32 io{};
        u32 mask{};
        u32 counter{};
    } divider{};

    u32 shift{};

    u64 enable_at{}; // cycle # we'll be enabled at
    u64 overflow_at{}; // cycle # we'll overflow at
    u64 sch_id{};
    u32 sch_scheduled_still{};
    u32 cascade{};
    u16 val_at_stop{};
    u32 irq_on_overflow{};
    u16 reload{};
    u64 reload_ticks{};
};

struct timer9_t {
    explicit timer9_t(core *parent, u32 num_in) : bus(parent), num(num_in) {}
    bool enabled();
    u32 num;
    core *bus;

    static void overflow_callback(void *ptr, u64 timer_num, u64 current_clock, u32 jitter);
    void cascade_step(u64 current_time);
    void overflow(u64 current_time);
    u32 read();
    void write_cnt(u32 val);

    struct {
        u32 io{};
        u32 mask{};
        u32 counter{};
    } divider{};

    u32 shift{};

    u64 enable_at{}; // cycle # we'll be enabled at
    u64 overflow_at{}; // cycle # we'll overflow at
    u64 sch_id{};
    u32 sch_scheduled_still{};
    u32 cascade{};
    u16 val_at_stop{};
    u32 irq_on_overflow{};
    u16 reload{};
    u64 reload_ticks{};
};
}
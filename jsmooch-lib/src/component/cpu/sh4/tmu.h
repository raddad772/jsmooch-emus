//
// Created by RadDad772 on 3/8/24.
//

#pragma once
#include "helpers/int.h"
#include "helpers/scheduler.h"

// TMUs count down at 1/4 speed from the CPU
namespace SH4 {
    struct core;
    struct TMU {
        explicit TMU(core *parent) : cpu(parent) {}
    core *cpu;
    u32 TOCR{};
    u32 TSTR{};
    u32 TCOR[3]{};
    u32 TCNT[3]{};
    u32 TCR[3]{};
    u32 shift[3]{};
    u32 old_mode[3]{};
    u32 base[3]{};
    u64 base64[3]{};
    u32 mask[3]{};
    u64 mask64[3]{};
    i64 read_TCNT64(u32 ch) const;
    u32 read_TCNT(u32 ch, bool is_regread);
    void sched_chan_tick(u32 ch);
    void write_TCNT(u32 ch, u32 data);
    void turn_onoff(u32 ch, u32 on);
    void write_TSTR(u32 val);
    void update_counts(u32 ch);
    void reset();
    void write_TCR(u32 ch, u32 val);
    void write(u32 addr, u64 val, u8 sz, bool* success);
    u64 read(u32 addr, u8 sz, bool* success);
};
}
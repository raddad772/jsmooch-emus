#pragma once

#include "helpers/int.h"

namespace mac {

struct core;

struct RTC {
    explicit RTC(core *parent) : bus(parent) {}
    core *bus;

    u8 read_bits();
    void do_read();
    void finish_write();
    void write_bits(u8 val, u8 write_mask);

    u8 param_mem[20]{};
    u8 old_clock_bit{};
    u8 cmd{};
    u32 seconds{};

    u64 cycle_counter{};

    u8 test_register{};
    u8 write_protect_register{};
    struct {
        u8 shift{}, progress{}, kind{};
    } tx{};
};
}
//
// Created by . on 5/9/25.
//

#pragma once

#include "helpers/int.h"
#include "helpers/cvec.h"

struct SNES_joypad_inputs {
    u32 a, b, x, y, l, r, start, select, up, down, left, right;
};

struct physical_io_device;
struct SNES_joypad {
    SNES_joypad_inputs input_buffer{};
    cvec_ptr<physical_io_device> pio_ptr{};

    u32 counter{}, latched{};

    void latch(u32 val);
    u32 data();
    static void setup_pio(physical_io_device &d, u32 num, const char*name, u32 connected);

};

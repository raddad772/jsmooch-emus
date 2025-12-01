//
// Created by . on 11/18/24.
//

#pragma once

#include "helpers/int.h"
#include "helpers/physical_io.h"


namespace genesis {
struct c6button_inputs {
    u32 a, b, c, x, y, z, start, mode, up, down, left, right;
};

struct c6button {
    physical_io_device *pio;
    u32 select;

    u64 timeout_at, counter;
    u64 *master_clock;
    c6button_inputs input_buffer;

    void latch();
    void setup_pio(physical_io_device *d, u32 num, const char*name, u32 connected);
    u8 read_data();
    void write_data(u8 data);
};

}
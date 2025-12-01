//
// Created by . on 6/1/24.
//

#pragma once

#include "helpers/int.h"
#include "helpers/physical_io.h"

namespace genesis {

struct c3button_inputs {
    u32 a, b, c, start, up, down, left, right;
};


struct c3button {
    physical_io_device *pio;
    u32 select;
    c3button_inputs input_buffer;

    void latch();
    void setup_pio(physical_io_device *d, u32 num, const char*name, u32 connected);
    u8 read_data() const;
    void write_data(u8 val);
};

}
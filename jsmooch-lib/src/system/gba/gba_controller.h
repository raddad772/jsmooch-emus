//
// Created by . on 12/4/24.
//

#pragma once

#include "helpers/int.h"
#include "helpers/physical_io.h"

namespace GBA {
struct controller_inputs {
    u32 a, b, l, r, start, select, up, down, left, right;
};


struct controller {
    physical_io_device *pio{};
    controller_inputs input_buffer{};
    void setup_pio(physical_io_device *d);
    u32 get_state();
};

}
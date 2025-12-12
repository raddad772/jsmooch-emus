//
// Created by . on 12/4/24.
//

#pragma once

#include "helpers/int.h"
#include "helpers/physical_io.h"

namespace NDS {

struct controller_inputs {
    u32 a{}, b{}, l{}, r{}, start{}, select{}, up{}, down{}, left{}, right{};
};


struct controller {
    explicit controller(core *parent) : bus(parent) {}
    void setup_pio(physical_io_device *d);
    core *bus;
    u32 get_state(u32 byte);
    physical_io_device *pio{};
    controller_inputs input_buffer{};
};


}


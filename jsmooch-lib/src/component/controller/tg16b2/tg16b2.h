//
// Created by . on 7/13/25.
//

#pragma once

#include "helpers/int.h"
#include "helpers/physical_io.h"
namespace TG16 {

struct controller_2button {
    physical_io_device *pio{};
    u32 sel{}, clr{};

    u8 read_data();
    void write_data(u8 val);

    void setup_pio(physical_io_device &d, u32 num, const char*name, bool connected);
};

}

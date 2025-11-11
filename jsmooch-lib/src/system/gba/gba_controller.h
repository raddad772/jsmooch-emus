//
// Created by . on 12/4/24.
//

#ifndef JSMOOCH_EMUS_GBA_CONTROLLER_H
#define JSMOOCH_EMUS_GBA_CONTROLLER_H

#include "helpers/int.h"
#include "helpers/physical_io.h"

struct GBA_controller_inputs {
    u32 a, b, l, r, start, select, up, down, left, right;
};


struct GBA_controller {
    struct physical_io_device *pio;
    struct GBA_controller_inputs input_buffer;
};

void GBA_controller_setup_pio(physical_io_device *d);
u32 GBA_get_controller_state(physical_io_device *d);

#endif //JSMOOCH_EMUS_GBA_CONTROLLER_H

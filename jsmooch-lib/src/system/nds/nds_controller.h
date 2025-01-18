//
// Created by . on 12/4/24.
//

#ifndef JSMOOCH_EMUS_NDS_CONTROLLER_H
#define JSMOOCH_EMUS_NDS_CONTROLLER_H

#include "helpers/int.h"
#include "helpers/physical_io.h"

struct NDS_controller_inputs {
    u32 a, b, l, r, start, select, up, down, left, right;
};


struct NDS_controller {
    struct physical_io_device *pio;
    struct NDS_controller_inputs input_buffer;
};

void NDS_controller_setup_pio(struct physical_io_device *d);
u32 NDS_get_controller_state(struct physical_io_device *d);

#endif //JSMOOCH_EMUS_NDS_CONTROLLER_H

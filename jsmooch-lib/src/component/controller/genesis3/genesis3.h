//
// Created by . on 6/1/24.
//

#ifndef JSMOOCH_EMUS_GENESIS3_H
#define JSMOOCH_EMUS_GENESIS3_H

#include "helpers/int.h"
#include "helpers/physical_io.h"

struct genesis_controller_3button {
    struct physical_io_device *pio;
};

void genesis_controller_3button_init(struct genesis_controller_3button* this);
void genesis_controller_3button_delete(struct genesis_controller_3button* this);

void genesis3_setup_pio(struct physical_io_device *d, u32 num, const char*name, u32 connected);

#endif //JSMOOCH_EMUS_GENESIS3_H

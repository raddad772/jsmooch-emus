//
// Created by . on 11/18/24.
//

#ifndef JSMOOCH_EMUS_GENESIS6_H
#define JSMOOCH_EMUS_GENESIS6_H

#include "helpers_c/int.h"
#include "helpers_c/physical_io.h"

struct genesis_controller_6button_inputs {
    u32 a, b, c, x, y, z, start, mode, up, down, left, right;
};


struct genesis_controller_6button {
    struct physical_io_device *pio;
    u32 select;

    u64 timeout_at, counter;
    u64 *master_clock;
    struct genesis_controller_6button_inputs input_buffer;
};

void genesis_controller_6button_latch(struct genesis_controller_6button*);

void genesis6_setup_pio(struct physical_io_device *d, u32 num, const char*name, u32 connected);

u8 genesis6_read_data(struct genesis_controller_6button *);
void genesis6_write_data(struct genesis_controller_6button *, u8 val);

void genesis_controller_6button_init(struct genesis_controller_6button* this, u64 *master_clock);
void genesis_controller_6button_delete(struct genesis_controller_6button* this);


#endif //JSMOOCH_EMUS_GENESIS6_H

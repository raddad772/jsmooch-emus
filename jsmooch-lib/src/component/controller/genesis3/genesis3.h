//
// Created by . on 6/1/24.
//

#ifndef JSMOOCH_EMUS_GENESIS3_H
#define JSMOOCH_EMUS_GENESIS3_H

#include "helpers_c/int.h"
#include "helpers_c/physical_io.h"

struct genesis_controller_3button_inputs {
    u32 a, b, c, start, up, down, left, right;
};


struct genesis_controller_3button {
    struct physical_io_device *pio;
    u32 select;
    struct genesis_controller_3button_inputs input_buffer;
};

void genesis_controller_3button_init(struct genesis_controller_3button*);
void genesis_controller_3button_delete(struct genesis_controller_3button*);
void genesis_controller_3button_latch(struct genesis_controller_3button*);

void genesis3_setup_pio(struct physical_io_device *d, u32 num, const char*name, u32 connected);

u8 genesis3_read_data(struct genesis_controller_3button *);
void genesis3_write_data(struct genesis_controller_3button *, u8 val);

#endif //JSMOOCH_EMUS_GENESIS3_H

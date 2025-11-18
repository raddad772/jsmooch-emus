//
// Created by Dave on 2/5/2024.
//

#ifndef JSMOOCH_EMUS_NES_JOYPAD_H
#define JSMOOCH_EMUS_NES_JOYPAD_H

#include "helpers/int.h"
#include "helpers/physical_io.h"

struct nespad_inputs {
    u32 a, b, start, select, up, down, left, right;
};

void nespad_inputs_init(struct nespad_inputs*);

struct NES_joypad {
    u32 counter, latched, joynum;
    struct nespad_inputs input_buffer;
    /*struct nespad_inputs input_waiting;*/
    struct cvec* devices;
    u32 device_index;
};

void NES_joypad_init(struct NES_joypad*, u32 joynum);
void NES_joypad_latch(struct NES_joypad *, u32 what);
u32 NES_joypad_data(struct NES_joypad*);
void NES_joypad_setup_pio(struct physical_io_device *d, u32 num, const char*name, u32 connected);


#endif //JSMOOCH_EMUS_NES_JOYPAD_H

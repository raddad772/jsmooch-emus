//
// Created by Dave on 2/5/2024.
//

#ifndef JSMOOCH_EMUS_NES_JOYPAD_H
#define JSMOOCH_EMUS_NES_JOYPAD_H

#include "helpers/int.h"

struct nespad_inputs {
    u32 a, b, start, select, up, down, left, right;
};

void nespad_inputs_init(struct nespad_inputs* this);

struct NES_joypad {
    u32 counter, latched, joynum;
    struct nespad_inputs input_buffer;
    struct nespad_inputs input_waiting;
};

void NES_joypad_init(struct NES_joypad* this, u32 joynum);
void NES_joypad_buffer_input(struct NES_joypad* this, struct nespad_inputs *from);
void NES_joypad_latch(struct NES_joypad *this, u32 what);
u32 NES_joypad_data(struct NES_joypad* this);

#endif //JSMOOCH_EMUS_NES_JOYPAD_H

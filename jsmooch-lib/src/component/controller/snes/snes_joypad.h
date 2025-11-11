//
// Created by . on 5/9/25.
//

#ifndef JSMOOCH_EMUS_SNES_JOYPAD_H
#define JSMOOCH_EMUS_SNES_JOYPAD_H

#include "helpers/int.h"


struct SNES_joypad_inputs {
    u32 a, b, x, y, l, r, start, select, up, down, left, right;
};

struct physical_io_device;
struct SNES_joypad {
    struct SNES_joypad_inputs input_buffer;
    struct physical_io_device *pio;

    u32 counter, latched;
};

void SNES_joypad_latch(SNES_joypad*, u32 val);
u32 SNES_joypad_data(SNES_joypad*);
void SNES_joypad_setup_pio(physical_io_device *d, u32 num, const char*name, u32 connected);
void SNES_joypad_init(SNES_joypad* this);
void SNES_joypad_delete(SNES_joypad* this);

#endif //JSMOOCH_EMUS_SNES_JOYPAD_H

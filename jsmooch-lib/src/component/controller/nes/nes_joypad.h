//
// Created by Dave on 2/5/2024.
//

#ifndef JSMOOCH_EMUS_NES_JOYPAD_H
#define JSMOOCH_EMUS_NES_JOYPAD_H

#include "helpers/int.h"
#include "helpers/physical_io.h"

struct nespad_inputs {
    u32 a{}, b{}, start{}, select{}, up{}, down{}, left{}, right{};
};

void nespad_inputs_init(struct nespad_inputs*);

struct NES_joypad {
    u32 counter{}, latched{}, joynum{};
    nespad_inputs input_buffer{};
    std::vector<physical_io_device> devices{};
    u32 device_index{};

    explicit NES_joypad(u32 joynum);
    void latch(u32 what);
    u32 data();
    static void setup_pio(struct physical_io_device *d, u32 num, const char*name, u32 connected);
};

#endif //JSMOOCH_EMUS_NES_JOYPAD_H

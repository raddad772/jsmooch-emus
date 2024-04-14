//
// Created by Dave on 2/4/2024.
//

#ifndef JSMOOCH_EMUS_NES_H
#define JSMOOCH_EMUS_NES_H

#include "nes_clock.h"
#include "nes_bus.h"
#include "mappers/mapper.h"
#include "nes_cart.h"
#include "nes_cpu.h"
#include "nes_ppu.h"

void NES_new(struct jsm_system* system, struct JSM_IOmap *iomap);
void NES_delete(struct jsm_system* system);

enum NES_TIMINGS {
    NES_NTSC,
    NES_PAL,
    NES_DENDY
};

struct NES {
    struct NES_clock clock;
    //struct NES_bus bus;
    struct r2A03 cpu;
    struct NES_PPU ppu;

    u32 cycles_left;
    u32 display_enabled;

    struct nespad_inputs controller1_in;
    struct nespad_inputs controller2_in;
    struct NES_mapper bus;
    struct NES_cart cart;
};

#endif //JSMOOCH_EMUS_NES_H

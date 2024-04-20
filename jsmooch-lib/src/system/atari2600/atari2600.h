//
// Created by Dave on 4/14/24.
//

#ifndef JSMOOCH_EMUS_ATARI2600_H
#define JSMOOCH_EMUS_ATARI2600_H

#include "component/cpu/m6502/m6502.h"
#include "component/misc/m6532.h"
#include "cart.h"
#include "tia.h"

void atari2600_new(struct jsm_system* system, struct JSM_IOmap *iomap);
void atari2600_delete(struct jsm_system* system);

struct atari2600_inputs {
    u32 fire, up, down, left, right;
};

struct atari2600_CPU_bus {
    union {
        struct {
            u32 _0_6: 7;
            u32 a7: 1;
            u32 _8: 1;
            u32 a9: 1;
            u32 _2: 2;
            u32 a12: 1;
        };
        u32 u;
    } Addr;
    u32 D;
    u32 RW;
};

struct atari2600 {
    //struct atari2600_clock clock;
    //struct atari2600_bus bus;
    struct M6502 cpu;
    struct M6532 riot;
    struct atari_TIA tia;

    struct atari2600_CPU_bus CPU_bus;

    i32 cycles_left;
    u32 display_enabled;
    u64 master_clock;

    struct atari2600_inputs controller1_in;
    struct atari2600_inputs controller2_in;
    struct atari2600_cart cart;

    struct {
        u32 reset;
        u32 select;
        u32 color;
        u32 p0_difficulty;
        u32 p1_difficulty;
    } case_switches;
};


#endif //JSMOOCH_EMUS_ATARI2600_H

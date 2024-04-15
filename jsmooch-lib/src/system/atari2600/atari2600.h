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
            u32 _1: 6;
            u32 a7: 1;
            u32 _2: 1;
            u32 a9: 1;
            u32 _3: 2;
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

    struct {
        u64 master_clock;
        u64 master_frame;

        u32 frames_since_restart;

        u32 line_cycle;
        u32 sy;

        struct {
            u32 cpu_divisor; // 3
            u32 tia_divisor; // 1

            u32 vblank_in_lines;
            u32 display_line_start;
            u32 vblank_out_start;
            u32 vblank_out_lines;
        } timing;
    } clock;

    struct atari2600_CPU_bus CPU_bus;

    i32 cycles_left;
    u32 display_enabled;

    u32 mem_map[0x2000];

    struct atari2600_inputs controller1_in;
    struct atari2600_inputs controller2_in;
    struct atari2600_cart cart;
};


#endif //JSMOOCH_EMUS_ATARI2600_H

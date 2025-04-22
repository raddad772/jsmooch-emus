//
// Created by . on 4/19/25.
//

#ifndef JSMOOCH_EMUS_SNES_CLOCK_H
#define JSMOOCH_EMUS_SNES_CLOCK_H

#include <helpers/int.h>

struct SNES_clock {
    u64 master_cycle_count;
    u64 master_frame;


    struct {
        i32 divider;
        i32 has;
    } cpu;

    struct {
        i32 divider;
        i32 has;
        u32 h, v;
    } ppu;

    struct {
        i32 divider;
        long double has;
        long double ratio;

        long double next_sample;
        long double sample_stride;
    } apu;

    struct {
        u32 master_hz;
        u32 apu_master_hz;
        struct {
            u32 master_cycles;
        } frame;
        struct {
            u32 master_cycles;
        } line;
        struct {
            u32 master_cycles;
        } second;

    } timing;

};


void SNES_clock_init(struct SNES_clock *);

#endif //JSMOOCH_EMUS_SNES_CLOCK_H

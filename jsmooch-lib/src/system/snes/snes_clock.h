//
// Created by . on 4/19/25.
//

#ifndef JSMOOCH_EMUS_SNES_CLOCK_H
#define JSMOOCH_EMUS_SNES_CLOCK_H

#include <helpers/int.h>

struct SNES_clock {
    struct {
        i32 divider;
        i32 has;
    } cpu;

    struct {
        i32 divider;
        i32 has;
    } ppu;

    struct {
        i32 divider;
        long double has;
        long double ratio;
    } apu;

    struct {
        u32 master_hz;
        u32 apu_master_hz;
    } timing;
};


void SNES_clock_init(struct SNES_clock *);

#endif //JSMOOCH_EMUS_SNES_CLOCK_H

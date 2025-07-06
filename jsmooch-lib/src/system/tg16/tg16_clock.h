//
// Created by . on 6/18/25.
//

#ifndef JSMOOCH_EMUS_TG16_CLOCK_H
#define JSMOOCH_EMUS_TG16_CLOCK_H

#include "helpers/int.h"

struct TG16_clock {
    u64 master_cycles;
    u64 unused;
    u64 master_frames;

    struct {
        u64 scanline_start;
    } vdc;

    struct {
        u64 timer;
    } dividers;

    struct {
        u64 cpu, vce, timer;
    } next;

    struct {
        struct {
            u64 cycles;
        } scanline;

        struct {
            u64 cycles;
            u32 lines;
        } frame;

        struct {
            u64 cycles;
            u32 frames;
        } second;
    } timing;
};

void TG16_clock_init(struct TG16_clock *);
void TG16_clock_reset(struct TG16_clock *);

#endif //JSMOOCH_EMUS_TG16_CLOCK_H

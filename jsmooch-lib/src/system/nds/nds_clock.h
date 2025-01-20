//
// Created by . on 12/4/24.
//

#ifndef JSMOOCH_EMUS_NDS_CLOCK_H
#define JSMOOCH_EMUS_NDS_CLOCK_H

#include "helpers/int.h"

struct NDS_clock {
    u64 master_cycle_count7;
    u64 master_cycle_count9;
    u64 master_frame;

    i64 cycles7;
    i64 cycles9;

    struct {
        u32 x, y;
        u64 scanline_start;
        u32 hblank_active, vblank_active;
    } ppu;
};


void NDS_clock_init(struct NDS_clock *);
void NDS_clock_reset(struct NDS_clock *);

#endif //JSMOOCH_EMUS_NDS_CLOCK_H

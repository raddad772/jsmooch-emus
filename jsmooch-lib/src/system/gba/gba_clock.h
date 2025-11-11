//
// Created by . on 12/4/24.
//

#ifndef JSMOOCH_EMUS_GBA_CLOCK_H
#define JSMOOCH_EMUS_GBA_CLOCK_H

#include "helpers/int.h"

struct GBA_clock {
    u64 master_cycle_count;
    u64 master_frame;

    struct {
        u32 x, y;
        u64 scanline_start;
        u32 hblank_active, vblank_active;
    } ppu;
};


void GBA_clock_init(GBA_clock *);
void GBA_clock_reset(GBA_clock *);

#endif //JSMOOCH_EMUS_GBA_CLOCK_H

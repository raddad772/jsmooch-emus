//
// Created by . on 12/4/24.
//

#ifndef JSMOOCH_EMUS_NDS_CLOCK_H
#define JSMOOCH_EMUS_NDS_CLOCK_H

#include "helpers/int.h"

struct NDS_clock {
    u64 frame_start_cycle;
    i64 cycles_left_this_frame;
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

    struct {

        struct {
            u32 hz;
        } apu;
        struct {
            u64 hz;
            u64 hz_2;
        } arm7;

        struct {
            u64 cycles;
            float per_second;

            u64 vblank_down_on;
            u64 vblank_up_on;
        } frame;

        struct {
            u64 cycles_total;
            u64 cycle_of_hblank;
            u64 number;
        } scanline;
    } timing;


};


void NDS_clock_init(NDS_clock *);
void NDS_clock_reset(NDS_clock *);

#endif //JSMOOCH_EMUS_NDS_CLOCK_H

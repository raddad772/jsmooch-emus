//
// Created by . on 6/18/25.
//

#include <cstdio>
#include <cstring>
#include "tg16_clock.h"
#include "component/gpu/huc6260/huc6260.h"


namespace TG16 {
/*
The (NTSC) PC Engine is clocked with a master clock equal to six times the NTSC color burst (315/88 MHz), or approximately 21.47727 MHz.
This is divided into the following clocks:
CPU "high" clock speed = master clock / 3 (approx. 7.15909 MHz),
CPU "low" clock speed = master clock / 12 (approx. 1.78978 MHz),
Timer speed = master clock / 3072 (approx. 6.991 KHz),
VCE pixel clocks:
"10MHz" = master clock / 2,
"7MHz" = master clock / 3,
"5MHz" = master clock / 4.
 */

clock::clock()
{
    reset();
}

void clock::reset()
{
    timing.second.frames = 60;
    timing.frame.lines = 262;

    //u64 per_frame = master_freq / timing.second.frames;
    u64 per_line = HUC6260::CYCLE_PER_LINE; // = 1364

    u64 per_frame = per_line * timing.frame.lines;
    u64 master_freq = per_frame * timing.second.frames;
    printf("\nMASTER FREQ %lld", master_freq);

    timing.scanline.cycles = per_line;
    timing.frame.cycles = per_frame;
    timing.second.cycles = master_freq;
    printf("\nMASTER FREQ %lld", master_freq);

    next.cpu = 3;
    next.vce = 4;
    next.timer = 3072;
}

}
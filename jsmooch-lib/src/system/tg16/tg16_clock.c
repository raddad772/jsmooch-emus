//
// Created by . on 6/18/25.
//

#include <stdio.h>
#include <string.h>
#include "tg16_clock.h"
#include "component/gpu/huc6260/huc6260.h"

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

void TG16_clock_init(struct TG16_clock *this)
{
    memset(this, 0, sizeof(*this));
    TG16_clock_reset(this);
}

void TG16_clock_reset(struct TG16_clock *this)
{
    this->timing.second.frames = 60;
    this->timing.frame.lines = 262;

    //u64 per_frame = master_freq / this->timing.second.frames;
    u64 per_line = HUC6260_CYCLE_PER_LINE; // = 1364

    u64 per_frame = per_line * this->timing.frame.lines;
    u64 master_freq = per_frame * this->timing.second.frames;
    printf("\nMASTER FREQ %lld", master_freq);

    this->timing.scanline.cycles = per_line;
    this->timing.frame.cycles = per_frame;
    this->timing.second.cycles = master_freq;
    printf("\nMASTER FREQ %lld", master_freq);

    this->next.cpu = 3;
    this->next.vce = 4;
    this->next.timer = 3072;
}
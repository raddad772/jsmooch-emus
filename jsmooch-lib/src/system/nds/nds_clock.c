//
// Created by . on 12/4/24.
//

#include "nds_clock.h"

void NDS_clock_init(struct NDS_clock *this)
{
    this->ppu.scanline_start = 0;
    this->master_cycle_count7 = 0;
    this->master_cycle_count9 = 0;

    this->timing.arm7.hz = 33513982;//33000000;
    this->timing.frame.per_second = 60;
    this->timing.frame.cycles = (u64)((float)this->timing.arm7.hz / this->timing.frame.per_second);
    this->timing.scanline.number = 263;
    this->timing.scanline.cycles_total = this->timing.frame.cycles / this->timing.scanline.number;
    this->timing.scanline.cycle_of_hblank = this->timing.scanline.cycles_total - 594;

    this->timing.frame.vblank_up_on = 192;
    this->timing.frame.vblank_down_on = 262;

    // 1007.0800780078125 cycles per sample
    this->timing.apu.hz = 32768;
}

void NDS_clock_reset(struct NDS_clock *this)
{
}

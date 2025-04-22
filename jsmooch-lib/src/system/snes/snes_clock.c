//
// Created by . on 4/19/25.
//
#include <string.h>

#include "snes_clock.h"

static void fill_timing_ntsc(struct SNES_clock *this)
{
    this->timing.master_hz = 21477270;
    this->timing.apu_master_hz = 24576000;
    this->apu.ratio = 24576000.0 / 21477270.0;

    this->timing.line.master_cycles = 1364;
    this->timing.frame.master_cycles = this->timing.line.master_cycles * 262;
    this->timing.second.master_cycles = this->timing.frame.master_cycles * 60;

    this->apu.sample_stride = (long double)this->timing.second.master_cycles / 32000.0;
}

void SNES_clock_init(struct SNES_clock *this)
{
    memset(this, 0, sizeof(*this));

    fill_timing_ntsc(this);

}
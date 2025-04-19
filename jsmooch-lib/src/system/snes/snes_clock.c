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
}

void SNES_clock_init(struct SNES_clock *this)
{
    memset(this, 0, sizeof(*this));

    fill_timing_ntsc(this);
}
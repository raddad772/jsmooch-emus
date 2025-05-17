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

    this->apu.sample.stride = (long double)this->timing.second.master_cycles / 32000.0;
    this->apu.cycle.stride = (long double)this->timing.second.master_cycles / 1024000.0;
    //this->apu.sample.pitch_ratio =  (long double)this->timing.second.master_cycles / 131072000.0;
    this->apu.sample.pitch_ratio =  (long double)this->timing.second.master_cycles / 13107200.0;

    this->timing.line.hdma_setup_position = this->rev == 1 ? 12 + 8 : 12;
    this->timing.line.dram_refresh = this->rev == 1 ? 530 : 538;
    this->timing.line.hdma_position = 1104;
    this->timing.line.hblank_start = 277 * 4;
    this->timing.line.hblank_stop = 21 * 4;

    this->cpu.divider = 6;
}

void SNES_clock_init(struct SNES_clock *this)
{
    memset(this, 0, sizeof(*this));
    this->rev = 0;

    fill_timing_ntsc(this);

}

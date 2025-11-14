//
// Created by . on 6/1/24.
//

#include <cstdlib>
#include "genesis_clock.h"

void genesis_clock_ntsc(genesis_clock* this);
void genesis_clock_pal(genesis_clock* this);

void genesis_clock_init(genesis_clock* this, enum jsm::systems kind)
{
    *this = (genesis_clock) {};
    this->master_cycle_count = 0;
    genesis_clock_reset(this);
    this->kind = kind;
    switch(kind) {
        case jsm::systems::GENESIS_USA:
        case jsm::systems::GENESIS_JAP:
            genesis_clock_ntsc(this);
            break;
        case jsm::systems::MEGADRIVE_PAL:
            genesis_clock_pal(this);
            break;
        default:
            abort();
    }
    this->timing.frame.cycles_per = this->timing.scanline.cycles_per * this->timing.frame.scanlines_per;
    this->timing.second.cycles_per = this->timing.frame.cycles_per * this->timing.second.frames_per;

}

void genesis_clock_reset(genesis_clock* this)
{
    this->master_frame = 0;
    this->vdp.hcount = this->vdp.vcount = 0;
}


/*
System master clock rate: 53.693175 MHz (NTSC), 53.203424 MHz (PAL)[1]
Master clock cycles per frame: 896,040 (NTSC), 1,067,040 (PAL)
Master clock cycles per scanline: 3420[2] */
void genesis_clock_ntsc(genesis_clock* this)
{
    this->timing.scanline.cycles_per = 3420;
    this->timing.frame.scanlines_per = 262;
    this->timing.second.frames_per = 60;

    this->vdp.bottom_rendered_line = 223; // or 240
    this->vdp.bottom_max_rendered_line = 223;
    this->vdp.vblank_on_line = 224;

    this->vdp.clock_divisor = 16; // ?
    this->psg.clock_divisor = 240; // 48 of SMS/GG * 5
    this->ym2612.clock_divisor = 1008;
}

void genesis_clock_pal(genesis_clock* this)
{
    this->timing.scanline.cycles_per = 3420;
    this->timing.frame.scanlines_per = 312;
    this->timing.second.frames_per = 50;

    this->vdp.bottom_rendered_line = 223; // or 240
    this->vdp.bottom_max_rendered_line = 239;
    //this->vdp.vblank_on_line =


    this->vdp.clock_divisor = 16; // ?
    this->psg.clock_divisor = 240; // 48 of SMS/GG * 5
    this->ym2612.clock_divisor = 1008;
}


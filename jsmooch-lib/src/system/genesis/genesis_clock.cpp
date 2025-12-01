//
// Created by . on 6/1/24.
//

#include <cassert>
#include <cstdlib>
#include "genesis_clock.h"

namespace genesis {
clock::clock(jsm::systems in_kind) : kind(in_kind) {
    reset();
    switch(in_kind) {
        {
            case jsm::systems::GENESIS_JAP:
            case jsm::systems::GENESIS_USA:
                ntsc();
                break;
            case jsm::systems::MEGADRIVE_PAL:
                pal();
                break;
            default:
                assert(1==2);
        }}
    timing.frame.cycles_per = timing.scanline.cycles_per * timing.frame.scanlines_per;
    timing.second.cycles_per = timing.frame.cycles_per * timing.second.frames_per;
}

void clock::reset()
{
    master_frame = 0;
    vdp.hcount = vdp.vcount = 0;
}


/*
System master clock rate: 53.693175 MHz (NTSC), 53.203424 MHz (PAL)[1]
Master clock cycles per frame: 896,040 (NTSC), 1,067,040 (PAL)
Master clock cycles per scanline: 3420[2] */
void clock::ntsc()
{
    timing.scanline.cycles_per = 3420;
    timing.frame.scanlines_per = 262;
    timing.second.frames_per = 60;

    vdp.bottom_rendered_line = 223; // or 240
    vdp.bottom_max_rendered_line = 223;
    vdp.vblank_on_line = 224;

    vdp.clock_divisor = 16; // ?
    psg.clock_divisor = 240; // 48 of SMS/GG * 5
    ym2612.clock_divisor = 1008;
}

void clock::pal()
{
    timing.scanline.cycles_per = 3420;
    timing.frame.scanlines_per = 312;
    timing.second.frames_per = 50;

    vdp.bottom_rendered_line = 223; // or 240
    vdp.bottom_max_rendered_line = 239;
    //vdp.vblank_on_line =


    vdp.clock_divisor = 16; // ?
    psg.clock_divisor = 240; // 48 of SMS/GG * 5
    ym2612.clock_divisor = 1008;
}

}
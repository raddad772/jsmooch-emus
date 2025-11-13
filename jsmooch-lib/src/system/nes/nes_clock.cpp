//
// Created by Dave on 2/5/2024.
//

#include "nes_clock.h"

void NES_clock::reset()
{
    nmi = 0;
    master_clock = 0;
    master_frame = 0;
    cpu_master_clock = 0;
    ppu_master_clock = 0;
    //sound_master_clock = 0;
    //vblank = 0;
    ppu_y = 0;
    frame_odd = 0;
    frames_since_restart = 0;

    master_clock = master_frame = 0;

    frames_since_restart = 0;

    cpu_master_clock = apu_master_clock = ppu_master_clock = 0;
    trace_cycles = 0;

}
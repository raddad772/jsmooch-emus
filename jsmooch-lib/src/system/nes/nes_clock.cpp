//
// Created by Dave on 2/5/2024.
//

#include "nes_clock.h"

void NES_clock::reset()
{
    this->nmi = 0;
    this->master_clock = 0;
    this->master_frame = 0;
    this->cpu_master_clock = 0;
    this->ppu_master_clock = 0;
    //this->sound_master_clock = 0;
    //this->vblank = 0;
    this->ppu_y = 0;
    this->frame_odd = 0;
    this->frames_since_restart = 0;
}
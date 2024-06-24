//
// Created by . on 6/1/24.
//

#include "genesis_clock.h"

void genesis_clock_ntsc(struct genesis_clock* this);

void genesis_clock_init(struct genesis_clock* this)
{
    *this = (struct genesis_clock) {};
    this->master_cycle_count = 0;
    genesis_clock_reset(this);
    genesis_clock_ntsc(this);
}

void genesis_clock_reset(struct genesis_clock* this)
{
    this->master_frame = 0;
    this->vdp.sx = this->vdp.sy = 0;
}

void genesis_clock_ntsc(struct genesis_clock* this)
{
    this->m68k.clock_divisor = 7;
    this->z80.clock_divisor = 15;
    this->vdp.clock_divisor = 4; // ?

    this->m68k.cycles_til_clock = this->m68k.clock_divisor;
    this->z80.cycles_til_clock = this->z80.clock_divisor;
    this->vdp.cycles_til_clock = this->vdp.clock_divisor;
}


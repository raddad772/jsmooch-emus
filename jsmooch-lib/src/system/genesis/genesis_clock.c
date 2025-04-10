//
// Created by . on 6/1/24.
//

#include <stdlib.h>
#include "genesis_clock.h"

void genesis_clock_ntsc(struct genesis_clock* this);
void genesis_clock_pal(struct genesis_clock* this);

void genesis_clock_init(struct genesis_clock* this, enum jsm_systems kind)
{
    *this = (struct genesis_clock) {};
    this->master_cycle_count = 0;
    genesis_clock_reset(this);
    this->kind = kind;
    switch(kind) {
        case SYS_GENESIS_USA:
        case SYS_GENESIS_JAP:
            genesis_clock_ntsc(this);
            break;
        case SYS_MEGADRIVE_PAL:
            genesis_clock_pal(this);
            break;
        default:
            abort();
    }
}

void genesis_clock_reset(struct genesis_clock* this)
{
    this->master_frame = 0;
    this->vdp.hcount = this->vdp.vcount = 0;
}

void genesis_clock_pal(struct genesis_clock* this)
{
    abort();
}

void genesis_clock_ntsc(struct genesis_clock* this)
{
    this->m68k.clock_divisor = 7; // ym2612 does a furhter /24 off this
    this->z80.clock_divisor = 15;
    this->vdp.clock_divisor = 16; // ?
    this->psg.clock_divisor = 240; // 48 of SMS/GG * 5
    this->ym2612.clock_divisor = 1008;

    this->m68k.cycles_til_clock = this->m68k.clock_divisor;
    this->z80.cycles_til_clock = this->z80.clock_divisor;
    this->vdp.cycles_til_clock = this->vdp.clock_divisor;
    this->psg.cycles_til_clock = this->psg.clock_divisor;
}


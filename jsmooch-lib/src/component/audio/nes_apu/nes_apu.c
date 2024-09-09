//
// Created by . on 9/8/24.
//

#include <string.h>
#include <stdio.h>

#include "nes_apu.h"

void NES_APU_init(struct NES_APU* this)
{
    memset(this, 0, sizeof(struct NES_APU));
    for (u32 i = 0; i < 4; i++) {
        this->channels[i].ext_enable = 1;
        this->channels[i].number = i;
    }
    this->ext_enable = 1;

}

void NES_APU_reset(struct NES_APU* this)
{
    //this->channels[0].
}


void NES_APU_write_IO(struct NES_APU* this, u32 addr, u8 val)
{

}


u8 NES_APU_read_IO(struct NES_APU* this, u32 addr, u8 old_val, u32 has_effect)
{
    return old_val;
}

void NES_APU_cycle(struct NES_APU* this)
{
    this->clocks.counter_1 ^= 1;
    if (this->clocks.counter_1) {
        this->clocks.counter_240hz--;
        if (this->clocks.counter_240hz <= 0) {

        }
    }
}

float NES_APU_mix_sample(struct NES_APU* this, u32 is_debug)
{
    return 0.0f;
}

float NES_APU_sample_channel(struct NES_APU* this, int cnum)
{
    return 0.0f;
}

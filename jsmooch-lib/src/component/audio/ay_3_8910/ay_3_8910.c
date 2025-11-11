//
// Created by . on 8/4/24.
//

#include "ay_3_8910.h"

#define REG_AFINE    0x00
#define REG_ACOARSE  0x01
#define REG_BFINE    0x02
#define REG_BCOARSE  0x03
#define REG_CFINE    0x04
#define REG_CCOARSE  0x05
#define REG_NOISEPER 0x06
#define REG_ENABLE   0x07
#define REG_AVOL     0x08
#define REG_BVOL     0x09
#define REG_CVOL     0x0a
#define REG_EAFINE   0x0b
#define REG_EACOARSE 0x0c
#define REG_EASHAPE  0x0d
#define REG_PORTA    0x0e
#define REG_PORTB    0x0f

#define REG_EBFINE   0x10
#define REG_EBCOARSE 0x11
#define REG_ECFINE   0x12
#define REG_ECCOARSE 0x13
#define REG_EBSHAPE  0x14
#define REG_ECSHAPE  0x15
#define REG_ADUTY    0x16
#define REG_BDUTY    0x17
#define REG_CDUTY    0x18
#define REG_NOISEAND 0x19
#define REG_NOISEOR  0x1a
#define REG_TEST     0x1f


static u8 env_table_done = 0;
static u8 env_table[16][16];


static void init_env_table()
{
    // 0000, 0001, 0010, and 0011 are all  15->0 on first cycle, and 0 after that
    /*
     *
     * so we have continue, attack, alternate, and hold
     * attack =0 means initial descend, attack=1 means initial ascend
     * continue means keep looping it
     * alternate means swap mirrors back and forth each cycle
     * hold means, "flip priority and hold after first cycle?"
     */
    env_table_done = 1;
}

void ay_3_8910_init(ay_3_8910* this, enum ay_3_8910_variants variant)
{
    this->variant = variant;
    this->divider_16 = 0;
    if (!env_table_done) {
        //env_table_done = 1;
        init_env_table();
    }
}

void ay_3_8910_reset(ay_3_8910* this)
{
    this->divider_16 = 0;

}

void ay_3_8910_delete(ay_3_8910* this)
{

}



void ay_3_8910_write(ay_3_8910* this, u8 addr, u8 val)
{
    addr &= 0x1F;
    switch(addr) {
        case REG_AFINE:
            this->sw[0].freq = (this->sw[0].freq & 0x3F00) | val;
            return;
        case REG_ACOARSE:
            this->sw[0].freq = (this->sw[0].freq & 0xFF) | ((val & 0x3F) << 8);
            return;
        case REG_BFINE:
            this->sw[1].freq = (this->sw[1].freq & 0x3F00) | val;
            return;
        case REG_BCOARSE:
            this->sw[1].freq = (this->sw[1].freq & 0xFF) | ((val & 0x3F) << 8);
            return;
        case REG_CFINE:
            this->sw[2].freq = (this->sw[2].freq & 0x3F00) | val;
            return;
        case REG_CCOARSE:
            this->sw[2].freq = (this->sw[2].freq & 0xFF) | ((val & 0x3F) << 8);
            return;
        case REG_ENABLE:
            this->sw[0].enable = (val) & 1;
            this->sw[1].enable = (val >> 1) & 1;
            this->sw[2].enable = (val >> 2) & 1;
            this->sw[0].enable_noise = (val >> 3) & 1;
            this->sw[1].enable_noise = (val >> 4) & 1;
            this->sw[2].enable_noise = (val >> 5) & 1;
            this->io_ports.enable_a = (val >> 6) & 1;
            this->io_ports.enable_b = (val >> 7) & 1;
            return;
        case REG_NOISEPER:
            this->noise.period = val & 0x1F;
            return;
        case REG_AVOL:
            this->sw[0].amplitude = val & 15;
            this->sw[0].amplitude_mode = (val >> 4) & 1;
            return;
        case REG_BVOL:
            this->sw[1].amplitude = val & 15;
            this->sw[1].amplitude_mode = (val >> 4) & 1;
            return;
        case REG_CVOL:
            this->sw[2].amplitude = val & 15;
            this->sw[2].amplitude_mode = (val >> 4) & 1;
            return;
        case REG_EACOARSE:
            this->env.period = (this->env.period & 0xFF) | (val << 8);
            return;
        case REG_EAFINE:
            this->env.period = (this->env.period & 0xFF00) | val;
            return;
        case REG_EASHAPE:
            this->env.hold = (val) & 1;
            this->env.alternate = (val >> 1) & 1;
            this->env.attack = (val >> 2) & 1;
            this->env.econtinue = (val >> 3) & 1;
            return;
    }
}

u8 ay_3_8910_read(ay_3_8910* this, u8 addr)
{
    addr &= 0x1F;
    return 0;
}

static void tick_sw(ay_3_8910* this, u32 i)
{
    if (this->sw[i].counter == 0) {
        this->sw[i].counter = this->sw[i].freq;
        if (this->sw[i].counter == 0) this->sw[i].counter = 1;
        this->sw[i].polarity ^= 1;
    }
    this->sw[i].counter--;
}

static void tick_noise(ay_3_8910* this)
{
    if (this->noise.counter == 0) {
        this->noise.counter = this->noise.period;
        if (this->noise.counter == 0) this->noise.counter = 1;
        // update it
    }

    this->noise.counter--;
}

static void tick_env(ay_3_8910* this)
{
    if (this->env.count_up) this->env.counter++;
    else this->env.counter--;
    this->env.counter &= 15;

}

void ay_3_8910_cycle(ay_3_8910* this)
{
    u32 cdc = this->divider_16;
    this->divider_16 = (this->divider_16 + 1) & 15;

    if (cdc == 0) {
        u32 d256 = this->divider_256;
        this->divider_256 = (this->divider_256 + 1) & 15;
        if (d256 == 0) {
            tick_env(this);
        }

        // tick stuff
        for (u32 i = 0; i < 3; i++) {
            tick_sw(this, i);
        }

        tick_noise(this);
        this->noise.counter--;
    }
}
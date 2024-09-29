//
// Created by Dave on 2/8/2024.
//

#include "sn76489.h"

static i16 SMSGG_voltable[16] = {
        8191, 6507, 5168, 4105, 3261, 2590, 2057, 1642,
        1298, 1031, 819, 650, 516, 410, 326, 0
};

void SN76489_reset(struct SN76489* this)
{
    this->polarity[0] = this->polarity[1] = this->polarity[2] = this->polarity[3] = 1;
    this->vol[0] = this->vol[1] = this->vol[2] = 0x0F;
    this->vol[3] = 0x07;
    
    this->sw[0].freq = this->sw[1].freq = this->sw[2].freq = 0;
    this->noise.lfsr = 0x8000;
    
    this->io_reg = 0;
}

void SN76489_init(struct SN76489* this)
{
    this->sw[0] = this->sw[1] = this->sw[2] = (struct SN76489_SW) { .counter=0, .freq=0 };
    this->io_kind = 0;
    SN76489_reset(this);

    this->noise.ext_enable = 1;
    this->sw[0].ext_enable = this->sw[1].ext_enable = this->sw[2].ext_enable = 1;
    this->ext_enable = 1;
}

static void SN76489_cycle_squares(struct SN76489* this)
{
    for (u32 i = 0; i < 3; i++) {
        struct SN76489_SW* tone = &this->sw[i];
        if ((tone->counter > 0) && ((tone->freq > 7) || (tone->freq < 2))) {
            if (tone->counter > 0)
                tone->counter--;

            if (tone->counter <= 0) {
                tone->counter = tone->freq;

                if (tone->freq >= 7) // according to MaskOfDestiny, 0 and 1 should toggle every cycle, and be basically the same, due to up-count and >=
                    this->polarity[i] ^= 1;
            }
        }
    }
}

static void SN76489_cycle_noise(struct SN76489* this)
{
    this->noise.counter--;
    if (this->noise.counter <= 0) {
        u32 multiplier = 1;
        switch (this->noise.shift_rate) {
            case 0:
                this->noise.counter += 16 * multiplier;
                break;
            case 1:
                this->noise.counter += 2 * 16 * multiplier;
                break;
            case 2:
                this->noise.counter += 4 * 16 * multiplier;
                break;
            case 3:
                this->noise.counter += this->sw[2].freq * 16;
                break;
        }
        this->noise.countdown ^= 1;
        if (this->noise.countdown) {
            this->polarity[3] = (i16)(this->noise.lfsr & 1);

            if (this->noise.mode) { // White noise mode
                u32 p = this->noise.lfsr & 9;
                p ^= (p >> 8);
                p ^= (p >> 4);
                p &= 0xF;
                p = ((0x6996 >> p) & 1) ^ 1;
                this->noise.lfsr = (this->noise.lfsr >> 1) | (p << 15);
            } else {
                this->noise.lfsr = (this->noise.lfsr >> 1) | ((this->noise.lfsr & 1) << 15);
            }
        }
    }
}

void SN76489_cycle(struct SN76489* this) {
    SN76489_cycle_noise(this);
    SN76489_cycle_squares(this);
}

i16 SN76489_sample_channel(struct SN76489* this, int i)
{
    i16 sample = 0;
    i16 intensity = SMSGG_voltable[this->vol[i]];
    if (i < 3) { // square wave
        if (this->sw[i].freq > 7) { // 5 and lower are basically off, and 8-6 should be muted anyway cuz reasons
            sample = ((this->polarity[i] * 2) - 1) * intensity;
        }
    }
    else sample = ((this->polarity[3] * 2) - 1) * intensity;
    return sample;
}

i16 SN76489_mix_sample(struct SN76489* this, u32 for_debug)
{
    i16 sample = 0;
    if ((!this->ext_enable) && (!for_debug)) return 0;
    for (u32 i = 0; i < 3; i++) {
        i16 intensity = SMSGG_voltable[this->vol[i]];
        if (this->sw[i].ext_enable && (this->sw[i].freq > 7)) { // 5 and lower are basically off, and 8-6 should be muted anyway cuz reasons
            sample += ((this->polarity[i] * 2) - 1) * intensity;
        }
    }

    i16 intensity = SMSGG_voltable[this->vol[3]];
    if (this->noise.ext_enable) sample += ((this->polarity[3] * 2) - 1) * intensity;

    return sample;
}

void SN76489_write_data(struct SN76489* this, u32 val)
{
    if (val & 0x80) { // LATCH/DATA byte
        this->io_reg = (val >> 5) & 3;
        this->io_kind = (val >> 4) & 1;
        u32 data = val & 0x0F;
        if (this->io_kind) { // volume
            this->vol[this->io_reg] = data;
        }
        else { // tone
            if (this->io_reg < 3) this->sw[this->io_reg].freq = (this->sw[this->io_reg].freq & 0x3F0) | data;
            else {
                this->noise.lfsr = 0x8000;
                this->noise.shift_rate = data & 3;
                this->noise.mode = (data >> 2) & 1;
            }
        }
    }
    else {  // Data byte
        u32 data = val & 0x0F;
        if (this->io_kind) { // volume
            this->vol[this->io_reg] = data;
        } else {
            if (this->io_reg < 3)
                this->sw[this->io_reg].freq = (this->sw[this->io_reg].freq & 0x0F) | ((val & 0x3F) << 4);
            else {
                this->noise.lfsr = 0x8000;
                this->noise.shift_rate = data & 3;
                this->noise.mode = (data >> 2) & 1;
            }
        }
    }
}
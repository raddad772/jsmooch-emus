//
// Created by Dave on 2/8/2024.
//

#include <cstdio>
#include <cstring>

#include "sn76489.h"

static constexpr i16 SMSGG_voltable[16] = {
        8191, 6507, 5168, 4105, 3261, 2590, 2057, 1642,
        1298, 1031, 819, 650, 516, 410, 326, 0
};

void SN76489::reset()
{
    polarity[0] = polarity[1] = polarity[2] = polarity[3] = 1;
    vol[0] = vol[1] = vol[2] = 0x0F;
    vol[3] = 0x0F;
    //vol[0] = vol[1] = vol[2] = vol[3] = 0;
    
    sw[0].freq = sw[1].freq = sw[2].freq = 0;
    noise.lfsr = 0x8000;
    
    io_reg = 0;
}

void SN76489::cycle_squares()
{
    for (u32 i = 0; i < 3; i++) {
        SN76489_SW& tone = sw[i];
        //if ((tone.counter > 0) && ((tone.freq > 7) || (tone.freq < 2))) {
        //if ((tone.counter > 0) || (tone.freq > 7) || (tone.freq < 2)) {
            if (tone.counter > 0)
                tone.counter--;

            if (tone.counter <= 0) {
                tone.counter = tone.freq;

                //if ((tone.freq == 1) || (tone.freq >= 7)) // according to MaskOfDestiny, 0 and 1 should toggle every cycle, and be basically the same, due to up-count and >=
                polarity[i] ^= 1;
            }
        //}
    }
}

void SN76489::cycle_noise()
{
    noise.counter--;
    if (noise.counter <= 0) {
        u32 multiplier = 1;
        switch (noise.shift_rate) {
            case 0:
                noise.counter += 16 * multiplier;
                break;
            case 1:
                noise.counter += 2 * 16 * multiplier;
                break;
            case 2:
                noise.counter += 4 * 16 * multiplier;
                break;
            case 3:
                noise.counter += sw[2].freq * 16;
                break;
        }
        noise.countdown ^= 1;
        if (noise.countdown) {
            polarity[3] = static_cast<i16>(noise.lfsr & 1);

            if (noise.mode) { // White noise mode
                u32 p = noise.lfsr & 9;
                p ^= (p >> 8);
                p ^= (p >> 4);
                p &= 0xF;
                p = ((0x6996 >> p) & 1) ^ 1;
                noise.lfsr = (noise.lfsr >> 1) | (p << 15);
            } else {
                noise.lfsr = (noise.lfsr >> 1) | ((noise.lfsr & 1) << 15);
            }
        }
    }
}

void SN76489::cycle() {
    cycle_noise();
    cycle_squares();
}

i16 SN76489::sample_channel(int i)
{
    i16 sample = 0;
    i16 intensity = SMSGG_voltable[vol[i]];
    if (i < 3) { // square wave
        //if (sw[i].freq > 7) { // 5 and lower are basically off, and 8-6 should be muted anyway cuz reasons
        sample = ((polarity[i] * 2) - 1) * intensity;
        //}
    }
    else sample = ((polarity[3] * 2) - 1) * intensity;
    return sample;
}

i16 SN76489::mix_sample(bool for_debug)
{
    i16 sample = 0;
    if ((!ext_enable) && (!for_debug)) return 0;
    for (u32 i = 0; i < 3; i++) {
        i16 intensity = SMSGG_voltable[vol[i]];
        //if (sw[i].ext_enable && (sw[i].freq > 7)) { // 5 and lower are basically off, and 8-6 should be muted anyway cuz reasons
        if (sw[i].ext_enable) { // 5 and lower are basically off, and 8-6 should be muted anyway cuz reasons
            sample += ((polarity[i] * 2) - 1) * intensity;
        }
    }

    //i16 intensity = SMSGG_voltable[(vol[3] << 1) + 1];

    if (noise.ext_enable) {
        i16 intensity = SMSGG_voltable[vol[3]];
        sample += ((polarity[3] * 2) - 1) * intensity;
    }

    return sample;
}

void SN76489::write_data(u8 val)
{
    if (val & 0x80) { // LATCH/DATA byte
        io_reg = (val >> 5) & 3;
        io_kind = (val >> 4) & 1;
        u32 data = val & 0x0F;
        if (io_kind) { // volume
            vol[io_reg] = data;
        }
        else { // tone
            if (io_reg < 3) sw[io_reg].freq = (sw[io_reg].freq & 0x3F0) | data;
            else {
                noise.lfsr = 0x8000;
                noise.shift_rate = data & 3;
                noise.mode = (data >> 2) & 1;
            }
        }
    }
    else {  // Data byte
        u32 data = val & 0x0F;
        if (io_kind) { // volume
            vol[io_reg] = data;
        } else {
            if (io_reg < 3)
                sw[io_reg].freq = (sw[io_reg].freq & 0x0F) | ((val & 0x3F) << 4);
            else {
                noise.lfsr = 0x8000;
                noise.shift_rate = data & 3;
                noise.mode = (data >> 2) & 1;
            }
        }
    }
}

void SN76489::serialize(serialized_state &state)
{
    u32 i;
#define S(x) Sadd(state, & x, sizeof( x))
    S(io_reg);
    S(io_kind);

    for (i = 0; i < 4; i++) {
        S(vol[i]);
        S(polarity[i]);
    }

    for (i = 0; i < 3; i++) {
        S(sw[i].counter);
        S(sw[i].freq);
    }

    S(noise.lfsr);
    S(noise.shift_rate);
    S(noise.mode);
    S(noise.counter);
    S(noise.countdown);
#undef S
}

#define L(x) Sload(state, & x, sizeof( x))
void SN76489::deserialize(serialized_state &state)
{
    u32 i;
    L(io_reg);
    L(io_kind);

    for (i = 0; i < 4; i++) {
        L(vol[i]);
        L(polarity[i]);
    }

    for (i = 0; i < 3; i++) {
        L(sw[i].counter);
        L(sw[i].freq);
    }

    L(noise.lfsr);
    L(noise.shift_rate);
    L(noise.mode);
    L(noise.counter);
    L(noise.countdown);
}
#undef L

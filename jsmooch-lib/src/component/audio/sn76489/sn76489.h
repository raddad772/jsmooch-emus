//
// Created by Dave on 2/8/2024.
//

#ifndef JSMOOCH_EMUS_SN76489_H
#define JSMOOCH_EMUS_SN76489_H

#include "helpers/int.h"

/*
Big thanks to TotalJustice of TotalSMS, who allowed me to
 use their implementation of the SMS PSG with minor
 modifications
 */

struct SN76489 {
    u32 vol[4];
    i16 polarity[4];
    struct SN76489_noise {
        u32 lfsr;
        u32 shift_rate;
        u32 mode;
        i32 counter;
        u32 countdown;
        u32 ext_enable;
    } noise;

    struct SN76489_SW {
        i32 counter;
        i32 freq;
        u32 ext_enable;
    } sw[3];

    u32 ext_enable;
    u32 io_reg; // current register selected
    u32 io_kind; // 0 = tone, 1 = volume
};

void SN76489_init(struct SN76489*);
void SN76489_cycle(struct SN76489*);
i16 SN76489_mix_sample(struct SN76489*, u32 for_debug);
i16 SN76489_sample_channel(struct SN76489*, int num);
void SN76489_reset(struct SN76489*);
void SN76489_write_data(struct SN76489*, u32 val);

#endif //JSMOOCH_EMUS_SN76489_H

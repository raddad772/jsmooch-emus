//
// Created by . on 9/6/24.
//

#ifndef JSMOOCH_EMUS_GB_APU_H
#define JSMOOCH_EMUS_GB_APU_H

#include "helpers_c/int.h"

struct GB_APU {
    u32 ext_enable;

    struct GBSNDCHAN {
        u32 ext_enable;
        u32 number;
        u32 dac_on;

        u32 vol;
        u32 on;
        u32 left, right;

        u32 period;
        u32 next_period;
        u32 length_enable;
        i32 period_counter;
        u32 wave_duty;
        i32 polarity;
        u32 duty_counter;
        u32 short_mode;
        u32 divisor;
        u32 clock_shift;
        i32 length_counter;

        i32 samples[16];
        u8 sample_buffer[2];

        struct GBSNDSWEEP {
            i32 pace;
            i32 pace_counter;
            i32 direction;
            i32 individual_step;
        } sweep;

        struct GBSNDENVSWEEP {
            i32 period;
            i32 period_counter;
            i32 direction;
            u32 initial_vol;
            u32 rshift;
            u32 on;
        } env;
    } channels[4];

    struct {
        u32 master_on;
    } io;

    struct {
        i32 divider_2048; // / 512Hz @ 1MHz
        u32 frame_sequencer;
    } clocks;
};

void GB_APU_init(struct GB_APU* this);
void GB_APU_write_IO(struct GB_APU* this, u32 addr, u8 val);
u8 GB_APU_read_IO(struct GB_APU* this, u32 addr, u8 old_val, u32 has_effect);
void GB_APU_cycle(struct GB_APU* this);
float GB_APU_mix_sample(struct GB_APU* this, u32 is_debug);
float GB_APU_sample_channel(struct GB_APU* this, int cnum);

#endif //JSMOOCH_EMUS_GB_APU_H

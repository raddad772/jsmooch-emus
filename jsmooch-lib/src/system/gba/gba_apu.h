//
// Created by . on 12/28/24.
//

#ifndef JSMOOCH_EMUS_GBA_APU_H
#define JSMOOCH_EMUS_GBA_APU_H

#include "helpers/int.h"

struct GBA_APU {
    u32 ext_enable;
    struct GBA_APU_FIFO {
        u32 timer_id;
        u32 head, tail, len, output_head;
        i8 data[32]; // 8 32-bit entries

        i32 sample;
        i32 output; // output being 0-15
        u32 enable_l, enable_r;
        u32 vol;
        u32 needs_commit;

        u32 ext_enable;

    } fifo[2];

    struct {
        u32 divider_16; // 16MHz->1MHz divider
        u32 divider_16_4; // 16 * 4 divider. 1MHz->262kHz
        u32 divider_2048; // / 512Hz @ 1MHz
        u32 frame_sequencer;
    } clocks;

    struct {
        float float_l, float_r;
    } output;


    struct GBASNDCHAN {
        u32 ext_enable;
        u32 number;
        u32 dac_on;

        u32 vol;
        u32 on;
        u32 left, right;

        u32 enable_l, enable_r;

        u32 period;
        u32 next_period;
        u32 length_enable;
        u32 period_counter;
        u32 wave_duty;
        i32 polarity;
        u32 duty_counter;
        u32 short_mode;
        u32 divisor;
        u32 clock_shift;
        i32 length_counter;

        i32 samples[32];
        u32 sample_bank_mode;
        u32 sample_sample_bank; // Keeping track of our bank playing
        u32 cur_sample_bank;
        u8 sample_buffer[2];

        struct GBASNDSWEEP {
            i32 pace;
            i32 pace_counter;
            i32 direction;
            i32 individual_step;
        } sweep;

        struct GBASNDENVSWEEP {
            i32 period;
            i32 period_counter;
            i32 direction;
            u32 initial_vol;
            u32 force75;
            u32 rshift;
            u32 on;
        } env;

    } channels[4];

    struct {
        i32 output_l, output_r;
    } psg;

    struct {
        u32 sound_bias;
        u32 ch03_vol;
        u32 ch03_vol_l;
        u32 ch03_vol_r;
        u32 master_enable;
        u32 bias_amplitude;
    } io;

    struct {
        u32 counter, divisor, mask;
    } divider2;
};

struct GBA;
void GBA_APU_init(struct GBA*);
u32 GBA_APU_read_IO(struct GBA*, u32 addr, u32 sz, u32 access, u32 has_effect);
void GBA_APU_write_IO(struct GBA*, u32 addr, u32 sz, u32 access, u32 val);
void GBA_APU_cycle(struct GBA*);
void GBA_APU_sound_FIFO(struct GBA*, u32 num);
float GBA_APU_mix_sample(struct GBA*, u32 is_debug);
float GBA_APU_sample_channel(struct GBA*, u32 num);
#endif //JSMOOCH_EMUS_GBA_APU_H

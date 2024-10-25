//
// Created by . on 7/19/24.
//

#ifndef JSMOOCH_EMUS_YM2612_H
#define JSMOOCH_EMUS_YM2612_H

#include "helpers/int.h"

struct ym2612_env {
    u16 TL; // 7 bits Total Level
    u16 SL; // 4 bits Sustain Level
    u16 AR; // 5 bits Attack Rate
    u16 DR; // 5 bits Decay Rate
    u16 SR; // 5 bits Sustain Rate
    u16 RR; // 4 bits Release Rate
    u16 RS; // Rate Scale

    struct {
        u32 enable, attack, alt, hold, invert;
    } ssg;
    enum {
        EP_attack = 0,
        EP_decay = 1,
        EP_sustain = 2,
        EP_release = 3,
    } env_phase;
    u16 rate; // 6-bit calculated Rate value
    u16 attenuation; // Current 10-bit attentuation value
    u16 polarity; // flag indicating polarity of output; inverts output
    u16 key_scale; // 3 bits key scaling
    u32 invert_xor;

    struct {
        u32 period, counter;
        u32 phase;
    } divider;
    u32 steps;
};

struct ym2612 {
    u16 output; // Current mixed sample

    struct {
        u16 address;
    } io;

    struct YM2612_CHANNEL {
        u32 mode; // 0 = single-frequency. Only ch[2] allows any other mode
        u32 algorithm, tremolo, vibrato, feedback;

        u32 ext_enable;

        u16 output; // Output value of channel, mixed

        struct YM2612_OPERATOR {
            struct ym2612_env env;
            u16 output_level;
            u16 total_level;
            u32 lfo_enable;
            u32 detune, multiple;
            u32 key;

            struct {
                u32 reload, latch, value;
            } pitch;

            struct {
                u32 reload, latch, value;
            } octave;

            struct {
                u32 delta, value;
            } phase;
        } operator[4];
    } channel[6];

    struct {
        u32 sample;
        u32 enable;
    } dac;

    struct {
        u32 div144;
        u32 div24;
        u32 env_divider;
        u32 env_cycle_counter;
        u32 timer_count;
    } clock;

    struct {
        u32 clock, rate, enable, divider;
    } lfo;

    struct {
        u32 enabled;
        u16 period;
        u32 counter;
        u32 line;
        u32 irq_enable;
    } timer_a;

    struct {
        u32 enabled;
        u16 period;
        u32 counter, divider;
        u32 line;
        u32 irq_enable;
    } timer_b;
};

void ym2612_init(struct ym2612*);
void ym2612_delete(struct ym2612*);

void ym2612_write(struct ym2612*, u32 addr, u8 val);
u8 ym2612_read(struct ym2612*, u32 addr, u32 old, u32 has_effect);
void ym2612_reset(struct ym2612*);
void ym2612_cycle(struct ym2612*);
i16 ym2612_sample_channel(struct ym2612*, u32 ch);

#endif //JSMOOCH_EMUS_YM2612_H

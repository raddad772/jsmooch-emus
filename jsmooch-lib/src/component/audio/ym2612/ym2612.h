//
// Created by . on 7/19/24.
//

#ifndef JSMOOCH_EMUS_YM2612_H
#define JSMOOCH_EMUS_YM2612_H

/* Old version was hybrid.
 * New version is straight Ares port to learn from
 */

#include "helpers/debugger/debuggerdefs.h"
#include "helpers/int.h"

enum OPN2_variant {
    OPN2V_ym2612,
    OPN2V_ym3438
};

struct ym2612 {

    enum OPN2_variant variant;
    struct {
        i32 left_output, right_output, mono_output; // Current mixed sample
        u64 last_master_clock_op0;
    } mix;

    struct {
        u16 address; // 9bit
    } io;

    struct YM2612_CHANNEL {
        u32 num;
        u32 ext_enable;
        u32 left_enable, right_enable;
        struct {
            i32 mono, left, right; // 14 bits
        } ext_output;

        u32 algorithm, feedback, vibrato; //3bit
        u32 tremolo, mode; // 2bit

        struct YM2612_OPERATOR {
            u32 num;
            u32 key_on; // 1bit
            u32 key_line; // 1bit
            u32 lfo_enable; // 1bit
            u32 detune; // 3bit
            u32 multiple; //4bit
            u32 total_level; // 7bit

            u16 output_level;
            i16 output, prior, prior_buffer;

            struct {
                u16 value, reload, latch; // 11 bits
            } pitch;

            struct {
                u16 value, reload, latch; // 3 bits
            } octave;

            struct {
                u32 value, delta; // 20bit
            } phase;

            struct YM2612_ENV {
                enum {
                    EP_attack = 0,
                    EP_decay = 1,
                    EP_sustain = 2,
                    EP_release = 3,
                } state;
                i32 rate, divider;
                u32 steps;
                u32 value;

                u32 key_scale; // 2bit
                u32 attack_rate; // 5bit
                u32 decay_rate; // 5bit
                u32 sustain_rate; // 5bit
                u32 sustain_level; // 4bit
                u32 release_rate; // 5bit
            } envelope;

            struct {
                u32 enable, attack, alternate, hold, invert; // 1bit
            } ssg;
        } operator[4];
    } channel[6];

    struct {
        u32 sample;
        u32 enable;
    } dac;

    struct {
        u32 div144;
    } clock;

    struct {
        u32 clock, divider; // clock=12bit, divider = 32bit
    } envelope;

    struct {
        u32 clock, rate, enable, divider;
    } lfo;

    struct {
        u32 enable, irq, line, period, counter;
    } timer_a;

    struct {
        u32 enable, irq, line, period, counter, divider;
    } timer_b;

    u64 *master_cycle_count;
    u64 last_master_cycle;

    DBG_EVENT_VIEW_ONLY;
};

void ym2612_init(struct ym2612 *this, enum OPN2_variant variant, u64 *master_cycle_count);
void ym2612_delete(struct ym2612*);

void ym2612_write(struct ym2612*, u32 addr, u8 val);
u8 ym2612_read(struct ym2612*, u32 addr, u32 old, u32 has_effect);
void ym2612_reset(struct ym2612*);
void ym2612_cycle(struct ym2612*);
i16 ym2612_sample_channel(struct ym2612*, u32 ch);

#endif //JSMOOCH_EMUS_YM2612_H

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
    u32 ext_enable;

    struct {
        u32 group;
        u32 addr;

        u32 chn, opn;

        u32 ch3_special;
    } io;

    struct {
        i32 busy_for_how_long, timer_b_overflow, timer_a_overflow;
        u64 env_cycle_counter;
    } status;


    struct YM2612_CHANNEL {
        i32 op0_prior[2];
        u32 num;
        u32 block;
        u32 ext_enable;
        u32 left_enable, right_enable;
        i32 output;

        u32 algorithm, feedback, pms; //3bit . pms = vibrato
        u32 ams, mode; // 2bit ams = tremolo

        struct YM2612_OPERATOR {
            u32 num;
            u32 key_on; // 1bit
            u32 key_line; // 1bit
            u32 lfo_enable; // 1bit
            u32 detune; // 3bit
            i32 detune_delta;
            struct {
                u32 val;
                u32 rshift, multiplier;
            } multiple;

            struct YM2612_CHANNEL *ch;

            i32 output;

            struct {
                u16 value, reload, latch; // 11 bits
            } fnum;

            struct {
                u16 value, reload, latch; // 3 bits
            } block;


            struct {
                u32 value, delta; // 20bit

                u32 input;
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
                u32 level; // 10 bits, 4.6 fixed-point
                u32 attenuation;

                u32 key_scale; // 2bit
                i32 rks;
                u32 attack_rate; // 5bit
                u32 decay_rate; // 5bit
                u32 sustain_rate; // 5bit
                u32 sustain_level; // 4bit
                u32 release_rate; // 5bit

                u32 total_level;
            } envelope;

        } operator[4];
    } channel[6];

    struct {
        i32 sample;
        u32 enable;
    } dac;

    struct {
        u32 div24;
        u32 div24_3;
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

    DBG_EVENT_VIEW_ONLY;
};

struct serialized_state;
void ym2612_init(struct ym2612 *this, enum OPN2_variant variant, u64 *master_cycle_count);
void ym2612_delete(struct ym2612*);
void ym2612_serialize(struct ym2612*, struct serialized_state *state);
void ym2612_deserialize(struct ym2612*, struct serialized_state *state);

void ym2612_write(struct ym2612*, u32 addr, u8 val);
u8 ym2612_read(struct ym2612*, u32 addr, u32 old, u32 has_effect, u64 master_clock);
void ym2612_reset(struct ym2612*);
void ym2612_cycle(struct ym2612*);
i16 ym2612_sample_channel(struct ym2612*, u32 ch);

#endif //JSMOOCH_EMUS_YM2612_H

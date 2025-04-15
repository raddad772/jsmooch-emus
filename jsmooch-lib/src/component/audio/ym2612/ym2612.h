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

    struct ym2612_lfo {
        u32 enabled;
        u8 counter, divider, period;
    } lfo;


    struct {
        u32 group;
        u32 addr;
        u32 csm_enabled;
    } io;

    struct {
        u64 busy_until;
        u64 env_cycle_counter;
    } status;


    struct YM2612_CHANNEL {
        enum YM2612_FREQ_MODE {
            YFM_single = 0,
            YFM_multiple
        } mode;
        i32 op0_prior[2];
        u32 num;
        u32 ext_enable;
        u32 left_enable, right_enable;
        i16 output;

        u32 algorithm, feedback, pms; //3bit . pms = vibrato
        u32 ams; // 2bit ams = tremolo
        struct {
            u16 value, latch; // 11 bits
        } f_num;
        struct {
            u16 value, latch; // 11 bits
        } block;


        struct YM2612_OPERATOR {
            u32 num;
            u32 am_enable; // 1bit

            u32 lfo_counter, ams;

            struct YM2612_CHANNEL *ch;

            i16 output;
            struct {
                u32 value, latch;
            } f_num;
            struct {
                u32 value, latch;
            } block;

            struct {
                u32 counter; // 20bit
                u16 output; // 10bit
                u16 f_num, block;
                u32 input;
                u32 multiple, detune;
            } phase;

            struct YM2612_ENV {
                enum {
                    EP_attack = 0,
                    EP_decay = 1,
                    EP_sustain = 2,
                    EP_release = 3,
                } state;
                i32 rate, divider;
                u32 cycle_count;
                u32 steps;
                u32 level; // 10 bits, 4.6 fixed-point
                i32 attenuation;

                u32 key_scale; // 2bit
                u32 key_scale_rate;
                u32 attack_rate; // 5bit
                u32 decay_rate; // 5bit
                u32 sustain_rate; // 5bit
                u32 sustain_level; // 4bit
                u32 release_rate; // 5bit

                struct {
                    u32 invert_output, attack, enabled, hold, alternate;
                }ssg;
                u32 total_level;
            } envelope;

            u32 key_on;

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
        u32 enable, irq, line, period, counter;
    } timer_a;

    struct {
        u32 enable, irq, line, period, counter, divider;
    } timer_b;

    u64 *master_cycle_count;
    u64 master_wait_cycles;

    DBG_EVENT_VIEW_ONLY;
};

struct serialized_state;
void ym2612_init(struct ym2612 *this, enum OPN2_variant variant, u64 *master_cycle_count, u64 master_wait_cycles);
void ym2612_delete(struct ym2612*);
void ym2612_serialize(struct ym2612*, struct serialized_state *state);
void ym2612_deserialize(struct ym2612*, struct serialized_state *state);

void ym2612_write(struct ym2612*, u32 addr, u8 val);
u8 ym2612_read(struct ym2612*, u32 addr, u32 old, u32 has_effect);
void ym2612_reset(struct ym2612*);
void ym2612_cycle(struct ym2612*);
i16 ym2612_sample_channel(struct ym2612*, u32 ch);

#endif //JSMOOCH_EMUS_YM2612_H

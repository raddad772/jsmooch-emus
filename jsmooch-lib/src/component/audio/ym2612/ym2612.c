//
// Created by . on 10/25/24.
//

// This is a straight port of the Ares code, to help
//  me understand this cool beast.

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <assert.h>

#include "helpers/debug.h"
#include "helpers/serialize/serialize.h"
#include "ym2612.h"

#define SIGNe14to32(x) (((((x) >> 13) & 1) * 0xFFFFC000) | ((x) & 0x3FFF))

#ifndef MIN
#define MIN(a,b) ((a) < (b) ? (a) : (b))
#endif

#ifndef MAX
#define MAX(a,b) ((a) > (b) ? (a) : (b))
#endif

static i32 sine[0x200];
static i32 pow2[0x100];
static const i32 detune_table[32][4] = {
        {0,  0,  1,  2},  {0,  0,  1,  2},  {0,  0,  1,  2},  {0,  0,  1,  2},  // Block 0
        {0,  1,  2,  2},  {0,  1,  2,  3},  {0,  1,  2,  3},  {0,  1,  2,  3},  // Block 1
        {0,  1,  2,  4},  {0,  1,  3,  4},  {0,  1,  3,  4},  {0,  1,  3,  5},  // Block 2
        {0,  2,  4,  5},  {0,  2,  4,  6},  {0,  2,  4,  6},  {0,  2,  5,  7},  // Block 3
        {0,  2,  5,  8},  {0,  3,  6,  8},  {0,  3,  6,  9},  {0,  3,  7, 10},  // Block 4
        {0,  4,  8, 11},  {0,  4,  8, 12},  {0,  4,  9, 13},  {0,  5, 10, 14},  // Block 5
        {0,  5, 11, 16},  {0,  6, 12, 17},  {0,  6, 13, 19},  {0,  7, 14, 20},  // Block 6
        {0,  8, 16, 22},  {0,  8, 16, 22},  {0,  8, 16, 22},  {0,  8, 16, 22},  // Block 7
};

static const int envelope_rates[64][8] = {
        {0,0,0,0,0,0,0,0}, {0,0,0,0,0,0,0,0}, {0,1,0,1,0,1,0,1}, {0,1,0,1,0,1,0,1},
        {0,1,0,1,0,1,0,1}, {0,1,0,1,0,1,0,1}, {0,1,1,1,0,1,1,1}, {0,1,1,1,0,1,1,1},
        {0,1,0,1,0,1,0,1}, {0,1,0,1,1,1,0,1}, {0,1,1,1,0,1,1,1}, {0,1,1,1,1,1,1,1},
        {0,1,0,1,0,1,0,1}, {0,1,0,1,1,1,0,1}, {0,1,1,1,0,1,1,1}, {0,1,1,1,1,1,1,1},
        {0,1,0,1,0,1,0,1}, {0,1,0,1,1,1,0,1}, {0,1,1,1,0,1,1,1}, {0,1,1,1,1,1,1,1},
        {0,1,0,1,0,1,0,1}, {0,1,0,1,1,1,0,1}, {0,1,1,1,0,1,1,1}, {0,1,1,1,1,1,1,1},
        {0,1,0,1,0,1,0,1}, {0,1,0,1,1,1,0,1}, {0,1,1,1,0,1,1,1}, {0,1,1,1,1,1,1,1},
        {0,1,0,1,0,1,0,1}, {0,1,0,1,1,1,0,1}, {0,1,1,1,0,1,1,1}, {0,1,1,1,1,1,1,1},
        {0,1,0,1,0,1,0,1}, {0,1,0,1,1,1,0,1}, {0,1,1,1,0,1,1,1}, {0,1,1,1,1,1,1,1},
        {0,1,0,1,0,1,0,1}, {0,1,0,1,1,1,0,1}, {0,1,1,1,0,1,1,1}, {0,1,1,1,1,1,1,1},
        {0,1,0,1,0,1,0,1}, {0,1,0,1,1,1,0,1}, {0,1,1,1,0,1,1,1}, {0,1,1,1,1,1,1,1},
        {0,1,0,1,0,1,0,1}, {0,1,0,1,1,1,0,1}, {0,1,1,1,0,1,1,1}, {0,1,1,1,1,1,1,1},
        {1,1,1,1,1,1,1,1}, {1,1,1,2,1,1,1,2}, {1,2,1,2,1,2,1,2}, {1,2,2,2,1,2,2,2},
        {2,2,2,2,2,2,2,2}, {2,2,2,4,2,2,2,4}, {2,4,2,4,2,4,2,4}, {2,4,4,4,2,4,4,4},
        {4,4,4,4,4,4,4,4}, {4,4,4,8,4,4,4,8}, {4,8,4,8,4,8,4,8}, {4,8,8,8,4,8,8,8},
        {8,8,8,8,8,8,8,8}, {8,8,8,8,8,8,8,8}, {8,8,8,8,8,8,8,8}, {8,8,8,8,8,8,8,8},
};

static int math_done = 0;

void do_math()
{
    math_done = 1;
    for (u32 n = 0; n < 0x200; n++) {
        u32 i = (!(n & 0x100)) ? n & 0xFF : 0x1FF - (n & 0x1FF);
        // Scale is slightly offset from [0, π/2]
        u32 base_phase = 2 * i + 1;
        double phase = ((double)base_phase) / 512.0 * M_PI / 2.0;

        // Compute the inverted log2-sine value
        double attenuation = -log2(sin(phase));

        // Convert to 4.8 fixed-point decimal
        sine[n] = (i32)(u16)round((attenuation * 256.0));
    }
    for (u32 i = 0; i < 256; i++) {
        // Table index N represents exponent of -(N+1)/256
        double exponent = -((double)(i + 1)) / 256.0;
        double value = pow(2.0, exponent);

        // Convert to 0.11 fixed-point decimal
        pow2[i] = (i32)(u16)round((value * 2048.0));
    }
}

void ym2612_init(struct ym2612 *this, enum OPN2_variant variant, u64 *master_cycle_count)
{
    memset(this, 0, sizeof(*this));
    DBG_EVENT_VIEW_INIT;

    this->ext_enable = 1;
    this->master_cycle_count = master_cycle_count;

    if (!math_done) do_math();

    for (u32 i = 0; i < 6; i++) {
        struct YM2612_CHANNEL *ch = &this->channel[i];
        ch->num = i;
        ch->left_enable = ch->right_enable = 1;
        ch->block.value = i < 3 ? 0 : 1;
        for (u32 j = 0; j < 4; j++) {
            struct YM2612_OPERATOR *op = &ch->operator[j];
            op->num = j;
            op->ch = ch;
        }
    }
    this->dac.sample = 0;
}

void ym2612_delete(struct ym2612*this)
{

}

static void write_addr(struct ym2612 *this, u32 val, u32 hi)
{
    this->io.group = hi;
    this->io.addr = val;
}

static inline void op_key_on(struct ym2612 *this, struct YM2612_CHANNEL *ch, u32 opn, u32 val) {
    struct YM2612_OPERATOR *op = &ch->operator[opn];
    u32 key_is_on = op->envelope.state != EP_release;
    if (val) {
        op->key_on = 1;
        if (!key_is_on) {
            op->phase.counter = 0;
            u32 rate = 2 * op->envelope.attack_rate + op->envelope.key_scale_rate;

            if (rate >= 62) {
                op->envelope.state = EP_decay;
                op->envelope.attenuation = 0;
            } else {
                op->envelope.state = EP_attack;
            }

            op->envelope.ssg.invert_output = 0;
        }
    } else {
        op->key_on = 0;
        if (op->envelope.ssg.enabled && (op->envelope.state != EP_release) && (op->envelope.ssg.invert_output != op->envelope.ssg.attack)) {
            op->envelope.attenuation =  (0x200 - op->envelope.attenuation) & 0x3FF;
            op->envelope.state = EP_release;
        }
    }
}

void ym2612_reset(struct ym2612*this)
{

}

static void lfo_enable(struct ym2612*this, u32 enabled)
{
    this->lfo.enabled = enabled;
    if (!enabled) this->lfo.counter = 0;
}

static void lfo_freq(struct ym2612* this, u8 val)
{
    static const int dividers[8] = { 108, 77, 71, 67, 62, 44, 8, 5 };
    this->lfo.period = dividers[val];
}

static void run_lfo(struct ym2612* this)
{
    this->lfo.divider++;
    if (this->lfo.divider >= this->lfo.period) {
        this->lfo.divider = 0;

        if (this->lfo.enabled) {
            this->lfo.counter = (this->lfo.counter + 1) & 0x7F;
        }
    }
}

static u16 lfo_fm(u8 lfo_counter, u8 pms, u16 f_num)
{
    if (pms == 0) return f_num << 1;
    u32 fm_table_idx = (lfo_counter & 0x20) ? (0x1F - (lfo_counter & 0x1F)) >> 2 : (lfo_counter & 0x1F) >> 2;

    static const u16 tabl[8][8] = { {0, 0, 0, 0, 0, 0, 0, 0},
                                    {0, 0, 0, 0, 4, 4, 4, 4},
                                    {0, 0, 0, 4, 4, 4, 8, 8},
                                    {0, 0, 4, 4, 8, 8, 12, 12},
                                    {0, 0, 4, 8, 8, 8, 12, 16},
                                    {0, 0, 8, 12, 16, 16, 20, 24},
                                    {0, 0, 16, 24, 32, 32, 40, 48},
                                    {0, 0, 32, 48, 64, 64, 80, 96},
                                    };


    u32 raw_increment = tabl[pms][fm_table_idx];
    u16 fm_increment = 0;
    for (int i = 4; i < 11; i++) {
        u32 bit = (f_num >> i) & 1;
        fm_increment += bit * (raw_increment >> (10 - i));
    }

    if (lfo_counter & 0x40)
        return ((f_num << 1) - fm_increment) & 0xFFF;
    else
        return ((f_num << 1) + fm_increment) & 0xFFF;
}

u16 lfo_am(u8 lfo_counter, u8 ams)
{
    u16 ama = (lfo_counter & 0x40) ? (lfo_counter & 0x3F) : (0x3F - lfo_counter);
    ama <<= 1;

    switch(ams) {
        case 0: return 0;
        case 1: return ama >> 3;
        case 2: return ama >> 1;
        case 3: return ama;
    }
    NOGOHERE;
}

static void mix_sample(struct ym2612*this)
{
    i32 left = 0, right = 0;
    for (u32 i = 0; i < 6; i++) {
        struct YM2612_CHANNEL *ch = &this->channel[i];
        if (ch->ext_enable) {
            i32 smp = ch->output;
            if (ch->left_enable) left += smp;
            if (ch->right_enable) right += smp;
        }
    }
    // 14-bit samples, 6 of them though, so we are at 17 bits, so shift right by 1
    this->mix.left_output = left >> 1;
    this->mix.right_output = right >> 1;
    this->mix.mono_output = (left + right) >> 1;
}


static inline u8 scale_key_code(u16 f_num, u8 block)
{
    // TODO: use lookup table
    u32 f11 = (f_num >> 10) & 1;
    u32 f10 = (f_num >> 9) & 1;
    u32 f9 = (f_num >> 8) & 1;
    u32 f8 = (f_num >> 7) & 1;
    return (block << 2)
           | (f11 << 1)
           | ((f11 && (f10 || f9 || f8) || (!f11 && f10 && f9 && f8)));
}

static i32 phase_compute_increment(struct ym2612 *this, struct YM2612_CHANNEL *ch, struct YM2612_OPERATOR *op)
{
    u32 mfn = lfo_fm(this->lfo.counter, ch->pms, op->phase.f_num);
    u32 shifted = (mfn << op->phase.block) >> 2;

    u32 key_code =scale_key_code(op->phase.f_num, op->phase.block);
    u32 detune_mag = op->phase.detune & 3;
    u32 detune_inc_mag = detune_table[key_code][detune_mag];
    u32 detuned_f = (op->phase.detune & 4) ? shifted - detune_inc_mag : shifted + detune_inc_mag;
    detuned_f &= 0x1FFFF;
    if (!op->phase.multiple) detuned_f >>= 1;
    else detuned_f *= op->phase.multiple;
    return detuned_f;
}

static void run_op_phase(struct ym2612 *this, struct YM2612_CHANNEL *ch, struct YM2612_OPERATOR *op)
{
    i32 inc = phase_compute_increment(this, ch, op);
    op->phase.counter = (op->phase.counter + inc) & 0xFFFFF;
    op->phase.output = op->phase.counter >> 10;
}

static void run_op_env_ssg(struct ym2612 *this, struct YM2612_CHANNEL *ch, struct YM2612_OPERATOR *op)
{
    if (op->envelope.attenuation < 0x200) return;

    if (op->envelope.ssg.alternate) {
        if (op->envelope.ssg.hold) {
            op->envelope.ssg.invert_output = 1;
        }
        else {
            op->envelope.ssg.invert_output ^= 1;
        }
    }
    else if (!op->envelope.ssg.hold) {
        op->phase.counter = 0;
    }

    if (!op->envelope.ssg.hold && ((op->envelope.state == EP_decay) || (op->envelope.state == EP_sustain))) {
        if ((2 * op->envelope.attack_rate + op->envelope.key_scale_rate) >= 62) {
            op->envelope.attenuation = 0;
            op->envelope.state = EP_decay;
        }
        else {
            op->envelope.state = EP_attack;
        }
    }
    else if ((op->envelope.state == EP_release) || ((op->envelope.state != EP_attack) && (op->envelope.ssg.invert_output == op->envelope.ssg.attack))) {
        op->envelope.attenuation = 0x3FF;
    }
}

static void run_op_env(struct ym2612 *this, struct YM2612_CHANNEL *ch, struct YM2612_OPERATOR *op)
{
    if (op->envelope.ssg.enabled) {
        run_op_env_ssg(this, ch, op);
    }

    op->envelope.divider--;
    if (!op->envelope.divider) {
        op->envelope.divider = 3;

        op->envelope.cycle_count++;
        op->envelope.cycle_count = (op->envelope.cycle_count & 0xFFF) + (op->envelope.cycle_count >> 12);

        u32 sustain_level = op->envelope.sustain_level == 15 ? 0x3E0 : op->envelope.sustain_level << 5;
        if (op->envelope.state == EP_attack && op->envelope.attenuation == 0) {
            op->envelope.state = EP_decay;
        }
        if (op->envelope.state == EP_decay && op->envelope.attenuation >= sustain_level) {
            op->envelope.state = EP_sustain;
        }

        u32 r;
        switch(op->envelope.state) {
            case EP_attack: r = op->envelope.attack_rate; break;
            case EP_decay: r = op->envelope.decay_rate; break;
            case EP_sustain: r = op->envelope.sustain_rate; break;
            case EP_release: r = (op->envelope.release_rate << 1) + 1; break;
        }

        i32 comp = 2 * r * op->envelope.key_scale_rate;
        i32 rate = !r ? 0 : MIN(63, comp);

        i32 update_f_shift = 11 - (rate >> 2);
        if (update_f_shift < 0) update_f_shift = 0;
        if ((op->envelope.cycle_count & ((1 << update_f_shift) - 1)) == 0) {
            u32 idx = (op->envelope.cycle_count >> update_f_shift) & 7;
            u16 increment = envelope_rates[rate][idx];
            switch(op->envelope.state) {
                case EP_attack:
                    if (rate <= 61) {
                        op->envelope.attenuation += (~op->envelope.attenuation * increment) >> 4;
                    }
                    break;
                case EP_sustain:
                case EP_release:
                case EP_decay:
                    if (op->envelope.ssg.enabled) {
                        if (op->envelope.attenuation < 0x200) {
                            u32 cmp = op->envelope.attenuation + (4 * increment);
                            op->envelope.attenuation = MIN(0x3FF, cmp);
                        }
                    }
                    else {
                        u32 cmp = MIN(0x3FF, op->envelope.attenuation + increment);
                    }
            }
            op->envelope.attenuation &= 0x3FF;
        }
    }
}

static u16 attenuation_to_amplitude(u16 attenuation)
{
    int int_part = (attenuation >> 8) & 0x1F;
    if (int_part > 12) return 0;
    u16 fp2 = pow2[attenuation & 0xFF];
    return (fp2 << 2) >> int_part;
}

static u16 op_attenuation(struct YM2612_OPERATOR *op)
{
    u16 attenuation;
    if (op->envelope.ssg.enabled && (op->envelope.state != EP_release) && (op->envelope.ssg.invert_output != op->envelope.ssg.attack)) {
        attenuation = (0x200 - op->envelope.attenuation) & 0x3FF;
    }
    else {
        attenuation = op->envelope.attenuation;
    }

    attenuation += op->envelope.total_level << 3;
    return MIN(0x3FF, attenuation);
}

static i16 run_op_output(struct YM2612_CHANNEL *ch, u32 opn, i32 mod_input)
{
    struct YM2612_OPERATOR *op = &ch->operator[opn];
    i16 phase = op->phase.output + mod_input;

    u32 sign = (phase >> 9) & 1;
    i32 sine_attenuation = sine[phase & 0x1FF];
    i32 env_attenuation = op_attenuation(op);

    i32 env_am_attenuation;
    if (op->am_enable) {
        i32 amat = lfo_am(op->lfo_counter, op->ams);
        env_am_attenuation = (env_attenuation + amat);
        if (env_am_attenuation < 0) env_am_attenuation = 0;
        if (env_am_attenuation > 0x3FF) env_am_attenuation = 0x3FF;
    }
    else env_am_attenuation = env_attenuation;

    u16 amplitude = attenuation_to_amplitude(sine_attenuation + (env_am_attenuation << 2));
    //op->prev_output = op->output;
    op->output = sign ? (-((i16)amplitude)) : (i16)amplitude;;
    return op->output;
}

static void run_ch(struct ym2612 *this, u32 chn)
{
    struct YM2612_CHANNEL *ch = &this->channel[chn];
    for (u32 opn = 0; opn < 4; opn++) {
        struct YM2612_OPERATOR *op = &ch->operator[opn];
        run_op_phase(this, ch, op);
        run_op_env(this, ch, op);

        op->lfo_counter = this->lfo.counter;
        op->ams = ch->ams;
    }
    // Update phases
    i32 input0 = 0;
    if (ch->feedback) {
        input0 = (ch->op0_prior[0] + ch->op0_prior[1]) >> (10 - ch->feedback);
    }
    i32 input, input2;

    // Apply algorithm
    switch(ch->algorithm) {
        case 0: // S1 -> S2 -> S3 -> S4. // but they calculate 1-3-2-4
            input = run_op_output(ch, 0, input0); // 1
            run_op_output(ch, 2, ch->operator[1].output); // 3
            input = run_op_output(ch, 1, input); // 2
            ch->output = run_op_output(ch, 3, input);
            break;
        case 1: // (S1 + S2) -> S3 -> S4.   1 3 2 4
            input = run_op_output(ch, 0, input0); // S1
            input += ch->operator[1].output; // old S2
            input = run_op_output(ch, 2, input); // S3 with (S1 + old S2)
            run_op_output(ch, 2, ch->operator[0].output); // S2 with current S1 to updaet it
            ch->output = run_op_output(ch, 3, input); // S4
            break;
        case 2: // S1 + (S2 -> S3) -> S4
            input = run_op_output(ch, 2, ch->operator[1].output); // S3
            run_op_output(ch, 1, 0); // S2
            input += run_op_output(ch, 0, input0); // S1
            ch->output = run_op_output(ch, 3, input); // S4
            break;
        case 3: // (S1 -> S2) + S3 -> S4
            input = run_op_output(ch, 0, input0);
            input = run_op_output(ch, 1, input);
            input += run_op_output(ch, 2, 0);
            ch->output = run_op_output(ch, 3, input);
            break;
        case 4: // (S1->S2) + (S3->S4)
            input = run_op_output(ch, 0, input0);
            input2 = run_op_output(ch, 2, 0);

            input = run_op_output(ch, 1, input);
            input2 = run_op_output(ch, 3, input2);
            ch->output = (input + input2);
            if (ch->output < -8192) ch->output = -8192;
            if (ch->output > 8191) ch->output = 8191;
            break;
        case 5: // S1 -> all(S2 + S3 + S4)
            input0 = run_op_output(ch, 0, input0);
            input = run_op_output(ch, 2, input0);
            input += run_op_output(ch, 1, input0);
            input += run_op_output(ch, 3, input0);
            ch->output = input;
            if (ch->output < -8192) ch->output = -8192;
            if (ch->output > 8191) ch->output = 8191;
            break;
        case 6: // (S1->S2) + S3 + S4
            input0 = run_op_output(ch, 0, input0);
            input = run_op_output(ch, 2, 0);
            input += run_op_output(ch, 1, input0);
            input += run_op_output(ch, 3, 0);
            ch->output = input;
            if (ch->output < -8192) ch->output = -8192;
            if (ch->output > 8191) ch->output = 8191;
            break;
        case 7: // add all 4
            input = run_op_output(ch, 0, input0);
            input += run_op_output(ch, 2, 0);
            input += run_op_output(ch, 1, 0);
            input += run_op_output(ch, 3, 0);
            ch->output = input;
            if (ch->output < -8192) ch->output = -8192;
            if (ch->output > 8191) ch->output = 8191;
            break;
    }
    ch->op0_prior[0] = ch->op0_prior[1];
    ch->op0_prior[1] = ch->operator[0].output;

    // Create outputs

    /*struct YM2612_OPERATOR *c = &ch->operator[3];
    ch->output = c->key_on ? c->output : 0;*/
}

static const i32 env_inc_table[64][8] = {
{0,0,0,0,0,0,0,0}, {0,0,0,0,0,0,0,0}, {0,1,0,1,0,1,0,1}, {0,1,0,1,0,1,0,1},  // 0-3
{0,1,0,1,0,1,0,1}, {0,1,0,1,0,1,0,1}, {0,1,1,1,0,1,1,1}, {0,1,1,1,0,1,1,1},  // 4-7
{0,1,0,1,0,1,0,1}, {0,1,0,1,1,1,0,1}, {0,1,1,1,0,1,1,1}, {0,1,1,1,1,1,1,1},  // 8-11
{0,1,0,1,0,1,0,1}, {0,1,0,1,1,1,0,1}, {0,1,1,1,0,1,1,1}, {0,1,1,1,1,1,1,1},  // 12-15
{0,1,0,1,0,1,0,1}, {0,1,0,1,1,1,0,1}, {0,1,1,1,0,1,1,1}, {0,1,1,1,1,1,1,1},  // 16-19
{0,1,0,1,0,1,0,1}, {0,1,0,1,1,1,0,1}, {0,1,1,1,0,1,1,1}, {0,1,1,1,1,1,1,1},  // 20-23
{0,1,0,1,0,1,0,1}, {0,1,0,1,1,1,0,1}, {0,1,1,1,0,1,1,1}, {0,1,1,1,1,1,1,1},  // 24-27
{0,1,0,1,0,1,0,1}, {0,1,0,1,1,1,0,1}, {0,1,1,1,0,1,1,1}, {0,1,1,1,1,1,1,1},  // 28-31
{0,1,0,1,0,1,0,1}, {0,1,0,1,1,1,0,1}, {0,1,1,1,0,1,1,1}, {0,1,1,1,1,1,1,1},  // 32-35
{0,1,0,1,0,1,0,1}, {0,1,0,1,1,1,0,1}, {0,1,1,1,0,1,1,1}, {0,1,1,1,1,1,1,1},  // 36-39
{0,1,0,1,0,1,0,1}, {0,1,0,1,1,1,0,1}, {0,1,1,1,0,1,1,1}, {0,1,1,1,1,1,1,1},  // 40-43
{0,1,0,1,0,1,0,1}, {0,1,0,1,1,1,0,1}, {0,1,1,1,0,1,1,1}, {0,1,1,1,1,1,1,1},  // 44-47
{1,1,1,1,1,1,1,1}, {1,1,1,2,1,1,1,2}, {1,2,1,2,1,2,1,2}, {1,2,2,2,1,2,2,2},  // 48-51
{2,2,2,2,2,2,2,2}, {2,2,2,4,2,2,2,4}, {2,4,2,4,2,4,2,4}, {2,4,4,4,2,4,4,4},  // 52-55
{4,4,4,4,4,4,4,4}, {4,4,4,8,4,4,4,8}, {4,8,4,8,4,8,4,8}, {4,8,8,8,4,8,8,8},  // 56-59
{8,8,8,8,8,8,8,8}, {8,8,8,8,8,8,8,8}, {8,8,8,8,8,8,8,8}, {8,8,8,8,8,8,8,8},  // 60-63
};

static u32 cycle_timers(struct ym2612* this)
{
    u32 retval = 0;
    if (this->timer_a.enable) {
        this->timer_a.counter = (this->timer_a.counter + 1) & 0x3FF; // 10 bits
        if (!this->timer_a.counter) {
            retval = 1;
            this->timer_a.counter = this->timer_a.period;
            this->timer_a.line |= this->timer_a.irq;
        }
    }

    if (this->timer_b.enable) {
        this->timer_b.divider = (this->timer_b.divider + 1) & 15;
        if (!this->timer_b.divider) {
            this->timer_b.counter = (this->timer_b.counter + 1) & 0xFF;
            if (!this->timer_b.counter) {
                this->timer_b.counter = this->timer_b.period;
                this->timer_b.line |= this->timer_b.irq;
            }
        }
    }
    return retval;
}

void ym2612_cycle(struct ym2612*this)
{
    run_lfo(this);
    u32 a_overflowed = cycle_timers(this);

    if (this->io.csm_enabled && a_overflowed) {
        // Man this chip is odd at times
        struct YM2612_CHANNEL *ch = &this->channel[2];
        for (u32 opn = 0; opn < 4; opn++) {
            struct YM2612_OPERATOR *op = &ch->operator[opn];
            if (op->envelope.state == EP_release) {
                op_key_on(this, ch, opn, 1);
                op_key_on(this, ch, opn, 0);
            }
        }
    }

    run_ch(this, 0);
    run_ch(this, 1);
    run_ch(this, 2);
    run_ch(this, 3);
    run_ch(this, 4);
    run_ch(this, 5);
    if (this->dac.enable) this->channel[5].output = this->dac.sample;
    mix_sample(this);

}

i16 ym2612_sample_channel(struct ym2612*this, u32 chn)
{
    // Return an i16
    struct YM2612_CHANNEL *ch = &this->channel[chn];
    return ch->output;
}

void ym2612_serialize(struct ym2612*this, struct serialized_state *state)
{

}

void ym2612_deserialize(struct ym2612*this, struct serialized_state *state)
{

}

static void env_update_key_scale_rate(struct ym2612* this, struct YM2612_CHANNEL *ch, struct YM2612_OPERATOR *op, u16 f_num, u16 block)
{
    op->envelope.key_scale_rate = scale_key_code(f_num, block) >> (3 - op->envelope.key_scale);
}

static void op_update_freq(struct ym2612 *this, struct YM2612_CHANNEL *ch, struct YM2612_OPERATOR *op, u16 f_num, u16 block)
{
    op->phase.f_num = f_num;
    op->phase.block = block;
    env_update_key_scale_rate(this, ch, op, f_num, block);
}

static void ch_update_phase_generators(struct ym2612 *this, struct YM2612_CHANNEL *ch)
{
    if ((ch->num == 2) && (ch->mode ==  YFM_multiple)) {
        for (u32 opn = 0; opn < 3; opn++) {
            struct YM2612_OPERATOR *op = &ch->operator[opn];
            op_update_freq(this, ch, op, op->f_num.value, op->block.value);
        }
        op_update_freq(this, ch, &ch->operator[3], ch->f_num.value, ch->block.value);
        return;
    }
    for (u32 opn = 0; opn < 4; opn++) {
        struct YM2612_OPERATOR *op = &ch->operator[opn];
        op_update_freq(this, ch, op, ch->f_num.value, ch->block.value);
    }
}

static void op_update_key_scale(struct ym2612 *this, struct YM2612_CHANNEL *ch, struct YM2612_OPERATOR *op, u8 val)
{
    op->envelope.key_scale = val;
    env_update_key_scale_rate(this, ch, op, op->phase.f_num, op->phase.block);
}

static void write_ch_reg(struct ym2612 *this, u8 val, u32 bch)
{
    u32 chn = bch + (this->io.addr & 3);
    struct YM2612_CHANNEL *ch = &this->channel[chn];
    switch(this->io.addr) {
        case 0xA0:
        case 0xA1:
        case 0xA2: {
            ch->f_num.value = ch->f_num.latch | val;
            ch->block.value = ch->block.latch;
            ch_update_phase_generators(this, ch);
            return; }
        case 0xA4:
        case 0xA5:
        case 0xA6:
            ch->f_num.latch = (val & 7) << 8;
            ch->block.latch = (val >> 3) & 7;
            return;

        case 0xA8:
        case 0xA9:
        case 0xAA: {
            u32 chi = bch + 2;
            u32 oi = 0;
            switch(this->io.addr) {
                case 0xA8:
                    oi = 2;
                    break;
                case 0xA9:
                    oi = 0;
                    break;
                case 0xAA:
                    oi = 1;
                    break;
            }
            ch = &this->channel[chi];
            struct YM2612_OPERATOR *op = &ch->operator[oi];
            op->f_num.value = op->f_num.latch | val;
            op->block.value = op->block.latch;
            if (ch->mode == YFM_multiple)
                ch_update_phase_generators(this, ch);
            return; }
        case 0xAC:
        case 0xAD:
        case 0xAE: {
            u32 chi = bch + 2;
            u32 oi = 0;
            switch(this->io.addr) {
                case 0xA8:
                    oi = 2;
                    break;
                case 0xA9:
                    oi = 0;
                    break;
                case 0xAA:
                    oi = 1;
                    break;
            }
            ch = &this->channel[chi];
            struct YM2612_OPERATOR *op = &ch->operator[oi];
            op->f_num.latch = (val & 7) << 8;
            op->block.latch = (val >> 3) & 7;

            return; }

        case 0xB0:
        case 0xB1:
        case 0xB2: {
            ch->algorithm = val & 7;
            ch->feedback = (val >> 3) & 7;
            return; }

        case 0xB4:
        case 0xB5:
        case 0xB6:
            ch->pms = val & 7;
            ch->ams = (val >> 4) & 3;
            ch->right_enable = (val >> 6) & 1;
            ch->left_enable = (val >> 7) & 1;
            return;
    }
}

static void write_op_reg(struct ym2612 *this, u8 val, u32 bch)
{
    u32 offset = this->io.addr & 3;
    if (offset == 3) return;

    u32 chn = bch + offset;
    u32 opn = ((this->io.addr & 8) >> 3) | ((this->io.addr & 4) >> 1);
    struct YM2612_CHANNEL *ch = &this->channel[chn];
    struct YM2612_OPERATOR *op = &ch->operator[opn];
    switch(this->io.addr >> 4) {
        case 3:
            op->phase.multiple = val & 15;
            op->phase.detune = (val >> 4) & 7;
            return;
        case 4:
            op->envelope.total_level = val & 0x7F;
            return;
        case 5:
            op->envelope.attack_rate = val & 0x1F;
            op_update_key_scale(this, ch, op, val >> 6);
            return;
        case 6:
            op->envelope.decay_rate = val & 0x1F;
            op->am_enable = (val >> 7) & 1;
            return;
        case 7:
            op->envelope.sustain_rate = val & 0x1F;
            return;
        case 8:
            op->envelope.release_rate = val & 15;
            op->envelope.sustain_level = val >> 4;
            return;
        case 9:
            op->envelope.ssg.hold = val & 1;
            op->envelope.ssg.alternate = (val >> 1) & 1;
            op->envelope.ssg.attack = (val >> 2) & 1;
            op->envelope.ssg.enabled = (val >> 3) & 1;
            return;
    }
}

static void write_group1(struct ym2612* this, u8 val)
{
    this->status.busy_until = (*this->master_cycle_count) + 32;
    switch(this->io.addr) {
        case 0x22:
            lfo_enable(this, (val >> 3) & 1);
            lfo_freq(this, val & 7);
            return;
        case 0x24: // TMRA upper 8
            this->timer_a.period = (this->timer_a.period & 3) | (val << 2);
            return;
        case 0x25: // //TMRA lower 2
            this->timer_a.period = (this->timer_a.period & 0x3FC) | (val & 3);
            return;
        case 0x26:
            this->timer_b.period = val;
            return;
        case 0x27: {
            if(!this->timer_a.enable && (val & 1)) this->timer_a.counter = this->timer_a.period;
            if(!this->timer_b.enable && (val & 2)) {
                this->timer_b.counter = this->timer_b.period;
                this->timer_b.divider = 0;
            }

            this->timer_a.enable = (val & 1);
            this->timer_b.enable = (val >> 1) & 1;
            this->timer_a.irq = (val >> 2) & 1;
            this->timer_b.irq = (val >> 3) & 1;

            if(val & 0x10) this->timer_a.line = 0;
            if(val & 0x20) this->timer_b.line = 0;

            this->io.csm_enabled = (val >> 6) == 2;

            struct YM2612_CHANNEL *ch = &this->channel[2];
            ch->mode = (val & 0xC0) ? YFM_multiple : YFM_single;
            ch_update_phase_generators(this, ch);

            return; }
        case 0x28: {
            u32 bchn = (val & 4) ? 3 : 0;
            u32 offset = val & 3;
            if (offset < 3) {
                u32 chn = bchn + offset;
                struct YM2612_CHANNEL *ch = &this->channel[bchn + offset];
                op_key_on(this, ch, 0, (val >> 4) & 1);
                op_key_on(this, ch, 1, (val >> 5) & 1);
                op_key_on(this, ch, 2, (val >> 6) & 1);
                op_key_on(this, ch, 3, (val >> 7) & 1);
            }
            return; }
        case 0x2A:
            this->dac.sample = ((i32)val - 128) << 6;
            return;
        case 0x2B:
            this->dac.enable = (val >> 7) & 1;
            return;
    }
    if ((this->io.addr >= 0x30) && (this->io.addr < 0xA0)) {
        write_op_reg(this, val, 0);
        return;
    }
    else if ((this->io.addr >= 0xA0) && (this->io.addr < 0xC0)) {
        write_ch_reg(this, val, 0);
        return;
    }

    printf("\nWRITE UNKNOWN YM2612 REG grp:0 addr:%d val:%02x", this->io.addr, val);
}

static void write_group2(struct ym2612* this, u8 val)
{
    this->status.busy_until = (*this->master_cycle_count) + 32;

    if ((this->io.addr >= 0x30) && (this->io.addr < 0xA0)) {
        write_op_reg(this, val, 3);
        return;
    }
    else if ((this->io.addr >= 0xA0) && (this->io.addr < 0xC0)) {
        write_ch_reg(this, val, 3);
        return;
    }

    printf("\nWRITE UNKNOWN YM2612 REG grp:1 addr:%d val:%02x", this->io.addr, val);
}

void ym2612_write(struct ym2612*this, u32 addr, u8 val)
{
    /*$4000: Address port + Set group 1 flag
    $4002: Address port + Set group 2 flag
    $4001 / $4003: Data port*/
    switch(addr & 3) {
        case 1:
        case 3:
            if (this->io.group == 1) write_group2(this, val);
            else write_group1(this, val);
            return;
        case 0:
            write_addr(this, val, 0);
            return;
        case 2:
            write_addr(this, val, 1);
            return;
    }
}

u8 ym2612_read(struct ym2612*this, u32 addr, u32 old, u32 has_effect)
{
    addr &= 3;
    u32 v = 0;
    if (addr == 0) {
        v = ((*this->master_cycle_count) < this->status.busy_until) << 7;
        v |= this->status.timer_b_overflow << 1;
        v |= this->status.timer_a_overflow;
    }
    return v;
}

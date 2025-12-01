//
// Created by . on 10/25/24.
//

#include <cstdlib>
#include <cstdio>

#ifdef _MSC_VER
#define _USE_MATH_DEFINES // for M_PI
#endif
#include <cmath>

#include <cassert>

#include "helpers/debug.h"
#include "helpers/serialize/serialize.h"
#include "ym2612.h"

namespace YM2612 {

#define SIGNe14to32(x) (((((x) >> 13) & 1) * 0xFFFFC000) | ((x) & 0x3FFF))

#ifndef MIN
#define MIN(a,b) ((a) < (b) ? (a) : (b))
#endif

#ifndef MAX
#define MAX(a,b) ((a) > (b) ? (a) : (b))
#endif

static u16 sine[0x200];
static u16 pow2[0x100];
static constexpr i32 detune_table[32][4] = {
        {0,  0,  1,  2},  {0,  0,  1,  2},  {0,  0,  1,  2},  {0,  0,  1,  2},  // Block 0
        {0,  1,  2,  2},  {0,  1,  2,  3},  {0,  1,  2,  3},  {0,  1,  2,  3},  // Block 1
        {0,  1,  2,  4},  {0,  1,  3,  4},  {0,  1,  3,  4},  {0,  1,  3,  5},  // Block 2
        {0,  2,  4,  5},  {0,  2,  4,  6},  {0,  2,  4,  6},  {0,  2,  5,  7},  // Block 3
        {0,  2,  5,  8},  {0,  3,  6,  8},  {0,  3,  6,  9},  {0,  3,  7, 10},  // Block 4
        {0,  4,  8, 11},  {0,  4,  8, 12},  {0,  4,  9, 13},  {0,  5, 10, 14},  // Block 5
        {0,  5, 11, 16},  {0,  6, 12, 17},  {0,  6, 13, 19},  {0,  7, 14, 20},  // Block 6
        {0,  8, 16, 22},  {0,  8, 16, 22},  {0,  8, 16, 22},  {0,  8, 16, 22},  // Block 7
};

static constexpr i32 envelope_rates[64][8] = {
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

static bool math_done = false;

void do_math()
{
    math_done = true;
    for (u32 mn = 0; mn < 0x200; mn++) {
        u32 i = ((mn & 0x100) ? (~mn) : mn) & 0xFF;

        double n = (double)((i << 1) | 1);
        double s = sin((n / 512.0 * M_PI / 2.0));

        // The table stores attenuation values, but on a log2 scale instead of log10
        double attenuation = -log2(s);

        // Table contains 12-bit values that represent 4.8 fixed-point
        sine[mn] = static_cast<u16>(round((attenuation * 256.0)));
    }
    for (u32 i = 0; i < 256; i++) {
        // Table index N represents exponent of -(N+1)/256
        double exponent = -static_cast<double>(i + 1) / 256.0;
        double value = pow(2.0, exponent);

        // Convert to 0.11 fixed-point decimal
        pow2[i] = static_cast<i32>(static_cast<u16>(round((value * 2048.0))));
    }
}

core::core(YM2612::variant in_variant, u64 *master_cycle_count_in, u64 master_wait_cycles_in) :
    variant(in_variant),
    channel{CHANNEL(this), CHANNEL(this), CHANNEL(this), CHANNEL(this), CHANNEL(this), CHANNEL(this)},
    master_cycle_count(master_cycle_count_in),
    master_wait_cycles(master_wait_cycles_in) {
    if (!math_done) do_math();

    for (u32 i = 0; i < 6; i++) {
        CHANNEL *ch = &channel[i];
        ch->num = i;
        ch->left_enable = ch->right_enable = 1;
        ch->block.value = i < 3 ? 0 : 1;
        for (u32 j = 0; j < 4; j++) {
            OPERATOR *op = &ch->op[j];
            op->num = j;
            op->ch = ch;
            op->envelope.attenuation = 0x3FF;
        }
    }
    dac.sample = 0;
}


void OPERATOR::key_on(u32 val) {
    if (val) {
        //if (op->envelope.state == EP_release) {
        if ((!keyon) || (envelope.state == EP_release)) {
            keyon = 1;
            phase.counter = 0;
            u32 rate = (2 * envelope.attack_rate) + envelope.key_scale_rate;
            //if (ch->num == 1) printf("\n%d.%d KEYON! RATE:%d", ch->num, num, rate);

            if (rate >= 62) {
                //if (ch->num==1) printf("\nIMMEDIATE DECAY!");
                envelope.state = EP_decay;
                envelope.attenuation = 0;
            } else {
                //if (ch->num==1) printf("\nNOPE WERE IN ATTACK!");
                envelope.state = EP_attack;
            }

            envelope.ssg.invert_output = 0;
        }
    } else {
        if (envelope.ssg.enabled && (envelope.state != EP_release) && (envelope.ssg.invert_output != envelope.ssg.attack)) {
            envelope.attenuation =  (0x200 - envelope.attenuation) & 0x3FF;
        }
        envelope.state = EP_release;
        keyon = 0;
    }
}

void core::reset() {
    printf("\nwarn YM2612 reset not done");
}

void LFO::enable(bool enabled_in)
{
    enabled = enabled_in;
    if (!enabled) counter = 0;
}

void LFO::freq(u8 val)
{
    static const int dividers[8] = { 108, 77, 71, 67, 62, 44, 8, 5 };
    period = dividers[val];
}

void LFO::run()
{
    divider++;
    if (divider >= period) {
        divider = 0;

        if (enabled) {
            counter = (counter + 1) & 0x7F;
        }
    }
}

u16 LFO::fm(const u8 lfo_counter, const u8 pms, const u16 f_num)
{
    if (pms == 0) return f_num << 1;
    const u32 fm_table_idx = (lfo_counter & 0x20) ? (0x1F - (lfo_counter & 0x1F)) >> 2 : (lfo_counter & 0x1F) >> 2;

    static constexpr u16 tabl[8][8] = { {0, 0, 0, 0, 0, 0, 0, 0},
                                    {0, 0, 0, 0, 4, 4, 4, 4},
                                    {0, 0, 0, 4, 4, 4, 8, 8},
                                    {0, 0, 4, 4, 8, 8, 12, 12},
                                    {0, 0, 4, 8, 8, 8, 12, 16},
                                    {0, 0, 8, 12, 16, 16, 20, 24},
                                    {0, 0, 16, 24, 32, 32, 40, 48},
                                    {0, 0, 32, 48, 64, 64, 80, 96},
                                    };


    const u32 raw_increment = tabl[pms][fm_table_idx];
    u16 fm_increment = 0;
    for (int i = 4; i < 11; i++) {
        const u32 bit = (f_num >> i) & 1;
        fm_increment += bit * (raw_increment >> (10 - i));
    }

    if (lfo_counter & 0x40)
        return ((f_num << 1) - fm_increment) & 0xFFF;
    else
        return ((f_num << 1) + fm_increment) & 0xFFF;
}

u16 LFO::am(const u8 lfo_counter, const u8 ams)
{
    u16 ama = (lfo_counter & 0x40) ? (lfo_counter & 0x3F) : (0x3F - lfo_counter);
    ama <<= 1;

    switch(ams) {
        case 0: return 0;
        case 1: return ama >> 3;
        case 2: return ama >> 1;
        case 3: return ama;
        default:
            NOGOHERE;
    }
}

void core::mix_sample()
{
    i64 left = 0, right = 0;
    for (u32 i = 0; i < 6; i++) {
        CHANNEL &ch = channel[i];
        if (ch.ext_enable) {
            const i32 smp = ch.output;
            if (ch.left_enable) left += smp;
            if (ch.right_enable) right += smp;
        }
    }
    //left >>= 2;
    //right >>= 2;

    mix.left_output = static_cast<i32>((left + mix.filter.sample.left) * 0x250C + mix.filter.output.left + 0xB5E8) >> 16;
    mix.filter.sample.left = left;
    mix.filter.output.left = mix.left_output;

    mix.right_output = static_cast<i32>((right + mix.filter.sample.right) * 0x250C + mix.filter.output.right + 0xB5E8) >> 16;
    mix.filter.sample.right = right;
    mix.filter.output.right = mix.right_output;

    mix.mono_output = (left + right) >> 2;
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
           | ((f11 && (f10 || f9 || f8)) || (!f11 && f10 && f9 && f8));
}

i32 core::phase_compute_increment(const OPERATOR &op) const {
    const u32 mfn = lfo.fm(lfo.counter, op.ch->pms, op.phase.f_num);
    const u32 shifted = (mfn << op.phase.block) >> 2;

    const u32 key_code = scale_key_code(op.phase.f_num, op.phase.block);
    const u32 detune_mag = op.phase.detune & 3;
    const u32 detune_inc_mag = detune_table[key_code][detune_mag];
    const u32 detuned_f = ((op.phase.detune & 4) ? shifted - detune_inc_mag : shifted + detune_inc_mag) & 0x1FFFF;
    if (!op.phase.multiple) return detuned_f >> 1;
    return detuned_f * op.phase.multiple;
}

void OPERATOR::run_phase()
{
    i32 inc = ch->ym2612->phase_compute_increment(*this);
    phase.counter = (phase.counter + inc) & 0xFFFFF;
    phase.output = phase.counter >> 10;
}

void OPERATOR::run_env_ssg()
{
    if (envelope.attenuation < 0x200) return;

    if (envelope.ssg.alternate) {
        if (envelope.ssg.hold) {
            envelope.ssg.invert_output = 1;
        }
        else {
            envelope.ssg.invert_output ^= 1;
        }
    }
    else if (!envelope.ssg.hold) {
        phase.counter = 0;
    }

    if (!envelope.ssg.hold && ((envelope.state == EP_decay) || (envelope.state == EP_sustain))) {
        if (((2 * envelope.attack_rate) + envelope.key_scale_rate) >= 62) {
            envelope.attenuation = 0;
            envelope.state = EP_decay;
        }
        else {
            envelope.state = EP_attack;
        }
    }
    else if ((envelope.state == EP_release) || ((envelope.state != EP_attack) && (envelope.ssg.invert_output == envelope.ssg.attack))) {
        envelope.attenuation = 0x3FF;
    }
}

void OPERATOR::run_env()
{
    if (envelope.ssg.enabled) {
        run_env_ssg();
    }

    if (!envelope.divider) {
        envelope.divider = 3;
        envelope.cycle_count++;
        envelope.cycle_count = (envelope.cycle_count & 0xFFF) + (envelope.cycle_count >> 12);

        u32 sustain_level = envelope.sustain_level == 15 ? 0x3E0 : (envelope.sustain_level << 5);
        if (envelope.state == EP_attack && envelope.attenuation == 0) {
            envelope.state = EP_decay;
        }
        if (envelope.state == EP_decay && envelope.attenuation >= sustain_level) {
            envelope.state = EP_sustain;
        }

        u32 r;
        switch(envelope.state) {
            case EP_attack: r = envelope.attack_rate; break;
            case EP_decay: r = envelope.decay_rate; break;
            case EP_sustain: r = envelope.sustain_rate; break;
            case EP_release: r = (envelope.release_rate << 1) + 1; break;
        }

        i32 rate = r == 0 ? 0 : (2 * r) + envelope.key_scale_rate;
        if (rate > 63) rate = 63;

        i32 update_f_shift = 11 - (rate >> 2);
        if (update_f_shift < 0) update_f_shift = 0;
        if ((envelope.cycle_count & ((1 << update_f_shift) - 1)) == 0) {
            const u32 idx = (envelope.cycle_count >> update_f_shift) & 7;
            const u16 increment = envelope_rates[rate][idx];
            switch(envelope.state) {
                case EP_attack: {
                    //i32 old_a = envelope.attenuation;
                    envelope.attenuation += ((~envelope.attenuation) * increment) >> 4;
                    if (envelope.attenuation < 0) envelope.attenuation = 0;
                    if (envelope.attenuation > 0x3FF) envelope.attenuation = 0x3FF;
                    //if (ch->num==1)
                        //printf("\nATT %d.%d IN:%03x OUT:%03x", ch->num, num, old_a, envelope.attenuation);
                    break; }
                case EP_release:
                case EP_sustain:
                case EP_decay: {
                    /*if (envelope.ssg.enabled) {
                        if (envelope.attenuation < 0x200) {
                            u32 cmp = envelope.attenuation + (4 * increment);
                            envelope.attenuation = MIN(0x3FF, cmp);
                        }
                    }
                    else {*/
                        //envelope.attenuation = MIN(0x3FF, envelope.attenuation + increment);

                    //i32 old_a = envelope.attenuation;
                    envelope.attenuation += increment;
                    if (envelope.attenuation < 0) envelope.attenuation = 0;
                    if (envelope.attenuation > 0x3FF) envelope.attenuation = 0x3FF;
                    //if (ch.num==1)
                        //printf("\nRDS %d.%d IN:%03x OUT:%03x", ch->num, num, old_a, envelope.attenuation);
                    //}
                    break; }
            }
        }
    }
    envelope.divider = envelope.divider - 1;
}

static u16 attenuation_to_amplitude(u16 attenuation)
{
    int int_part = (attenuation >> 8) & 0x1F;
    if (int_part > 12) return 0;
    u32 fp2 = pow2[attenuation & 0xFF];
    return (fp2 << 2) >> int_part;
}

u16 OPERATOR::attenuation() const {
    u16 mattenuation;
    if (envelope.ssg.enabled && (envelope.state != EP_release) && (envelope.ssg.invert_output != envelope.ssg.attack)) {
        mattenuation = (0x200 - envelope.attenuation) & 0x3FF;
    }
    else {
        mattenuation = envelope.attenuation;
    }

    mattenuation += envelope.total_level << 3;
    return MIN(0x3FF, mattenuation);
}

i16 OPERATOR::run_output(i32 mod_input)
{
    u16 nphase = (phase.output + ((mod_input & 0xFFFFFFFE) >> 1)) & 0x3FF;

    u32 sign = (nphase >> 9) & 1;
    i32 sine_attenuation = sine[nphase & 0x1FF];
    i32 env_attenuation = attenuation();

    i32 env_am_attenuation;
    if (am_enable) {
        i32 amat = ch->ym2612->lfo.am(lfo_counter, ams);
        env_am_attenuation = (env_attenuation + amat);
        if (env_am_attenuation < 0) env_am_attenuation = 0;
        if (env_am_attenuation > 0x3FF) env_am_attenuation = 0x3FF;
    }
    else env_am_attenuation = env_attenuation;

    i32 amplitude = attenuation_to_amplitude(sine_attenuation + (env_am_attenuation << 2)) & 0x7FFF;
    //prev_output = output;
    output = sign ? (-amplitude) : amplitude;
    return output;
}

void CHANNEL::run()
{
    for (u32 opn = 0; opn < 4; opn++) {
        OPERATOR &mop = op[opn];
        mop.run_phase();
        mop.run_env();

        mop.lfo_counter = ym2612->lfo.counter;
        mop.ams = ams;
    }
    // Update phases
    i32 input0 = 0;
    if (feedback) {
        input0 = (op0_prior[0] + op0_prior[1]) >> (10 - feedback);
    }

    // Apply algorithm
    i32 out=0;
    switch(algorithm) {
        case 0: // S0 -> S1 -> S2 -> S3. // but they calculate 0-2-1-3
            op[0].run_output(input0); // run 0
            op[2].run_output(op[1].output); // run 2 with OLD 1
            op[1].run_output(op[0].output); // run 1 with new 1
            out = op[3].run_output(op[2].output); // run 3 with current 2
            break;
        case 1: // (S0 + S1) -> S2 -> S3.   1 3 2 4
            op[2].run_output(op[0].output + op[1].output);
            op[0].run_output(input0);
            op[1].run_output(0);
            out = op[3].run_output(op[2].output);
            break;
        case 2: // S0 + (S1 -> S2) -> S3
            op[0].run_output(input0); // S1
            op[2].run_output(op[1].output);
            op[1].run_output(0);
            out = op[3].run_output(op[0].output + op[2].output); // S4
            break;
        case 3: // (S0 -> S1) + S2 -> S3
            op[0].run_output(input0);
            op[2].run_output(0);
            out = op[3].run_output(op[1].output + op[2].output);
            op[1].run_output(op[0].output);
            break;
        case 4: // (S0->S1) + (S2->S3)
            op[0].run_output(input0);
            out = op[1].run_output(op[0].output);
            op[2].run_output(0);
            out += op[3].run_output(op[2].output);
            break;
        case 5: // S0 -> all(S1 + S2 + S3)
            out = op[2].run_output(op[0].output);
            op[0].run_output(input0);
            out += op[1].run_output(op[0].output);
            out += op[3].run_output(op[0].output);
            break;
        case 6: // (S0->S1) + S2 + S3
            op[0].run_output(input0);
            out = op[1].run_output(op[0].output);
            out += op[2].run_output(0);
            out += op[3].run_output(0);
            break;
        case 7: // add all 4
            out = op[0].run_output(input0);
            out += op[2].run_output(0);
            out += op[1].run_output(0);
            out += op[3].run_output(0);
            break;
        default:
            NOGOHERE
    }
    if (out < -8192) out = -8192;
    if (out > 8191) out = 8191;
    output = out;
    op0_prior[0] = op0_prior[1];
    op0_prior[1] = op[0].output;
}

u32 core::cycle_timers()
{
    u32 retval = 0;
    if (timer_a.enable) {
        timer_a.counter = (timer_a.counter + 1) & 0x3FF; // 10 bits
        if (!timer_a.counter) {
            retval = 1;
            timer_a.counter = timer_a.period;
            timer_a.line |= timer_a.irq;
        }
    }

    timer_b.divider = (timer_b.divider + 1) & 15;
    if (!timer_b.divider) {
        if (timer_b.enable) {
            timer_b.counter = (timer_b.counter + 1) & 0xFF;
            if (!timer_b.counter) {
                timer_b.counter = timer_b.period;
                timer_b.line |= timer_b.irq;
            }
        }
    }
    return retval;
}

void core::cycle()
{
    lfo.run();
    u32 a_overflowed = cycle_timers();

    if (io.csm_enabled && a_overflowed && false) {
        // Man this chip is odd at times
        printf("\nCSM WHAT?");
        CHANNEL &ch = channel[2];
        for (u32 opn = 0; opn < 4; opn++) {
            OPERATOR &op = ch.op[opn];
            if (op.envelope.state == EP_release) {
                op.key_on(1);
                op.key_on(0);
            }
        }
    }

    channel[0].run();
    channel[1].run();
    channel[2].run();
    channel[3].run();
    channel[4].run();
    channel[5].run();
    if (dac.enable) channel[5].output = dac.sample;
    mix_sample();
}

i16 core::sample_channel(const u32 ch) const {
    // Return an i16
    return channel[ch].output;
}

void core::serialize(serialized_state &m_state)
{
    printf("\nWARN YM2612 SERIALIZE NOT IMPLEMENT!");
}

void core::deserialize(serialized_state &m_state)
{
    printf("\nWARN YM2612 DESERIALIZE NOT IMPLEMENT!");
}

void OPERATOR::update_env_key_scale_rate(u16 f_num_in, u16 block_in)
{
    envelope.key_scale_rate = scale_key_code(f_num_in, block_in) >> (3 - envelope.key_scale);
}

void OPERATOR::update_freq(u16 f_num_in, u16 block_in)
{
    phase.f_num = f_num_in;
    phase.block = block_in;
    update_env_key_scale_rate(f_num_in, block_in);
}

void CHANNEL::update_phase_generators()
{
    if ((num == 2) && (mode ==  YFM_multiple)) {
        for (u32 opn = 0; opn < 3; opn++) {
            OPERATOR &mop = op[opn];
            mop.update_freq(mop.f_num.value, mop.block.value);
        }
        OPERATOR &mop = op[3];
        mop.update_freq(f_num.value, block.value);
        return;
    }
    for (u32 opn = 0; opn < 4; opn++) {
        OPERATOR &mop = op[opn];
        mop.update_freq(f_num.value, block.value);
    }
}

void OPERATOR::update_key_scale(u8 val)
{
    envelope.key_scale = val;
    update_env_key_scale_rate(phase.f_num, phase.block);
}

void core::write_ch_reg(u8 val, u32 bch)
{
    u32 chn = bch + (io.addr & 3);
    CHANNEL &ch = channel[chn];
    switch(io.addr) {
        case 0xA0:
        case 0xA1:
        case 0xA2: {
            ch.f_num.value = ch.f_num.latch | val;
            ch.block.value = ch.block.latch;
            ch.update_phase_generators();
            return; }
        case 0xA4:
        case 0xA5:
        case 0xA6:
            ch.f_num.latch = (val & 7) << 8;
            ch.block.latch = (val >> 3) & 7;
            return;

        case 0xA8:
        case 0xA9:
        case 0xAA: {
            u32 chi = bch + 2;
            u32 oi = 0;
            switch(io.addr) {
                case 0xA8:
                    oi = 2;
                    break;
                case 0xA9:
                    oi = 0;
                    break;
                case 0xAA:
                    oi = 1;
                    break;
                    default:
            }
            CHANNEL &che = channel[chi];
            OPERATOR &op = che.op[oi];
            op.f_num.value = op.f_num.latch | val;
            op.block.value = op.block.latch;
            if (che.mode == CHANNEL::YFM_multiple)
                che.update_phase_generators();
            return; }
        case 0xAC:
        case 0xAD:
        case 0xAE: {
            u32 chi = bch + 2;
            u32 oi = 0;
            switch(io.addr) {
                case 0xA8:
                    oi = 2;
                    break;
                case 0xA9:
                    oi = 0;
                    break;
                case 0xAA:
                    oi = 1;
                    break;
                default:
            }
            CHANNEL &che = channel[chi];
            OPERATOR &op = che.op[oi];
            op.f_num.latch = (val & 7) << 8;
            op.block.latch = (val >> 3) & 7;

            return; }

        case 0xB0:
        case 0xB1:
        case 0xB2: {
            ch.algorithm = val & 7;
            ch.feedback = (val >> 3) & 7;
            return; }

        case 0xB4:
        case 0xB5:
        case 0xB6:
            ch.pms = val & 7;
            ch.ams = (val >> 4) & 3;
            ch.right_enable = (val >> 6) & 1;
            ch.left_enable = (val >> 7) & 1;
            return;
        default:
    }
}

void core::write_op_reg(u8 val, u32 bch)
{
    u32 offset = io.addr & 3;
    if (offset == 3) return;

    u32 chn = bch + offset;
    u32 opn = ((io.addr & 8) >> 3) | ((io.addr & 4) >> 1);
    CHANNEL &ch = channel[chn];
    OPERATOR &op = ch.op[opn];
    switch(io.addr >> 4) {
        case 3:
            op.phase.multiple = val & 15;
            op.phase.detune = (val >> 4) & 7;
            return;
        case 4:
            op.envelope.total_level = val & 0x7F;
            return;
        case 5:
            op.envelope.attack_rate = val & 0x1F;
            op.update_key_scale(val >> 6);
            return;
        case 6:
            op.envelope.decay_rate = val & 0x1F;
            op.am_enable = (val >> 7) & 1;
            return;
        case 7:
            op.envelope.sustain_rate = val & 0x1F;
            return;
        case 8:
            op.envelope.release_rate = val & 15;
            op.envelope.sustain_level = val >> 4;
            return;
        case 9:
            op.envelope.ssg.hold = val & 1;
            op.envelope.ssg.alternate = (val >> 1) & 1;
            op.envelope.ssg.attack = (val >> 2) & 1;
            op.envelope.ssg.enabled = (val >> 3) & 1;
            return;
        default:
    }
}

void core::write_group1(u8 val)
{
    status.busy_until = (*master_cycle_count) + master_wait_cycles;
    switch(io.addr) {
        case 0x22:
            lfo.enable((val >> 3) & 1);
            lfo.freq(val & 7);
            return;
        case 0x24: // TMRA upper 8
            timer_a.period = (timer_a.period & 3) | (val << 2);
            return;
        case 0x25: // //TMRA lower 2
            timer_a.period = (timer_a.period & 0x3FC) | (val & 3);
            return;
        case 0x26:
            timer_b.period = val;
            return;
        case 0x27: {
            if(!timer_a.enable && (val & 1)) timer_a.counter = timer_a.period;
            if(!timer_b.enable && (val & 2)) {
                timer_b.counter = timer_b.period;
                timer_b.divider = 0;
            }

            timer_a.enable = (val & 1);
            timer_b.enable = (val >> 1) & 1;
            timer_a.irq = (val >> 2) & 1;
            timer_b.irq = (val >> 3) & 1;

            if(val & 0x10) timer_a.line = 0;
            if(val & 0x20) timer_b.line = 0;

            io.csm_enabled = (val >> 6) == 2;

            CHANNEL &ch = channel[2];
            ch.mode = (val & 0xC0) ? CHANNEL::YFM_multiple : CHANNEL::YFM_single;
            ch.update_phase_generators();
            return; }
        case 0x28: {
            const u32 bchn = (val & 4) ? 3 : 0;
            const u32 offset = val & 3;
            if (offset < 3) {
                const u32 chn = bchn + offset;
                CHANNEL &ch = channel[chn];
                ch.op[0].key_on((val >> 4) & 1);
                ch.op[1].key_on((val >> 5) & 1);
                ch.op[2].key_on((val >> 6) & 1);
                ch.op[3].key_on((val >> 7) & 1);
            }
            return; }
        case 0x2A:
            dac.sample = (static_cast<i32>(val) - 128) << 6;
            return;
        case 0x2B:
            dac.enable = (val >> 7) & 1;
            return;
    }
    if ((io.addr >= 0x30) && (io.addr < 0xA0)) {
        write_op_reg(val, 0);
        return;
    }
    else if ((io.addr >= 0xA0) && (io.addr < 0xC0)) {
        write_ch_reg(val, 0);
        return;
    }

    printf("\nWRITE UNKNOWN YM2612 REG grp:0 addr:%d val:%02x", io.addr, val);
}

void core::write_group2(u8 val)
{
    status.busy_until = (*master_cycle_count) + master_wait_cycles;

    if ((io.addr >= 0x30) && (io.addr < 0xA0)) {
        write_op_reg(val, 3);
        return;
    }
    else if ((io.addr >= 0xA0) && (io.addr < 0xC0)) {
        write_ch_reg(val, 3);
        return;
    }

    printf("\nWRITE UNKNOWN YM2612 REG grp:1 addr:%d val:%02x", io.addr, val);
}

void core::write(u32 addr, u8 val)
{
    /*$4000: Address port + Set group 1 flag
    $4002: Address port + Set group 2 flag
    $4001 / $4003: Data port*/
    switch(addr & 3) {
        case 1:
        case 3:
            if (io.group == 1) write_group2(val);
            else write_group1(val);
            return;
        case 0:
            io.group = 0;
            io.addr = val;
            return;
        case 2:
            io.group = 1;
            io.addr = val;
            return;
            default:
    }
}

u8 core::read(u32 addr, u32 old, bool has_effect) const {
    addr &= 3;
    u32 v = 0;
    if (addr == 0) {
        v = ((*master_cycle_count) < status.busy_until) << 7;
        v |= timer_b.line << 1;
        v |= timer_a.line;
    }
    return v;
}

}
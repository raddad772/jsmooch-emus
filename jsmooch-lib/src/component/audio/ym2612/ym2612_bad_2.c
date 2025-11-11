//
// Created by . on 10/25/24.
//

// This is a straight port of the Ares code, to help
//  me understand this cool beast.

#include <stdio.h>
#include <string.h>
#include <math.h>
#include <assert.h>

#include "helpers/debug.h"
#include "ym2612.h"

#define SIGNe14to32(x) (((((x) >> 13) & 1) * 0xFFFFC000) | ((x) & 0x3FFF))

#ifndef MIN
#define MIN(a,b) ((a) < (b) ? (a) : (b))
#endif

#ifndef MAX
#define MAX(a,b) ((a) > (b) ? (a) : (b))
#endif

static u16 sine[0x400];
static i16 pow2[0x200];
static const u8 lfo_dividers[8] = {108, 77, 71, 67, 62, 44, 8, 5};
static const u8 vibratos[8][16] = {{0, 0, 0, 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, 0, 0},
                                   {0, 0, 0, 0,  1,  1,  1,  1,  1,  1,  1,  1,  0,  0, 0, 0},
                                   {0, 0, 0, 1,  1,  1,  2,  2,  2,  2,  1,  1,  1,  0, 0, 0},
                                   {0, 0, 1, 1,  2,  2,  3,  3,  3,  3,  2,  2,  1,  1, 0, 0},
                                   {0, 0, 1, 2,  2,  2,  3,  4,  4,  3,  2,  2,  2,  1, 0, 0},
                                   {0, 0, 2, 3,  4,  4,  5,  6,  6,  5,  4,  4,  3,  2, 0, 0},
                                   {0, 0, 4, 6,  8,  8,  10, 12, 12, 10, 8,  8,  6,  4, 0, 0},
                                   {0, 0, 8, 12, 16, 16, 20, 24, 24, 20, 16, 16, 12, 8, 0, 0},
};
static const u8 tremolos[4] = {7, 3, 1, 0};
static const u8 detunes[3][8] = {{5,  6,  6,  7,  8,  8,  9,  10},
                                 {11, 12, 13, 14, 16, 17, 18, 20},
                                 {16, 17, 19, 20, 22, 24, 26, 28}};

struct env_rate {
    u32 divider;
    u32 steps[4];
};

static const struct env_rate envelope_rates[16] = {
        11, {0x00000000, 0x00000000, 0x01010101, 0x01010101},
        10, {0x01010101, 0x01010101, 0x01110111, 0x01110111},
        9, {0x01010101, 0x01011101, 0x01110111, 0x01111111},
        8, {0x01010101, 0x01011101, 0x01110111, 0x01111111},
        7, {0x01010101, 0x01011101, 0x01110111, 0x01111111},
        6, {0x01010101, 0x01011101, 0x01110111, 0x01111111},
        5, {0x01010101, 0x01011101, 0x01110111, 0x01111111},
        4, {0x01010101, 0x01011101, 0x01110111, 0x01111111},
        3, {0x01010101, 0x01011101, 0x01110111, 0x01111111},
        2, {0x01010101, 0x01011101, 0x01110111, 0x01111111},
        1, {0x01010101, 0x01011101, 0x01110111, 0x01111111},
        0, {0x01010101, 0x01011101, 0x01110111, 0x01111111},
        0, {0x11111111, 0x11121112, 0x12121212, 0x12221222},
        0, {0x22222222, 0x22242224, 0x24242424, 0x24442444},
        0, {0x44444444, 0x44484448, 0x48484848, 0x48884888},
        0, {0x88888888, 0x88888888, 0x88888888, 0x88888888},
};

static int math_done = 0;

static void op_update_phase(ym2612 *ym, YM2612_CHANNEL *ch, YM2612_OPERATOR *op)
{
    u32 v1max = MAX((u32)op->pitch.value, 0x300);
    u32 key = MIN(v1max, 0x4FF);
    u32 ksr = (op->octave.value << 2) + ((key - 0x300) >> 7);
    u32 tuning = op->detune & 3 ? detunes[(op->detune & 3) - 1][ksr & 7] >> (3 - (ksr >> 3)) : 0;

    u32 lfo = ym->lfo.clock >> 2 & 0x1f;
    s32 pm = (op->pitch.value * vibratos[ch->vibrato][lfo & 15] >> 9) * (lfo > 15 ? -1 : 1);

    op->phase.delta = (op->pitch.value + pm) << 6 >> (7 - op->octave.value);
    op->phase.delta = (!(op->detune & 4) ? op->phase.delta + tuning : op->phase.delta - tuning) & 0x1ffff;
    op->phase.delta = (op->multiple ? op->phase.delta * op->multiple : op->phase.delta >> 1) & 0xfffff;
}
#define op_update_envelope(a,b,c) op_update_envelope_f(a,b,c,0)

static void op_update_envelope_f(ym2612 *ym, YM2612_CHANNEL *ch, YM2612_OPERATOR *op, int debuggy)
{
    u32 v1max = MAX((u32)op->pitch.value, 0x300);
    u32 key = MIN(v1max, 0x4ff);
    u32 ksr = (op->octave.value << 2) + ((key - 0x300) >> 7);
    u32 rate = 0;

    if(op->envelope.state == EP_attack)  rate += (op->envelope.attack_rate  << 1);
    if(op->envelope.state == EP_decay)   rate += (op->envelope.decay_rate   << 1);
    if(op->envelope.state == EP_sustain) rate += (op->envelope.sustain_rate << 1);
    if(op->envelope.state == EP_release) rate += (op->envelope.release_rate << 1);

    rate += (ksr >> (3 - op->envelope.key_scale)) * (rate > 0);
    rate  = MIN(rate, 63);

    const struct env_rate *entry = &envelope_rates[rate >> 2];
    op->envelope.rate    = rate;
    op->envelope.divider = entry->divider;
    op->envelope.steps   = entry->steps[rate & 3];
}

static void op_update_level(ym2612 *ym, YM2612_CHANNEL *ch, YM2612_OPERATOR *op)
{
    u32 lfo = ym->lfo.clock & 0x40 ? ym->lfo.clock & 0x3f : ~ym->lfo.clock & 0x3f;
    u32 depth = tremolos[ch->tremolo];

    bool invert = op->ssg.attack != op->ssg.invert && op->envelope.state != EP_release;
    u16 value = (op->ssg.enable && invert ? 0x200 - op->envelope.value : 0 + op->envelope.value) & 0x3FF;

    op->output_level = ((op->total_level << 3) + value + (op->lfo_enable ? lfo << 1 >> depth : 0)) << 3;
}

static void op_update_pitch(ym2612 *ym, YM2612_CHANNEL *ch, YM2612_OPERATOR *op)
{
    op->pitch.value = ch->mode ? op->pitch.reload : ch->operator[3].pitch.reload;
    op->octave.value = ch->mode ? op->octave.reload : ch->operator[3].octave.reload;

    op_update_phase(ym, ch, op);
    op_update_envelope(ym, ch, op);
}

static void ch_power(ym2612 *ym, YM2612_CHANNEL *ch)
{
    ch->left_enable = ch->right_enable = 1;
    ch->algorithm = ch->feedback = ch->vibrato = ch->tremolo = 0;

    ch->mode = 0;

    for (u32 opi = 0; opi < 4; opi++) {
        struct YM2612_OPERATOR *op = &ch->operator[opi];
        memset(op, 0, sizeof(*op));
        op->output_level = 0x1FFF;
        op->envelope.state = EP_release;
        op->envelope.divider = 11;
        op->envelope.value = 0x3FF;
        op->envelope.release_rate = 1;

        op_update_pitch(ym, ch, op);
        op_update_level(ym, ch, op);
    }
}


static inline i32 wavef(YM2612_OPERATOR *op, u32 modulation)
{
    s32 x = (modulation >> 1) + (op->phase.value >> 10);
    s32 y = sine[x & 0x3ff] + op->output_level;
    return y < 0x1a00 ? pow2[y & 0x1ff] << 2 >> (y >> 9) : 0; // -78 dB floor
};

static void update_mix(ym2612* this)
{

#define wave(x,y) (wavef(&op(x), y))
#define old(n) (op(n).prior & modMask)
#define mod(n) (op(n).output & modMask)
#define out(n) (op(n).output & sumMask)
#define op(n) ch->operator[n]

    s32 left  = 0;
    s32 right = 0;
    s32 mono = 0;
    const s32 modMask = -(1 << 1);
    const s32 sumMask = -(1 << 5);
    const s32 outMask = -(1 << 5);

    for(u32 ch_num = 0; ch_num < 6; ch_num++) {
        struct YM2612_CHANNEL *ch = &this->channel[ch_num];
        
        s32 feedback = modMask & (op(0).prior + op(0).prior_buffer) >> (9 - ch->feedback);
        s32 accumulator = 0;

        op(0).prior_buffer = op(0).prior; // only need to buffer the output for feedback
        for (u32 n = 0; n < 4; n++) op(n).prior = op(n).output;

        op(0).output = wave(0, feedback * (ch->feedback > 0));

        // Note: Despite not emulating per cycle, operator pipelining has been accounted for.
        // Given the op order 0-2-1-3, newly generated output is not available for the following op.

        switch(ch->algorithm) {
            case 0:
                //0 -> 1 -> 2 -> 3
                op(1).output = wave(1, mod(0));
                op(2).output = wave(2, old(1));
                op(3).output = wave(3, mod(2));
                accumulator += out(3);
                break;
            case 1:
                //(0 + 1) -> 2 -> 3
                op(1).output = wave(1, 0);
                op(2).output = wave(2, old(0) + old(1));
                op(3).output = wave(3, mod(2));
                accumulator += out(3);
                break;
            case 2:
                //0 + (1 -> 2) -> 3
                op(1).output = wave(1, 0);
                op(2).output = wave(2, old(1));
                op(3).output = wave(3, mod(0) + mod(2));
                accumulator += out(3);
                break;
            case 3:
                //(0 -> 1) + 2 -> 3
                op(1).output = wave(1, mod(0));
                op(2).output = wave(2, 0);
                op(3).output = wave(3, old(1) + mod(2));
                accumulator += out(3);
                break;
            case 4:
                //(0 -> 1) + (2 -> 3)
                op(1).output = wave(1, mod(0));
                op(2).output = wave(2, 0);
                op(3).output = wave(3, mod(2));
                accumulator += out(1) + out(3);
                break;
            case 5:
                //0 -> (1 + 2 + 3)
                op(1).output = wave(1, mod(0));
                op(2).output = wave(2, old(0));
                op(3).output = wave(3, mod(0));
                accumulator += out(1) + out(2) + out(3);
                break;
            case 6:
                //(0 -> 1) + 2 + 3
                op(1).output = wave(1, mod(0));
                op(2).output = wave(2, 0);
                op(3).output = wave(3, 0);
                accumulator += out(1) + out(2) + out(3);
                break;
            case 7:
                //0 + 1 + 2 + 3
                op(1).output = wave(1, 0);
                op(2).output = wave(2, 0);
                op(3).output = wave(3, 0);
                accumulator += out(0) + out(1) + out(2) + out(3);
                break;
        }


        //s32 voiceData = sclamp<14>(accumulator) & outMask;
        if (accumulator < -0x1FFF) accumulator = -0x1FFF;
        if (accumulator > 0x1FFF) accumulator = 0x1FFF;
        i32 voiceData = SIGNe14to32(accumulator) & outMask;
        //if(this->dac.enable && (ch_num == 5)) voiceData = ((s32)this->dac.sample - 0x80) << 6;
        if(this->dac.enable && (ch_num == 5)) {
            //voiceData = ((i32)this->dac.sample - 0x80) << 6;
            voiceData = ((i32)(i8)(u8)this->dac.sample - 0x80) << 6;
            //voiceData = this->dac.sample << 6;
        }

        // DAC output + voltage offset (crossover distortion)
        if (ch->ext_enable) {
            if (this->variant == OPN2V_ym2612) {
/*                if (voiceData < 0) {
                    left += ch->left_enable * (voiceData + (1 << 5)) - (4 << 5);
                    right += ch->right_enable * (voiceData + (1 << 5)) - (4 << 5);
                } else {
                    left += ch->left_enable * (voiceData + (0 << 5)) + (4 << 5);
                    right += ch->right_enable * (voiceData + (0 << 5)) + (4 << 5);
                }*/
                left += voiceData;
                right += voiceData;
            }
            else {
                // DAC output (ym3438 fix)
                if (ch->left_enable ) left  += voiceData;
                if (ch->right_enable) right += voiceData;
            }
        }

        if (voiceData < -0x1FFF) voiceData = -0x1FFF;
        if (voiceData > 0x1FFF) voiceData = 0x1FFF;
        ch->ext_output.mono = voiceData;
        ch->ext_output.left = left;
        ch->ext_output.right = right;
    }
    mono = (left + right) >> 1;
    if (left < -0x7FFF) left = -0x7FFF;
    if (left > 0x7FFF) left = 0x7FFF;
    if (right < -0x7FFF) right = -0x7FFF;
    if (right > 0x7FFF) right = 0x7FFF;
    if (mono < -0x7FFF) mono = -0x7FFF;
    if (mono > 0x7FFF) mono = 0x7FFF;
    this->mix.left_output = SIGNe14to32(left);
    this->mix.right_output = SIGNe14to32(right);
    this->mix.mono_output = SIGNe14to32(mono);
#undef old
#undef mod
#undef out
#undef wave
}

void ym2612_reset(ym2612* this)
{
    for (u32 chi = 0; chi < 6; chi++) {
        ch_power(this, &this->channel[chi]);
    }
}

void ym2612_init(ym2612 *this, enum OPN2_variant variant, u64 *master_cycle_count)
{
    memset(this, 0, sizeof(*this));
    this->variant = variant;
    this->master_cycle_count = master_cycle_count;
    DBG_EVENT_VIEW_INIT;

    for (u32 i = 0; i < 6; i++) {
        this->channel[i].ext_enable = 1;
    }

    if (!math_done) {
        math_done = 1;
        const u32 positive = 0;
        const u32 negative = 1;
        for(s32 x = 0; x <= 0xff; x++) {
            i32 y = -256 * log(sin((2 * x + 1) * M_PI / 1024)) / log(2) + 0.5;
            sine[0x000 + x] = positive + (y << 1);
            sine[0x1ff - x] = positive + (y << 1);
            sine[0x200 + x] = negative + (y << 1);
            sine[0x3ff - x] = negative + (y << 1);
        }

        for(s32 y = 0; y <= 0xff; y++) {
            s32 z = 1024 * pow(2, (0xff - y) / 256.0) + 0.5;
            pow2[positive + (y << 1)] = +z;
            pow2[negative + (y << 1)] = ~z;  //not -z
        }
    }
    for (u32 i = 0; i < 6; i++) {
        this->channel[i].num = i;
        for (u32 j = 0; j < 4; j++) {
            this->channel[i].operator[j].num = j;
        }
    }

    ym2612_reset(this);
}

static void cycle_timers(ym2612* this)
{
    if (this->timer_a.enable) {
        this->timer_a.counter = (this->timer_a.counter + 1) & 0x3FF; // 10 bits
        if (!this->timer_a.counter) {

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
}

static void cycle_lfo(ym2612 *this)
{
    if (this->lfo.enable && (++this->lfo.divider >= lfo_dividers[this->lfo.rate])) {
        this->lfo.divider = 0;
        this->lfo.clock++;
        for (u32 ch_num = 0; ch_num < 6; ch_num++) {
            struct YM2612_CHANNEL *ch = &this->channel[ch_num];
            for (u32 op_num = 0; op_num < 4; op_num++) {
                op_update_phase(this, ch, &ch->operator[op_num]);
                op_update_level(this, ch, &ch->operator[op_num]);
            }
        }
    }
}

static void cycle_env(ym2612 *this)
{
    if (++this->envelope.divider >= 3) {
        this->envelope.divider = 0;
        this->envelope.clock = (this->envelope.clock + 1) & 0xFFF;
        if (this->envelope.clock == 0) this->envelope.clock = 1;
    }
}

static void op_update_key_state(ym2612 *ym, YM2612_CHANNEL *ch, YM2612_OPERATOR *op)
{
    if (op->key_on == op->key_line) return;  //no change
    op->key_on = op->key_line;

    if(op->key_on) {
        //restart phase and envelope generators
        op->phase.value = 0;
        op->ssg.invert = false;
        op->envelope.state = EP_attack;
        op_update_envelope_f(ym, ch, op, 1);

        if(op->envelope.rate >= 62) {
            //skip attack phase
            op->envelope.value = 0;
        }
    } else {
        op->envelope.state = EP_release;
        op_update_envelope(ym, ch, op);

        if(op->ssg.enable && op->ssg.attack != op->ssg.invert) {
            //SSG-EG key-off
            op->envelope.value = 0x200 - op->envelope.value;
        }
    }

    op_update_level(ym, ch, op);
}

static void op_run_phase(ym2612 *ym, YM2612_CHANNEL *ch, YM2612_OPERATOR *op)
{
    op_update_key_state(ym, ch, op);
    op->phase.value += op->phase.delta;  //advance wave position
    if(!(op->ssg.enable && op->envelope.value >= 0x200)) return;  //SSG loop check

    if(!op->ssg.hold && !op->ssg.alternate) op->phase.value = 0;
    if(!(op->ssg.hold && op->ssg.invert)) op->ssg.invert ^= op->ssg.alternate;

    if(op->envelope.state == EP_attack) {
        //do nothing; SSG is meant to skip the attack phase
    } else if(op->envelope.state != EP_release && !op->ssg.hold) {
        //if still looping, reset the envelope
        op->envelope.state = EP_attack;
        op_update_envelope(ym, ch, op);

        if(op->envelope.rate >= 62) {
            //skip attack phase
            op->envelope.value = 0;
        }
    } else if(op->envelope.state == EP_release || (op->ssg.hold && op->ssg.attack == op->ssg.invert)) {
        //clear envelope once finished
        op->envelope.value = 0x3ff;
    }

    op_update_level(ym, ch, op);
}

static void op_run_envelope(ym2612 *ym, YM2612_CHANNEL *ch, YM2612_OPERATOR *op)
{
    u32 value = ym->envelope.clock >> op->envelope.divider;
    u32 step = op->envelope.steps >> ((~value & 7) << 2) & 0xf;
    u32 sustain = op->envelope.sustain_level < 15 ? op->envelope.sustain_level << 5 : 0x1f << 5;

    if (op->envelope.state == EP_attack) {
        if(op->envelope.value == 0) {
            op->envelope.state = EP_decay;
            op_update_envelope(ym, ch, op);
        } else if(op->envelope.rate < 62) {
            // will stop updating if attack rate is increased to upper threshold during attack phase (confirmed behavior)
            op->envelope.value += ~((u16)op->envelope.value) * step >> 4;
        }
    }
    if(op->envelope.state != EP_attack) {
        if(op->envelope.state == EP_decay && op->envelope.value >= sustain) {
            op->envelope.state = EP_sustain;
            op_update_envelope(ym, ch, op);
        }
        if(op->ssg.enable) step = op->envelope.value < 0x200 ? step << 2 : 0;  //SSG results in a 4x faster envelope
        op->envelope.value = MIN(op->envelope.value + step, 0x3ff);
    }

    op_update_level(ym, ch, op);
}

i16 ym2612_sample_channel(ym2612* this, u32 ch)
{
    return (i16)this->channel[ch].ext_output.mono;
}

void ym2612_delete(ym2612 *this)
{

}

static void write_address(ym2612 *this, u32 val)
{
    this->io.address = val;
}

static void write_data(ym2612 *this, u32 val)
{
    switch(this->io.address) {

        //LFO
        case 0x022: {
            this->lfo.rate = val & 7;
            this->lfo.enable = (val >> 3) & 1;
            this->lfo.clock = 0;
            this->lfo.divider = 0;
            break;
        }

            //timer A period (high)
        case 0x024: {
            this->timer_a.period = (this->timer_a.period & 3) | (val << 2); 
            break;
        }

            //timer A period (low)
        case 0x025: {
            this->timer_a.period = (this->timer_a.period & 0x3FC) | (val & 3);
            break;
        }

            //timer B period
        case 0x026: {
            this->timer_b.period = val;
            break;
        }

            //timer control
        case 0x027: {
            //reload period on 0->1 transition
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

            //d7 is enable bit for CSM mode (not yet implemented)
            this->channel[2].mode = (val >> 6) & 3;
            for (u32 i = 0; i < 4; i++) {
                op_update_pitch(this, &this->channel[2], &this->channel[2].operator[i]);
            }
            break;
        }

            //key on/off
        case 0x28: {
            //0,1,2,4,5,6 => 0,1,2,3,4,5
            u32 index = val & 7;
            if(index == 3 || index == 7) break;
            if(index >= 4) index--;

            this->channel[index].operator[0].key_line = (val >> 4) & 1;
            this->channel[index].operator[1].key_line = (val >> 5) & 1;
            this->channel[index].operator[2].key_line = (val >> 6) & 1;
            this->channel[index].operator[3].key_line = (val >> 7) & 1;

            break;
        }

            //DAC sample
        case 0x2a: {
            //u64 diff = *this->master_cycle_count - this->last_master_cycle;
            //this->last_master_cycle = *this->master_cycle_count;
            this->dac.sample = val;
            break;
        }

            //DAC enable
        case 0x2b: {
            this->dac.enable = (val >> 7) & 1;
            break;
        }

    }

    if((this->io.address & 3) == 3) return;
    //u32 voice = this->io.address.bit(8) * 3 + this->io.address.bit(0,1);
    u32 voice = ((this->io.address >> 8) & 1) * 3 + (this->io.address & 3);

    //u32 index = this->io.address.bit(2,3) >> 1 | this->io.address.bit(2,3) << 1;  //0,1,2,3 => 0,2,1,3
    u32 index = ((this->io.address >> 2) & 3) >> 1 | ((this->io.address >> 2) & 3) << 1;  //0,1,2,3 => 0,2,1,3

    struct YM2612_CHANNEL *channel = &this->channel[voice];
    struct YM2612_OPERATOR *op = &channel->operator[index];

    switch(this->io.address & 0x0f0) {;

        //detune, multiple
        case 0x030: {
            op->multiple = val & 0x1F;
            op->detune = (val >> 4) & 7;
            op_update_phase(this, channel, op);
            break;
        }

            //total level
        case 0x040: {
            op->total_level = val & 0x3F;
            op_update_level(this, channel, op);
            break;
        }

            //key scaling, attack rate
        case 0x050: {
            op->envelope.attack_rate = val & 0x1F;
            op->envelope.key_scale = (val >> 6) & 3;
            op_update_envelope(this, channel, op);
            op_update_phase(this, channel, op);
            break;
        }

            //LFO enable, decay rate
        case 0x060: {
            op->envelope.decay_rate = val & 0x1F;
            op->lfo_enable = (val >> 7) & 1;
            op_update_envelope(this, channel, op);
            op_update_level(this, channel, op);
            break;
        }

            //sustain rate
        case 0x070: {
            op->envelope.sustain_rate = val & 0x1F;
            op_update_envelope(this, channel, op);
            break;
        }

            //sustain level, release rate
        case 0x080: {
            op->envelope.release_rate = (val & 15) << 1 | 1;
            op->envelope.sustain_level = (val >> 4) & 15;
            op_update_envelope(this, channel, op);
            break;
        }

            //SSG-EG
        case 0x090: {
            op->ssg.enable = (val >> 3) & 1;
            op->ssg.hold      = op->ssg.enable ? (val & 1) : 0;
            op->ssg.alternate = op->ssg.enable ? ((val >> 1) & 1) : 0;
            op->ssg.attack    = op->ssg.enable ? ((val >> 2) & 1) : 0;
            op_update_level(this, channel, op);
            break;
        }

    }

    switch(this->io.address & 0x0fc) {

        //pitch (low)
        case 0x0a0: {
            channel->operator[3].pitch.reload = channel->operator[3].pitch.latch | val;
            channel->operator[3].octave.reload = channel->operator[3].octave.latch;
            for (u32 i = 0; i < 4; i++) {
                op_update_pitch(this, channel, &channel->operator[i]);
            }
            break;
        }

            //pitch (high)
        case 0x0a4: {
            channel->operator[3].pitch.latch = val << 8;
            channel->operator[3].octave.latch = val >> 3;
            break;
        }

            //per-operator pitch (low)
        case 0x0a8: {
            if(this->io.address == 0x0a9) index = 0;
            if(this->io.address == 0x0aa) index = 1;
            if(this->io.address == 0x0a8) index = 2;
            this->channel[2].operator[index].pitch.reload = this->channel[2].operator[index].pitch.latch | val;
            this->channel[2].operator[index].octave.reload = this->channel[2].operator[index].octave.latch;
            op_update_phase(this, &this->channel[2], &this->channel[2].operator[index]);
            break;
        }

            //per-operator pitch (high)
        case 0x0ac: {
            if(this->io.address == 0x0ad) index = 0;
            if(this->io.address == 0x0ae) index = 1;
            if(this->io.address == 0x0ac) index = 2;
            this->channel[2].operator[index].pitch.latch = val << 8;
            this->channel[2].operator[index].octave.latch = val >> 3;
            break;
        }

            //algorithm, feedback
        case 0x0b0: {
            channel->algorithm = val & 7;
            channel->feedback = (val >> 3) & 7;
            break;
        }

            //panning, tremolo, vibrato
        case 0x0b4: {
            channel->vibrato = val & 7;
            channel->tremolo = (val >> 4) & 3;
            channel->right_enable = (val >> 6) & 1;
            channel->left_enable = (val >> 7) & 1;
            for(index = 0; index < 4; index++) {
                op_update_level(this, channel, &channel->operator[index]);
                op_update_phase(this, channel, &channel->operator[index]);
            }
            break;
        }

    }
}

void ym2612_write(ym2612 *this, u32 addr, u8 val)
{
    switch(0x4000 | (addr & 3)) {
        case 0x4000: return write_address(this, val);
        case 0x4001: return write_data(this, val);
        case 0x4002: return write_address(this, 0x100 | (u32)val);
        case 0x4003: return write_data(this, val);
    }
}

u8 ym2612_read(ym2612 *this, u32 addr, u32 old, u32 has_effect)
{
    return (this->timer_a.line << 0) | (this->timer_b.line << 1);
}


void ym2612_cycle(ym2612 *this)
{
    this->clock.div144++;

    if (this->clock.div144 >= 144) { // Samples are output at this rate.
        this->clock.div144 = 0;
        update_mix(this);
        cycle_timers(this);
        cycle_lfo(this);
        cycle_env(this);

        for (u32 ch_num = 0; ch_num < 6; ch_num++) {
            struct YM2612_CHANNEL *ch = &this->channel[ch_num];
            for (u32 i = 0; i < 4; i++) {
                op_run_phase(this, ch, &ch->operator[i]);
                if (this->envelope.divider) continue;
                if (this->envelope.clock & (1 << this->envelope.divider) - 1) continue;
                op_run_envelope(this, ch, &ch->operator[i]);
            }
        }
    }
}

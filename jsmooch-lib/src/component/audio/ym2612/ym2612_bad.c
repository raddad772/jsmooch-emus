//
// Created by . on 7/19/24.
//

// Some aspects of this rely heavily on forum posts by Nemesis,
//  others on Ares. I've tried to keep it as original as possible.
// I plan to redo it once I fully understand it.

#include <stdio.h>
#include <string.h>
#include <math.h>
#include <assert.h>

#include "helpers/debug.h"
#include "ym2612.h"


#ifndef MIN
#define MIN(a,b) ((a) < (b) ? (a) : (b))
#endif

#ifndef MAX
#define MAX(a,b) ((a) > (b) ? (a) : (b))
#endif


static void ch_update_pitches(struct ym2612* ym, YM2612_CHANNEL *ch);
static void op_update_level(struct ym2612* ym, YM2612_CHANNEL *ch, YM2612_OPERATOR *this);
static void env_update(struct ym2612_env *this, u16 pitch, u16 octave);
static void ch_update_pitch(struct ym2612* ym, YM2612_CHANNEL *ch, YM2612_OPERATOR *op);

static int init_tables = 0;

static u16 sine_table[256], pow_table[256];
static const u16 env_dividers[16] = {11, 10, 9, 8, 7, 6, 5, 4,3, 2, 1, 0, 0, 0, 0, 0};
static const u8 lfo_divider[8] = {108, 77, 71, 67, 62, 44, 8, 5};

// These tables were taken from Ares, thanks!
static u32 env_steps[64] = {
    0x00000000, 0x00000000, 0x01010101, 0x01010101,
    0x01010101, 0x01010101, 0x01110111, 0x01110111,
    0x01010101, 0x01011101, 0x01110111, 0x01111111,
    0x01010101, 0x01011101, 0x01110111, 0x01111111,
    0x01010101, 0x01011101, 0x01110111, 0x01111111,
    0x01010101, 0x01011101, 0x01110111, 0x01111111,
    0x01010101, 0x01011101, 0x01110111, 0x01111111,
    0x01010101, 0x01011101, 0x01110111, 0x01111111,
    0x01010101, 0x01011101, 0x01110111, 0x01111111,
    0x01010101, 0x01011101, 0x01110111, 0x01111111,
    0x01010101, 0x01011101, 0x01110111, 0x01111111,
    0x01010101, 0x01011101, 0x01110111, 0x01111111,
    0x11111111, 0x11121112, 0x12121212, 0x12221222,
    0x22222222, 0x22242224, 0x24242424, 0x24442444,
    0x44444444, 0x44484448, 0x48484848, 0x48884888,
    0x88888888, 0x88888888, 0x88888888, 0x88888888
};

static const u32 detunes[3][8] = { { 5,  6,  6,  7,  8,  8,  9, 10 }, { 11, 12, 13, 14, 16, 17, 18, 20 }, { 16, 17, 19, 20, 22, 24, 26, 28 }};
static const u32 vibratos[8][16] = {
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0},
    {0, 0, 0, 1, 1, 1, 2, 2, 2, 2, 1, 1, 1, 0, 0, 0}, {0, 0, 1, 1, 2, 2, 3, 3, 3, 3, 2, 2, 1, 1, 0, 0},
    {0, 0, 1, 2, 2, 2, 3, 4, 4, 3, 2, 2, 2, 1, 0, 0}, {0, 0, 2, 3, 4, 4, 5, 6, 6, 5, 4, 4, 3, 2, 0, 0},
    {0, 0, 4, 6, 8, 8,10,12,12,10, 8, 8, 6, 4, 0, 0}, {0, 0, 8,12,16,16,20,24,24,20,16,16,12, 8, 0, 0},
};

static void update_key_state(struct ym2612* ym, YM2612_CHANNEL *ch, YM2612_OPERATOR *op)
{
    if (op->key == op->key_on) return;
    op->key = op->key_on;

    if (op->key_on) {
        op->phase.value = 0;
        op->env.invert_xor = 0;
        op->env.env_phase = EP_attack;
        env_update(&op->env, op->pitch.value, op->octave.value);

        if (op->env.rate >= 62) {
            // skip attack phase
            op->env.attenuation = 0;
        }
    }
    else {
        op->env.env_phase = EP_release;
        env_update(&op->env, op->pitch.value, op->octave.value);

        if (op->env.ssg.enable && (op->env.ssg.attack != op->env.ssg.invert)) {
            op->env.attenuation = 0x200 - op->env.attenuation;
        }
    }

    op_update_level(ym, ch, op);
}

static void do_init_tables() {
    // using @Nemesis' code from https://gendev.spritesmind.net/forum/viewtopic.php?t=386&start=150
    //sinTableBitCount = 8
    //attenuationFixedBitCount = 8
    double l2 = log(2.0);
    for (unsigned int i = 0; i < 256; ++i) {
        //Calculate the normalized phase value for the input into the sine table. Note
        //that this is calculated as a normalized result from 0.0-1.0 where 0 is not
        //reached, because the phase is calculated as if it was a 9-bit index with the
        //LSB fixed to 1. This was done so that the sine table would be more accurate
        //when it was "mirrored" to create the negative oscillation of the wave. It's
        //also convenient we don't have to worry about a phase of 0, because 0 is an
        //invalid input for a log function, which we need to use below.
        double phaseNormalized = ((double) ((i << 1) + 1) / 512);

        //Calculate the pure sine value for the input. Note that we only build a sine
        //table for a quarter of the full oscillation (0-PI/2), since the upper two bits
        //of the full phase are extracted by the external circuit.
        double sinResultNormalized = sin(phaseNormalized * (M_PI / 2));

        //Convert the sine result from a linear representation of volume, to a
        //logarithmic representation of attenuation. The YM2612 stores values in the sine
        //table in this form because logarithms simplify multiplication down to addition,
        //and this allowed them to attenuate the sine result by the envelope generator
        //output simply by adding the two numbers together.
        double sinResultAsAttenuation = -log(sinResultNormalized) / l2;
        //The division by log(2) is required because the log function is base 10, but the
        //YM2612 uses a base 2 logarithmic value. Dividing the base 10 log result by
        //log10(2) will convert the result to a base 2 logarithmic value, which can then
        //be converted back to a linear value by a pow2 function. In other words:
        //2^(log10(x)/log10(2)) = 2^log2(x) = x
        //If there was a native log2() function provided we could use that instead.

        //Convert the attenuation value to a rounded 12-bit result in 4.8 fixed point
        //format.
        sine_table[i] = (u16) ((sinResultAsAttenuation * 256) + 0.5);
    }


    //powTableBitCount = 8
    //powTableOutputBitCount = 11
    for(unsigned int i = 0; i < 256; ++i)
    {
        //Normalize the current index to the range 0.0-1.0. Note that in this case, 0.0
        //is a value which is never actually reached, since we start from i+1. They only
        //did this to keep the result to an 11-bit output. It probably would have been
        //better to simply subtract 1 from every final number and have 1.0 as the input
        //limit instead when building the table, so an input of 0 would output 0x7FF,
        //but they didn't.
        double entryNormalized = (double)(i + 1) / 256.0;

        //Calculate 2^-entryNormalized
        double resultNormalized = pow(2, -entryNormalized);

        //Write the result to the table
        pow_table[i] = (u16)((resultNormalized * 2048.0) + 0.5);
    }
}

void ym2612_init(struct ym2612* this)
{
    memset(this, 0, sizeof(*this));
    if (!init_tables) {
        do_init_tables();
        init_tables = 1;
    }

    for (u32 i = 0; i < 6; i++) {
        this->channel[i].ext_enable = 1;
        //this->channel[i].output = 0x1FFF;
    }

    this->dac.sample = 0x80;
}

void ym2612_delete(struct ym2612* this)
{

}

static void write_addr(struct ym2612* this, u16 val)
{
    this->io.address = val;
}

static void op_update_phase(struct ym2612 *ym, YM2612_CHANNEL *ch, YM2612_OPERATOR *this)
{
    u32 key = MIN(MAX((u32)this->pitch.value, 0x300), 0x4ff);
    u32 ksr = (this->octave.value << 2) + ((key - 0x300) >> 7);
    u32 tuning = (this->detune & 3) ? detunes[(this->detune & 3) - 1][ksr & 7] >> (3 - (ksr >> 3)) : 0;

    u32 lfo = ym->lfo.clock >> 2 & 0x1f;
    i32 pm = (this->pitch.value * vibratos[ch->vibrato][lfo & 15] >> 9) * (lfo > 15 ? -1 : 1);

    this->phase.delta = ((this->pitch.value + pm) << 6) >> (7 - this->octave.value);
    this->phase.delta = (!(this->detune & 4) ? this->phase.delta + tuning : this->phase.delta - tuning) & 0x1FFFF;
    this->phase.delta = (this->multiple ? this->phase.delta * this->multiple : this->phase.delta >> 1) & 0xfffff;
}

static void write_data(struct ym2612* this, u8 val) {
    switch (this->io.address) {
        case 0x022:
            this->lfo.rate = val & 7;
            this->lfo.enable = (val >> 3) & 1;
            this->lfo.clock = 0;
            this->lfo.divider = 0;
            break;
        case 0x24: // Timer a hi
            this->timer_a.period = (this->timer_a.period & 3) | ((u16) val << 2);
            return;
        case 0x25: // Timer a lo
            this->timer_a.period = (this->timer_a.period & 0x3C) | (val & 3);
            return;
        case 0x26: // timer b period
            this->timer_b.period = val;
            return;
        case 0x27: // timer control
            if (!this->timer_a.enabled && (val & 1))
                this->timer_a.counter = this->timer_a.period;
            if (!this->timer_b.enabled && (val & 2)) {
                this->timer_b.counter = this->timer_b.period;
                this->timer_b.divider = 0;
            }
            this->timer_a.enabled = val & 1;
            this->timer_b.enabled = (val >> 1) & 1;
            this->timer_a.irq_enable = (val >> 2) & 1;
            this->timer_b.irq_enable = (val >> 3) & 1;

            if (val & 0x10) this->timer_a.line = 0;
            if (val & 0x20) this->timer_b.line = 0;

            this->channel[2].mode = (val >> 6) & 3;
            ch_update_pitches(this, &this->channel[2]);
            return;
        case 0x28: { // KEYON/OFF
            u32 ch_num = val & 7;
            if ((ch_num == 7) || (ch_num == 3)) break;
            if (ch_num >= 4) ch_num--;

            struct YM2612_CHANNEL *ch = &this->channel[ch_num];
            ch->operator[0].key_on = ch->operator[1].key_on = ch->operator[2].key_on = ch->operator[3].key_on = (val >> 4) & 1;
            break;
        }
        case 0x2a: // DAC sample
            this->dac.sample = val;
            break;
            //DAC enable
        case 0x2b:
            this->dac.enable = (val >> 7) & 1;
            break;
    }
    if ((this->io.address & 3) == 3) return;

    u32 ch_num = (3 * ((this->io.address >> 8) & 1)) + (this->io.address & 3);
    u32 op_num = (((this->io.address >> 2) & 3) >> 1) | ((this->io.address >> 1) & 0xC);

    struct YM2612_CHANNEL *ch = &this->channel[ch_num];
    struct YM2612_OPERATOR *op = &ch->operator[op_num];

    switch (this->io.address & 0xF0) {
        case 0x30:
            op->multiple = val & 15;
            op->detune = (val >> 4) & 7;
            op_update_phase(this, ch, op);
            break;
        case 0x40:
            op->total_level = val & 0x3F;
            op_update_level(this, ch, op);
            break;
        case 0x50:
            op->env.AR = val & 0x1F;
            op->env.key_scale = (val >> 6) & 3;
            env_update(&op->env, op->pitch.value, op->octave.value);
            op_update_phase(this, ch, op);
            break;
        case 0x60:
            op->env.DR = val & 0x1F;
            op->lfo_enable = (val >> 7) & 1;
            env_update(&op->env, op->pitch.value, op->octave.value);
            op_update_level(this, ch, op);
            break;
        case 0x70:
            op->env.SR = val & 0x1F;
            env_update(&op->env, op->pitch.value, op->octave.value);
            break;
        case 0x80:
            op->env.RR = val & 15;
            op->env.SL = (val >> 4) & 15;
            env_update(&op->env, op->pitch.value, op->octave.value);
            break;
        case 0x90: {
            u32 enable = op->env.ssg.enable = (val >> 3) & 1;
            op->env.ssg.hold = (val & 1) * enable;
            op->env.ssg.alt = ((val >> 1) & 1) * enable;
            op->env.ssg.attack = ((val >> 2) & 1) * enable;
            op_update_level(this, ch, op);
            break;
        }

    }

    struct YM2612_CHANNEL *ch2 = &this->channel[2];
    struct YM2612_OPERATOR *op3 = &ch->operator[3];
    struct YM2612_OPERATOR *mop = NULL;
    switch (this->io.address & 0xFC) {
        case 0xA0:
            op3->pitch.reload = op3->pitch.latch | val;
            op3->octave.reload = op3->octave.latch;
            ch_update_pitches(this, ch);
            break;
        case 0xA4:
            op3->pitch.latch = val << 8;
            op3->octave.latch = val >> 3;
            break;
        case 0xA8:
            switch (this->io.address) {
                case 0xA8:
                    op_num = 2;
                    break;
                case 0xA9:
                    op_num = 0;
                    break;
                case 0xAA:
                    op_num = 1;
                    break;
            }
            mop = &ch2->operator[op_num];
            mop->pitch.reload = mop->pitch.latch | val;
            mop->octave.reload = mop->octave.latch;
            ch_update_pitch(this, ch, op);
        case 0xAC:
            switch (this->io.address) {
                case 0xAD:
                    op_num = 0;
                    break;
                case 0xAC:
                    op_num = 2;
                    break;
                case 0xAE:
                    op_num = 1;
                    break;
            }
            mop = &ch2->operator[op_num];
            mop->pitch.latch = val << 8;
            mop->octave.latch = val >> 3;
            break;
        case 0xB0:
            ch->algorithm = val & 7;
            ch->feedback = (val >> 3) & 7;
            break;
        case 0xB4:
            ch->vibrato = val & 7;
            ch->tremolo = (val >> 4) & 3;
            ch->right_on = (val >> 6) & 1;
            ch->left_on = (val >> 7) & 1;
            for (u32 i = 0; i < 4; i++) {
                mop = &ch->operator[i];
                op_update_level(this, ch, mop);
                op_update_phase(this, ch, mop);
            }
            break;
    }

}

static u32 tremolo_table[4] = {7, 3, 1, 0};

static void op_update_level(struct ym2612 *ym, YM2612_CHANNEL *ch, YM2612_OPERATOR *this) {
    u32 invert_clock = (((ym->lfo.clock >> 6) & 1) ^ 1) * 0x3F;
    u32 lfo = (ym->lfo.clock ^ invert_clock) & 0x3F;

    u32 invert = (this->env.env_phase != EP_release) && (this->env.ssg.attack != this->env.ssg.invert);
    u32 value = this->env.attenuation;
    if (this->env.ssg.enable && invert) value = 0x200 - value;
    value &= 0x3FF;

    this->output_level = (this->total_level << 3) + value;
    if (this->lfo_enable) {
        u32 depth = tremolo_table[ch->tremolo];
        this->output_level += (lfo << 1) >> depth;
    }
    this->output_level = (this->output_level << 3) & 0x3FFF;
}

void ym2612_write(struct ym2612 *this, u32 addr, u8 val) {
    addr &= 3;
    switch (0x4000 | addr) {
        case 0x4000:
            return write_addr(this, 0x000 | val);
        case 0x4001:
            return write_data(this, val);
        case 0x4002:
            return write_addr(this, 0x100 | val);
        case 0x4003:
            return write_data(this, val);
    }
}

void ym2612_reset(struct ym2612 *this) {

}

u8 ym2612_read(struct ym2612 *this, u32 addr, u32 old, u32 has_effect) {
    return this->timer_a.line | (this->timer_b.line << 1);
}

static void tick_timers(struct ym2612 *this) {
    if (this->timer_a.enabled) {
        this->timer_a.counter = (this->timer_a.counter + 1) & 0x3FF;
        if (this->timer_a.counter == 0) {
            this->timer_a.counter = this->timer_a.period;
            this->timer_a.line |= this->timer_a.irq_enable;
        }
    }

    if (this->timer_b.enabled) {
        this->timer_b.divider = (this->timer_b.divider + 1) & 0x0F;
        if (this->timer_b.divider == 0) {
            this->timer_b.counter = (this->timer_b.counter + 1) & 0xFF;
            if (this->timer_b.counter == 0) {
                this->timer_b.counter = this->timer_b.period;
                this->timer_b.line |= this->timer_b.irq_enable;
            }
        }
    }
}

static void env_update(struct ym2612_env *this, u16 pitch, u16 octave) {

    // "The envelope generator performs the rate calculation once, as the operator enters a new phase of the ADSR envelope."
    u32 key = MIN(MAX((u32) pitch, 0x300), 0x4FF);
    u32 ksr = (octave << 2) + ((key - 0x300) >> 7);
    this->rate = 0;
    u16 v = 0;
    switch (this->env_phase) {
        case EP_attack:
            this->rate = this->AR;
            break;
        case EP_decay:
            this->rate = this->DR;
            break;
        case EP_sustain:
            this->rate = this->SR;
            break;
        case EP_release:
            this->rate = (this->RR << 1) + 1;
            break;
    }

    if (this->rate != 0) this->rate += (ksr >> (3 - this->key_scale)) * (this->rate > 0);
    if (this->rate > 63) this->rate = 63;

    this->divider.period = (1 << env_dividers[this->rate >> 2]) - 1;
    this->steps = env_steps[this->rate];
}

static void ch_update_pitch(struct ym2612* ym, YM2612_CHANNEL *ch, YM2612_OPERATOR *op) {
    struct YM2612_OPERATOR *op3 = &ch->operator[3];
    op->pitch.value = ch->mode ? op->pitch.reload : op3->pitch.reload;
    op->octave.value = ch->mode ? op->octave.reload : op3->octave.reload;

    op_update_phase(ym, ch, op);
    env_update(&op->env, op->pitch.value, op->octave.value);
}

static void ch_update_pitches(struct ym2612* ym, YM2612_CHANNEL *ch) {
    //only channel[2] allows per-operator frequencies
    //implemented by forcing mode to zero (single frequency) for other channels
    //in single frequency mode, operator[3] frequency is used for all operators
    for (u32 i = 0; i < 4; i++) {
        struct YM2612_OPERATOR *op = &ch->operator[i];
        ch_update_pitch(ym, ch, op);
    }
}


static void ch_keyon(struct ym2612 *ym, YM2612_CHANNEL *ch)
{
    for (u32 i = 0; i < 4; i++) {
        struct YM2612_OPERATOR *op = &ch->operator[i];
        op->env.env_phase = EP_attack;
        op->env.invert_xor = 0x3FF;
        env_update(&op->env, op->pitch.value, op->octave.value);
        op->env.attenuation = 0;
    }
}

static void ch_keyoff(struct ym2612 *ym, YM2612_CHANNEL *ch)
{
    for (u32 i = 0; i < 4; i++) {
        struct YM2612_OPERATOR *op = &ch->operator[i];
        op->env.env_phase = EP_release;
        op->env.invert_xor = 0;
        env_update(&op->env, op->pitch.value, op->octave.value);
    }
}

static void do_env_tick(struct ym2612 *ym, ym2612_env *this, u16 pitch, u16 octave)
{
    this->divider.counter = (this->divider.counter + 1);
    if (this->divider.counter >= this->divider.period) {
        this->divider.counter = 0;

        u32 shift = (7 - this->divider.phase) << 2; // * 4
        u32 attenuation_update = (this->steps >> shift) & 7;

        this->divider.phase = (this->divider.phase + 1) & 7;


        u32 sustain = this->SL < 15 ? this->SL << 5 : 0x1f << 5;

        switch(this->env_phase) {
            case EP_attack:
                if (this->attenuation == 0) {
                    this->env_phase = EP_decay;
                    env_update(this, pitch, octave);
                }
                else if (this->rate < 62) // A too-high rate will cause DEATH! I mean, no update
                    this->attenuation += attenuation_update * (((1024 - this->attenuation) >> 4) + 1);
                break;
            case EP_decay:
            case EP_sustain:
            case EP_release:
                if(this->env_phase == EP_decay && this->attenuation >= sustain) {
                    this->env_phase = EP_sustain;
                    env_update(this, pitch, octave);
                }
                if(this->ssg.enable) attenuation_update = this->attenuation < 0x200 ? attenuation_update << 2 : 0;
                this->attenuation += attenuation_update;
                break;
        }
        if (this->attenuation >= 0x3FF) this->attenuation = 0x3FF;
    }
}

#define BIT10 0x3FF

static u16 calculate_operator(u16 phase, u16 phase_modulation, u16 env_input)
{
    u16 output = 0;
    // 10-bit add of phase and phase modulation
    phase = (phase + phase_modulation) & BIT10;

    // Sepaarate sign bit (9), inversion (8) bit to convert to quarter-phase
    u16 sign_bit = (phase >> 9) & 1; // For application to bit13 later, 14 bits to XOR with
    u16 sign_xor = sign_bit * 0x3FFF;
    u16 invert_bit = ((phase >> 8) & 1) * 0xFF;

    u16 attenuation = sine_table[(phase & 0xFF) ^ invert_bit]; // Perform invasion and get sine value
    // attenuation is now 4.8 fixed point

    // add in envelope generator, we will then have 5.8 fixed point
    attenuation += (env_input << 2);

    u16 shift = (attenuation & 0x1F00) >> 8;

    u16 num = (pow_table[attenuation & 0xFF] << 2) >> shift; // now 13 bits
    return (num ^ sign_xor) | (sign_bit << 13);
}

static void tick_envs(struct ym2612 *this)
{
    for (u32 ch_num = 0; ch_num < 6; ch_num++) {
        for (u32 op_num = 0; op_num < 4; op_num++) {
            struct YM2612_OPERATOR *op = &this->channel[ch_num].operator[op_num];
            do_env_tick(this, &op->env, op->pitch.value, op->octave.value);
        }
    }
}

#define SIGNe14to32(x) (((((x) >> 13) & 1) * 0xFFFFC000) | ((x) & 0x3FFF))


static void update_mix(struct ym2612* this)
{
    i32 accumulator = 0;
    for (u32 i = 0; i < 6; i++) {
        struct YM2612_CHANNEL *ch = &this->channel[i];
        if (ch->ext_enable) accumulator += SIGNe14to32(ch->output);
    }
    accumulator = (accumulator * 4) / 6;
    // Now it is a value between -8192 and 8191, I believe
    assert(accumulator >= -32768);
    assert(accumulator <= 32767);

    // Strictly speaking, this isn't the way to do this, but oh well!
    // In the future we need to time-domain multiplex the output at 6x the sample rate
    this->output = accumulator;
}

i16 ym2612_sample_channel(struct ym2612* this, u32 ch)
{
    return (i16)((SIGNe14to32(this->channel[ch].ext_output.mono)) * 4);
}

#define op_old(n) (ch->operator[n].prev & 0xFFFFFFFD)
#define op_mod(n) (ch->operator[n].output_level & 0xFFFFFFFD)
#define op_out(n) (ch->operator[n].output_level & 0xFFFFFFDF)

static void tick_channels(struct ym2612* this)
{
    // Go through all the operators and synthesize th sound...
    struct YM2612_OPERATOR *op[4];
    for (u32 ch_num = 0; ch_num < 6; ch_num++) {
        struct YM2612_CHANNEL *ch = &this->channel[ch_num];
        i16 accumulator = 0;
        op[0] = &ch->operator[0];
        op[1] = &ch->operator[1];
        op[2] = &ch->operator[2];
        op[3] = &ch->operator[3];

        i32 feedback = 0; // later
        op[0]->buffer_prev = op[0]->prev;
        for (u32 i = 0; i < 4; i++) {
            op[i]->prev = op[i]->output_level;
        }

        op[0]->output_level = calculate_operator(op[0]->phase.value, 0, op[0]->env.attenuation);

        switch(ch->algorithm) {
            case 0:
#define opout(n) op[n]->output_level
#define ophase(n) op[n]->phase.value
#define openv(n) op[n]->env.attenuation
                // chain 0 > 1 > 2 > 3
                opout(1) = calculate_operator(ophase(0), op_mod(0), openv(1));
                opout(2) = calculate_operator(op_old(2), op_old(1), openv(2));
                opout(3) = calculate_operator(ophase(3), op_mod(2), openv(3));
                accumulator += op_out(3);
                break;
            case 1: // 0 + 1 > 2 > 3
                opout(1) = calculate_operator(ophase(1), 0, openv(1));
                opout(2) = calculate_operator(ophase(2), op_old(0) + op_old(1), openv(2));
                opout(3) = calculate_operator(ophase(3), op_mod(2), openv(3));
                accumulator += op_out(3);
                break;
            case 2: // 0+ (1 > 2) > 3
                opout(1) = calculate_operator(ophase(1), 0, openv(1));
                opout(2) = calculate_operator(ophase(2), op_old(1), openv(2));
                opout(3) = calculate_operator(ophase(3), op_mod(0) + op_mod(2), openv(3));
                accumulator += op_out(3);
                break;
            case 3: // (0 > 1) + 2 > 3
                opout(1) = calculate_operator(ophase(1), op_mod(0), openv(1));
                opout(2) = calculate_operator(ophase(2), 0, openv(2));
                opout(3) = calculate_operator(ophase(3), op_mod(1) + op_mod(2), openv(3));
                accumulator += op_out(3);
                break;
            case 4: // 0>1, 2>3, then add
                opout(1) = calculate_operator(ophase(1), op_mod(0), openv(1));
                opout(2) = calculate_operator(ophase(2), 0, openv(2));
                opout(3) = calculate_operator(ophase(3), op_mod(2), openv(3));
                accumulator += op_out(1) + op_out(3);
                break;
            case 5: // 0 > (1+2+3)
                opout(1) = calculate_operator(ophase(1), op_mod(0), openv(1));
                opout(2) = calculate_operator(ophase(2), op_old(0), openv(2));
                opout(3) = calculate_operator(ophase(3), op_mod(0), openv(3));
                accumulator += op_out(1) + op_out(2) + op_out(3);
                break;
            case 6: // (0 > 1) + 2 + 3
                opout(1) = calculate_operator(ophase(1), op_mod(0), openv(1));
                opout(2) = calculate_operator(ophase(2), 0, openv(2));
                opout(3) = calculate_operator(ophase(3), 0, openv(3));
                accumulator += op_out(1) + op_out(2) + op_out(3);
                break;
            case 7: // add all together
                opout(1) = calculate_operator(ophase(1), 0, openv(1));
                opout(2) = calculate_operator(ophase(2), 0, openv(2));
                opout(3) = calculate_operator(ophase(3), 0, openv(3));
                accumulator += op_out(0) + op_out(1) + op_out(2) + op_out(3);
                break;
        }

        // If we're channel 6, do DAC?
        i32 ch_output = SIGNe14to32(accumulator & 0xFFFFFFDF);
        if (this->dac.enable && (ch_num == 5)) ch_output = ((i32)this->dac.sample - 0x80) << 6;
        ch->output = (i16)ch_output;
    }
}

static void tick_lfo(struct ym2612* this)
{
    if (this->lfo.enable) {
        this->lfo.divider++;
        if (this->lfo.divider >= lfo_divider[this->lfo.divider]) {
            this->lfo.divider = 0;
            this->lfo.clock++;
            for (u32 ch_num = 0; ch_num < 6; ch_num++) {
                struct YM2612_CHANNEL *ch = &this->channel[ch_num];
                for (u32 op_num = 0; op_num < 4; op_num++) {
                    struct YM2612_OPERATOR *op = &ch->operator[op_num];
                    op_update_phase(this, ch, op);
                    op_update_level(this, ch, op);
                }
            }
        }
    }
}


void ym2612_cycle(struct ym2612* this)
{
    this->clock.div144++;
    this->clock.div24++;
    this->clock.env_divider++;

    if (this->clock.div144 >= 144) { // Samples are output at this rate.
        this->clock.div144 = 0;
        tick_channels(this);
        tick_timers(this);
        tick_lfo(this);
        update_mix(this);
    }
    /*if (this->clock.div24 >= 24) { // Samples are updated at this rate, one operator per tick
        this->clock.div24 = 24;
        //tick_channels(this);
        update_mix(this);
    }*/
    if (this->clock.env_divider >= 351) {
        this->clock.env_divider = 0;
        tick_envs(this);
    }
}

//genesis_z80_ym2612_read
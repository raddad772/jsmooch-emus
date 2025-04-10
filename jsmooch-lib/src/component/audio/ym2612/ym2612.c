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
        ch->block = i < 3 ? 0 : 1;
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
    this->io.addr =val;
    this->io.chn = val & 3;
    if (this->io.chn == 3) return;
    if (this->io.group == 1) {
        this->io.chn += 3;
    }

    this->io.opn = ((val >> 3) & 1) | ((val >> 1) & 2);
}

static void write_fnum_hi(struct ym2612 *this, struct YM2612_CHANNEL *ch, u8 val, u32 ch3_op)
{
    if (ch->num == 3 && this->io.ch3_special) {
        ch->operator[ch3_op].fnum.latch = (val & 7) << 8;
        ch->operator[ch3_op].block.latch = (val >> 3) & 7;
        return;
    }
    u32 fnum = (val & 7) << 8;
    u32 block = (val >> 3) & 7;
    for (u32 i = 0; i < 4; i++) {
        ch->operator[i].fnum.latch = fnum;
        ch->operator[i].block.latch = block;
    }
}

static u32 get_detune_lookup(u32 fnum, u32 block)
{
    u32 luval = block << 2;
    luval |= (fnum >> 10) & 2;
#define F(x) (fnum >> x)
    luval |= (F(11) & (F(10) | F(9) | F(8)) | ((~F(11)) & F(10) & F(9)) & F(8)) & 1;
#undef F
    return luval;
}


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

static inline i32 get_detune_delta(u32 fnum, u32 block, u32 dtune_idx)
{
    i32 delta = detune_table[get_detune_lookup(fnum, block)][dtune_idx & 3];
    return delta * ((((i32)dtune_idx  >> 2) & 1) * 2) - 1;
}

static inline void update_key_scale(struct ym2612 *this, struct YM2612_CHANNEL *ch, struct YM2612_OPERATOR *op)
{
    u32 idx = get_detune_lookup(op->fnum.reload, op->block.reload);
    op->envelope.rks = (i32)idx >> (3 - (i32)op->envelope.key_scale);
}

static inline i32 calculate_rate(struct YM2612_ENV *e)
{
    i32 R;
    switch(e->state) {
        case EP_attack:
            R = e->attack_rate;
            break;
        case EP_decay:
            R = e->decay_rate;
            break;
        case EP_sustain:
            R = e->sustain_rate;
            break;
        case EP_release:
            R = e->release_rate;
            break;
    }
    i32 rate = 0;
    if (R) {
        rate = (R * 2) + e->rks;
    }
    if (rate > 63) rate = 63;
    return rate < 0 ? 0 : rate;
}


static inline void op_key_on(struct ym2612 *this, struct YM2612_CHANNEL *ch, struct YM2612_OPERATOR *op, u32 val)
{
    if (op->key_on == val) return;
    op->key_on = val;
    if (op->key_on) { // KEYON
        op->phase.value = 0;

        //change phase to attack
        op->envelope.state = EP_attack;

        // Rate 62, 63 immediately trip to decay
        u32 attack_rate = calculate_rate(&op->envelope);

        // if in attack and attenuation=0, go to decay
        if (op->envelope.level == 0) {
            op->envelope.state = EP_decay;
        }

        // when in decay if attenuation >= sustain_level, go to sustain
        if ((op->envelope.state == EP_decay) && (op->envelope.level >= op->envelope.sustain_level)) {
            op->envelope.state = EP_sustain;
        }

        if (attack_rate >= 62) {
            op->envelope.level = 0;
        }
    }
    else { // KEYOFF
        // only envelope cares about this

        // Change phase to release
        op->envelope.state = EP_release;
    }
}

static void write_fnum_lo(struct ym2612 *this, struct YM2612_CHANNEL *ch, u8 val, u32 ch3_op)
{
    if (ch->num == 3 && this->io.ch3_special) {
        ch->operator[ch3_op].fnum.reload = ch->operator[3].fnum.latch | val;
        ch->operator[ch3_op].block.reload = ch->operator[3].block.latch;
        ch->operator[ch3_op].phase.delta = (ch->operator[ch3_op].fnum.reload << ch->operator[ch3_op].block.reload) >> 1;
        ch->operator[ch3_op].detune_delta = get_detune_delta(ch->operator[ch3_op].fnum.reload, ch->operator[ch3_op].block.reload, ch->operator[ch3_op].detune);
        update_key_scale(this, ch, &ch->operator[ch3_op]);
        return;
    }
    u32 fnum = ch->operator[0].fnum.latch | val;
    u32 block = ch->operator[0].block.latch;
    u32 delta = (ch->operator[0].fnum.reload << ch->operator[0].block.reload) >> 1;
    i32 detune_delta = get_detune_delta(ch->operator[0].fnum.reload, ch->operator[0].block.reload, ch->operator[0].detune);
    for (u32 i = 0; i < 4; i++) {
        ch->operator[i].fnum.reload = fnum;
        ch->operator[i].block.reload = block;
        ch->operator[i].phase.delta = delta;
        ch->operator[i].detune_delta = detune_delta;
        update_key_scale(this, ch, &ch->operator[i]);
    }
}

static void write_data(struct ym2612 *this, u8 val)
{
    struct YM2612_CHANNEL *ch = &this->channel[this->io.chn];
    struct YM2612_OPERATOR *op = &ch->operator[this->io.opn];
    this->status.busy_until = (*this->master_cycle_count) + (32 * 42);
    static int a[256] = {};
    switch(this->io.addr) {
        case 0x24: // TMRA upper 8
            this->timer_a.period = (this->timer_a.period & 3) | (val << 2);
            return;
        case 0x25: // //TMRA lower 2
            this->timer_a.period = (this->timer_a.period & 0x3FC) | (val & 3);
            return;
        case 0x26:
            this->timer_b.period = val;
            return;
        case 0x27:
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

            this->io.ch3_special = (val & 0xC0) != 0;
            static int ab = 1;
            if (ab) {
                printf("\nWARN CH3 SPECIAL ENABLE!");
                ab = 0;
            }
            return;
        case 0x28: {// KEYON
            u32 chn = val & 7;
            if ((chn & 3) == 3) {
                printf("\nWARN KEYON BAD CHANNEL!?");
                return;
            }
            if (chn > 2) chn--;
            val >>= 4;
            for (u32 i = 0; i < 4; i++) {
                op_key_on(this, &this->channel[chn], &this->channel[chn].operator[i], val & 1);
                val >>= 1;
            }
            return; }
        case 0x2A: // dac PCM output
            this->dac.sample = (i32)val - 0x80; // 8bit
            //this->dac.sample = SIGNe8to32(this->dac.sample) << 6; // 8 - > 14bit
            this->dac.sample <<= 6;
            //if (this->dac.enable) this->channel[5].output = this->dac.sample;
            this->status.busy_until = 0;
            return;
        case 0x2B: // bit 7 is DAC enable
            // TODO: more here
            this->dac.enable = (val >> 7) & 1;
            return;

        case 0x30:
        case 0x31:
        case 0x32:
        case 0x34:
        case 0x35:
        case 0x36:
        case 0x38:
        case 0x39:
        case 0x3A:
        case 0x3C:
        case 0x3D:
        case 0x3E: // detune/multiple
            op->multiple.val = val & 15;
            if (op->multiple.val == 0) {
                op->multiple.rshift = 1;
                op->multiple.multiplier = 1;
            }
            else {
                op->multiple.rshift = 0;
                op->multiple.multiplier = op->multiple.val;
            }
            op->detune = (val >> 4) & 7;
            op->detune_delta = get_detune_delta(op->fnum.reload, op->block.reload, op->detune);
            return;

        case 0x40:
        case 0x41:
        case 0x42:
        case 0x44:
        case 0x45:
        case 0x46:
        case 0x48:
        case 0x49:
        case 0x4A:
        case 0x4C:
        case 0x4D:
        case 0x4E: // "total level"
            op->envelope.total_level = (val & 0x7F) << 3;
            return;

        case 0x50:
        case 0x51:
        case 0x52:
        case 0x54:
        case 0x55:
        case 0x56:
        case 0x58:
        case 0x59:
        case 0x5A:
        case 0x5C:
        case 0x5D:
        case 0x5E: // attack and key scale
            op->envelope.attack_rate = val & 0x1F;
            op->envelope.key_scale = (val >> 6) & 3;
            update_key_scale(this, ch, op);
            return;

        case 0x60:
        case 0x61:
        case 0x62:
        case 0x64:
        case 0x65:
        case 0x66:
        case 0x68:
        case 0x69:
        case 0x6A:
        case 0x6C:
        case 0x6D:
        case 0x6E: // decay and LFO AM
            op->envelope.decay_rate = val & 0x1F;
            op->lfo_enable = (val >> 7) & 1;
            return;

        case 0x70:
        case 0x71:
        case 0x72:
        case 0x74:
        case 0x75:
        case 0x76:
        case 0x78:
        case 0x79:
        case 0x7A:
        case 0x7C:
        case 0x7D:
        case 0x7E: // "sustain"
            op->envelope.sustain_rate = val & 0x1F;
            return;

        case 0x80:
        case 0x81:
        case 0x82:
        case 0x84:
        case 0x85:
        case 0x86:
        case 0x88:
        case 0x89:
        case 0x8A:
        case 0x8C:
        case 0x8D:
        case 0x8E: // "release" and sustain level
            op->envelope.release_rate = ((val & 0xF) << 1) + 1;
            u32 s = (val >> 4) & 0xF;
            if (s == 15) s = 0x3E0;
            else s <<= 5;
            op->envelope.sustain_level = s;
            return;

        case 0x90:
        case 0x91:
        case 0x92:
        case 0x94:
        case 0x95:
        case 0x96:
        case 0x98:
        case 0x99:
        case 0x9A:
        case 0x9C:
        case 0x9D:
        case 0x9E: // "SSG-EG" stuff
            if (val != 0) {
                static int ra = 1;
                if (ra) {
                    printf("\nWRAN SSG-EG MODE USE");
                    ra = 0;
                }
            }
            return;


        case 0xA0:
        case 0xA1:
        case 0xA2: // lowest 8 bits of f-num, and also latch f-num and block
            write_fnum_lo(this, ch, val, 3);
            return;

        case 0xA9: // ch3 op0 lo
            if (this->io.ch3_special)
                write_fnum_lo(this, &this->channel[3], val, 0);
            return;
        case 0xAD: // ch3 op0 hi
            if (this->io.ch3_special)
                write_fnum_hi(this, &this->channel[3], val, 0);
            return;

        case 0xAA: // ch3 op1 lo
            if (this->io.ch3_special)
                write_fnum_lo(this, &this->channel[3], val, 1);
            return;
        case 0xAE: // ch3 op1 hi
            if (this->io.ch3_special)
                write_fnum_hi(this, &this->channel[3], val, 1);
            return;

        case 0xA8: // ch3 op2 lo
            if (this->io.ch3_special)
                write_fnum_lo(this, &this->channel[3], val, 2);
            return;
        case 0xAC: // ch3 op2 hi
            if (this->io.ch3_special)
                write_fnum_hi(this, &this->channel[3], val, 2);
            return;

        case 0xA4:
        case 0xA5:
        case 0xA6: // highest 3 bits of f-num, also block
            write_fnum_hi(this, ch, val, 3);
            return;

        case 0xB0:
        case 0xB1:
        case 0xB2: // algorithm and op1 feedback
            ch->algorithm = val & 7;
            ch->feedback = (val >> 3) & 7;
            return;

        case 0xB4:
        case 0xB5:
        case 0xB6: // L/R panning for ch3/6
            ch->pms = val & 7;
            ch->ams = (val >> 4) & 3;
            ch->right_enable = (val >> 6) & 1;
            ch->left_enable = (val >> 7) & 1;
            // TODO: more here
            return;
        default:
            /*if (!a[this->io.addr]) {
                a[this->io.addr] = 1;
                //printf("\nWARN WRITE TO UNSUPPORTED YM2612 PORT ADDR:%02x GRP:%d", this->io.addr, this->io.group);
            }*/
            return;
    }
}


void ym2612_write(struct ym2612*this, u32 addr, u8 val)
{
    /*$4000: Address port + Set group 1 flag
    $4002: Address port + Set group 2 flag
    $4001 / $4003: Data port*/
    switch(addr & 3) {
        case 1:
        case 3:
            write_data(this, val);
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

void ym2612_reset(struct ym2612*this)
{

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

static void run_op_phase(struct ym2612 *this, struct YM2612_CHANNEL *ch, struct YM2612_OPERATOR *op)
{
    // Get delta with detune applied. 17bits
    u32 inc = (op->phase.delta + op->detune_delta) & 0x1FFFF;

    // Now apply multiple
    inc = (inc >> op->multiple.rshift) * op->multiple.multiplier; // up to 20bits

    op->phase.value = (op->phase.value + inc) & 0xFFFFF;
}

static i32 calc_op(struct YM2612_CHANNEL *ch, u32 opn, u32 input)
{
    struct YM2612_OPERATOR *op = &ch->operator[opn];
    u32 mphase = op->phase.value >> 10;
    u32 phase = (input + mphase) & 0x3FF;
    return phase;
}

static i32 run_op_output(struct YM2612_CHANNEL *ch, u32 opn, u32 phase_input)
{
    struct YM2612_OPERATOR *op = &ch->operator[opn];
    phase_input = (phase_input >> 1) & 0x3FF;
    u32 mphase = op->phase.value >> 10;
    u32 phase = (phase_input + mphase) & 0x3FF;
    i32 sn = (((i32)((phase >> 9) & 1) * 2) - 1) * -1;
    u32 table_idx = phase & 0x1FF;
    u32 attenuation = sine[table_idx] + (op->envelope.attenuation << 2);

    u32 fract_part = attenuation & 0xFF;
    u32 int_part = attenuation >> 8;
    if (int_part > 255) printf("\nWHAT!? %d", int_part);
    i32 output_level;
    if (int_part >= 13)
        output_level = 0;
    else
        output_level = (pow2[fract_part] << 2) >> int_part; // unsigned 13-bit PCM
    output_level *= sn; // signed 14-bit PCM
    op->output = op->key_on ? output_level : 0;
    return op->output;
}

static void run_ch(struct ym2612 *this, u32 chn)
{
    struct YM2612_CHANNEL *ch = &this->channel[chn];
    // Update phases
    i32 input0 = 0;
    if (ch->feedback) {
        input0 = (ch->op0_prior[0] + ch->op0_prior[0]) >> (10 - ch->feedback);
    }
    i32 input, input2;

    for (u32 opn = 0; opn < 4; opn++) {
        struct YM2612_OPERATOR *op = &ch->operator[opn];
        run_op_phase(this, ch, op);
        op->phase.input = 0;
    }

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

static inline void run_env(struct ym2612 *this, struct YM2612_CHANNEL *ch, struct YM2612_OPERATOR *op)
{
    struct YM2612_ENV *e = &op->envelope;
    if (e->state == EP_attack && e->level == 0) {
        e->state = EP_decay;
    }

    if (e->state == EP_decay && e->level >= e->sustain_level) {
        e->state = EP_sustain;
    }

    i32 rate = calculate_rate(e);
    i32 shift = 11 - (rate >> 2);
    if ((this->status.env_cycle_counter & ((1 << shift) - 1)) == 0) {
        u32 idx = (this->status.env_cycle_counter >> shift) & 7;
        i32 increment = env_inc_table[rate][idx];
        i32 current = e->level;

        switch(e->state) {
            case EP_attack:
                // A' = A + ((I * !A) >> 4)
                current = current + ((increment * -(current + 1)) >> 4);
                break;
            case EP_decay:
                current -= increment;
                break;
            case EP_sustain:
            case EP_release:
                current += increment;
                break;
        }
        if (current > 0x3FF) current = 0x3FF;
        if (current < 0) current = 0;
        e->level = current;
        e->attenuation = e->level + e->total_level;
        if (e->attenuation > 0x3FF) e->attenuation = 0x3FF;
    }


}

static void run_envs(struct ym2612 *this) {
    this->status.env_cycle_counter++;
    if (this->status.env_cycle_counter >= 0x1000) {
        this->status.env_cycle_counter = 1;
    }
    for (u32 chn = 0; chn < 6; chn++) {
        struct YM2612_CHANNEL *ch = &this->channel[chn];
        for (u32 opn = 0; opn < 4; opn++) {
            struct YM2612_OPERATOR *op = &ch->operator[opn];
            run_env(this, ch, op);
        }
    }
}

static void cycle_timers(struct ym2612* this)
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


void ym2612_cycle(struct ym2612*this)
{
    // When we get here, we already have /6. we need to /24 to get /144
    /*if (this->status.busy_for_how_long) {
        this->status.busy_for_how_long--;
    }*/

    /*this->clock.div24++;
    if (this->clock.div24 >= 24) {
        this->clock.div24 = 0;*/
        this->clock.div24_3 = (this->clock.div24_3 + 1) % 3;
        if (this->clock.div24_3 == 0) {
            run_envs(this);
        }


        run_ch(this, 0);
        run_ch(this, 1);
        run_ch(this, 2);
        run_ch(this, 3);
        run_ch(this, 4);
        run_ch(this, 5);
        if (this->dac.enable) this->channel[5].output = this->dac.sample;
        mix_sample(this);

        cycle_timers(this);
    //}
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

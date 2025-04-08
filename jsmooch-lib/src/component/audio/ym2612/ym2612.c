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

void ym2612_init(struct ym2612 *this, enum OPN2_variant variant, u64 *master_cycle_count)
{
    memset(this, 0, sizeof(*this));
    DBG_EVENT_VIEW_INIT;

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
}

void ym2612_delete(struct ym2612*this)
{

}

static void write_addr(struct ym2612 *this, u32 val, u32 hi)
{
    this->io.group = hi;
    this->io.addr =val;
    printf("\nYM2612 WRITE addr:%d grp:%d", this->io.addr, hi);
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

static inline void op_key_on(struct ym2612 *this, struct YM2612_CHANNEL *ch, struct YM2612_OPERATOR *op, u32 val)
{
    if (op->key_on == val) return;
    op->key_on = val;
    if (op->key_on) { // KEYON
        op->phase.value = 0;
        op->phase.output = 0;
        printf("\nKEYON ch%d op%d", ch->num, op->num);
    }
    else { // KEYOFF
        // only envelope cares about this
    }
}

static void ch_key_on(struct ym2612 *this, struct YM2612_CHANNEL *ch, u32 v)
{
    for (u32 i = 0; i < 4; i++) {
        struct YM2612_OPERATOR *op = &ch->operator[i];
        op_key_on(this, ch, op, v);
    }
}

static void write_fnum_lo(struct ym2612 *this, struct YM2612_CHANNEL *ch, u8 val, u32 ch3_op)
{
    if (ch->num == 3 && this->io.ch3_special) {
        ch->operator[ch3_op].fnum.reload = ch->operator[3].fnum.latch | val;
        ch->operator[ch3_op].block.reload = ch->operator[3].block.latch;
        ch->operator[ch3_op].phase.delta = (ch->operator[ch3_op].fnum.reload << ch->operator[ch3_op].block.reload) >> 1;
        ch->operator[ch3_op].detune_delta = get_detune_delta(ch->operator[ch3_op].fnum.reload, ch->operator[ch3_op].block.reload, ch->operator[ch3_op].detune);

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
    }
}

static void write_data(struct ym2612 *this, u8 val)
{
    struct YM2612_CHANNEL *ch = &this->channel[this->io.chn];
    struct YM2612_OPERATOR *op = &ch->operator[this->io.opn];
    printf("\nYM2612 WRITE DATA val:%02x", val);
    this->status.busy_for_how_long = 32;
    static int a[256] = {};
    switch(this->io.addr) {
        case 0x27:
            // TODO: more?
            this->io.ch3_special = (val & 0xC0) != 0;
            static int ab = 1;
            if (ab) {
                printf("\nWARN CH3 SPECIAL ENABLE!");
                ab = 0;
            }
            return;
        case 0x28: {// KEYON
            /*
000 = Channel 1
001 = Channel 2
010 = Channel 3
100 = Channel 4
101 = Channel 5
110 = Channel 6             */

            // 0 = ch.0
            // 1 = ch.1
            // 2 = ch.2
            // 3 = invalid
            // 4 = ch.3
            // 5 = ch.4
            // 6 = ch.5
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
            this->dac.sample = ((i32)val - 128);
            this->dac.sample = SIGNe8to32(this->dac.sample) << 6;
            if (this->dac.enable) this->channel[5].output = this->dac.sample << 2;
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
            if (!a[this->io.addr]) {
                a[this->io.addr] = 1;
                printf("\nWARN WRITE TO UNSUPPORTED YM2612 PORT ADDR:%02x GRP:%d", this->io.addr, this->io.group);
            }
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
        v = (this->status.busy_for_how_long > 0) << 7;
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
            i32 smp = ym2612_sample_channel(this, i);
            if (ch->left_enable) left += smp;
            if (ch->right_enable) right += smp;
        }
    }
    // 14-bit samples, 6 of them though, so we are at 17 bits, so shift right by 1
    this->mix.left_output = left >> 1;
    this->mix.right_output = right >> 1;
    this->mix.mono_output = (left + right) >> 2;
}

static void run_op(struct ym2612 *this, struct YM2612_CHANNEL *ch, struct YM2612_OPERATOR *op)
{
    // Get delta with detune applied. 17bits
    u32 inc = (op->phase.delta + op->detune_delta) & 0x1FFFF;

    // Now apply multiple
    inc = (inc >> op->multiple.rshift) * op->multiple.multiplier; // up to 20bits

    op->phase.value = (op->phase.value + inc) & 0xFFFFF;

    op->phase.output = op->phase.value >> 10;
}

static void run_ch(struct ym2612 *this, u32 chn)
{
    struct YM2612_CHANNEL *ch = &this->channel[chn];
    for (u32 opn = 0; opn < 4; opn++) {
        struct YM2612_OPERATOR *op = &ch->operator[opn];
        run_op(this, ch, op);
    }

    struct YM2612_OPERATOR *c = &ch->operator[3];
    u32 phase = c->phase.output;

    // OK now set our output to op3 carrier basically
// Phase is a 10-bit integer; convert to [0, 2*PI)
    double mphase = ((double)phase) / ((double)(1 << 10)) * M_2_PI;

    // Compute sine and use as channel output
    // -1 to 1
    double output = (sin(mphase) + 1.0); // 0...2
    output *= 32767.5;
    output -= 32768;
    ch->output = (i32)round(output);
}

void ym2612_cycle(struct ym2612*this)
{
    // When we get here, we already have /6. we need to /24 to get /144
    if (this->status.busy_for_how_long) {
        this->status.busy_for_how_long--;
    }

    this->clock.div24++;
    if (this->clock.div24 >= 24) {
        this->clock.div24 = 0;
        run_ch(this, 0);
        run_ch(this, 1);
        run_ch(this, 2);
        run_ch(this, 3);
        run_ch(this, 4);
        run_ch(this, 5);
        if (this->dac.enable) this->channel[5].output = this->dac.sample << 2;
        mix_sample(this);
    }

}

i16 ym2612_sample_channel(struct ym2612*this, u32 chn)
{
    // Return an i16
    if ((chn == 5) && (this->dac.enable)) {
        return this->dac.sample << 2;
    }
    struct YM2612_CHANNEL *ch = &this->channel[chn];

    struct YM2612_OPERATOR *c = &ch->operator[3];
    if (!c->key_on) return 0;
    u32 phase = c->phase.output;

    // OK now set our output to op3 carrier basically
// Phase is a 10-bit integer; convert to [0, 2*PI)
    double mphase = ((double)phase) / ((double)(1 << 10)) * M_PI * 2.0;

    // Compute sine and use as channel output
    // -1 to 1
    // convert to 14-bit (-8192 to 8191)
    double output = (sin(mphase) + 1.0); // 0...2
    output *= 8191.5;
    output -= 8192;
    output = round(output);

    return (i16)output;
}

void ym2612_serialize(struct ym2612*this, struct serialized_state *state)
{

}

void ym2612_deserialize(struct ym2612*this, struct serialized_state *state)
{

}

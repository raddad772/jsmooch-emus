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
}

void ym2612_delete(struct ym2612*this)
{

}

void ym2612_write(struct ym2612*this, u32 addr, u8 val)
{

}

u8 ym2612_read(struct ym2612*this, u32 addr, u32 old, u32 has_effect)
{
    return 0;
}

void ym2612_reset(struct ym2612*this)
{

}

void ym2612_cycle(struct ym2612*this)
{

}

i16 ym2612_sample_channel(struct ym2612*this, u32 ch)
{
    return 0;
}

void ym2612_serialize(struct ym2612*this, struct serialized_state *state)
{

}

void ym2612_deserialize(struct ym2612*this, struct serialized_state *state)
{

}

//
// Created by . on 8/4/24.
//

#ifndef JSMOOCH_EMUS_AY_3_8910_H
#define JSMOOCH_EMUS_AY_3_8910_H

#include "helpers/int.h"

enum ay_3_8910_variants {
    AY3V_8910,
    AY3V_8914 // just some shuffled registers
};

enum ay_3_8910_envs {
    AY3ENV_none
};

struct ay_3_8910 {
    enum ay_3_8910_variants variant;

    u32 divider_16;
    u32 divider_256;
    struct {
        u16 freq;
        u8 amplitude;
        u8 amplitude_mode;

        u32 enable;
        u32 enable_noise;
        i16 polarity;

        u16 counter;
    } sw[3];

    struct {
        u8 period;
        u16 counter;
        i16 polarity;
    } noise;

    struct {
        enum ay_3_8910_envs kind;
        u16 period;
        u8 counter;
        u8 count_up;
        u8 mirror;

        u8 e_out;

        u8 hold, alternate, attack, econtinue;
    } env;

    struct {
        u32 enable_a, enable_b;
    } io_ports;
};

void ay_3_8910_init(struct ay_3_8910*, enum ay_3_8910_variants variant);
void ay_3_8910_delete(struct ay_3_8910*);

void ay_3_8910_reset(struct ay_3_8910*);

void ay_3_8910_write(struct ay_3_8910*, u8 addr, u8 val);
u8 ay_3_8910_read(struct ay_3_8910*, u8 addr);

#endif //JSMOOCH_EMUS_AY_3_8910_H

//
// Created by . on 4/14/24.
//

#ifndef JSMOOCH_EMUS_TIA_H
#define JSMOOCH_EMUS_TIA_H

#include "helpers/int.h"

struct atari_TIA {
    // For output to display
    u32 last_used_buffer;
    u32 cur_output_num;
    u8 *cur_output;
    u8 *out_buffer[2];

    struct atari_TIA_col {
        u32 m0_p0, m0_p1, m1_p0, m1_p1;
        u32 p0_pf, p0_ball, p1_pf, p1_ball;
        u32 m0_pf, m0_ball, m1_pf, m1_ball;
        u32 ball_pf, p0_p1, m0_m1;
    } col;

    struct {
        u32 vsync;
        u32 vblank;
        u32 inpt_0_3_ctrl;
        u32 inpt_4_5_ctrl;

        union {
            struct {
                u8 pf0;
                u8 pf1;
                u8 pf2;
            };
            u32 u;
        } pf;

        u8 COLUP0;
        u8 COLUP1;
        u8 COLUPF;
        u8 COLUBK;
        u8 CTRLPF;
    } io;
};

struct atari2600;

void TIA_init(struct atari2600* this);
void TIA_reset(struct atari2600* this);
void TIA_bus_cycle(struct atari2600* this, u32 addr, u32 *data, u32 rw);
void TIA_run_cycle(struct atari2600* this);

#endif //JSMOOCH_EMUS_TIA_H

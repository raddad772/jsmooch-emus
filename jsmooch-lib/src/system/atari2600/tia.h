//
// Created by . on 4/14/24.
//

#ifndef JSMOOCH_EMUS_TIA_H
#define JSMOOCH_EMUS_TIA_H

#include "helpers/int.h"
#include "helpers/cvec.h"

#define TIA_WQ_MAX 20

struct atari_TIA {
    // For output to display
    u32 last_used_buffer;
    u32 cur_output_num;
    u8 *cur_output;
    u8 *out_buffer[2];

    u32 cpu_RDY;

    u32 hcounter;
    u32 vcounter;
    u32 master_frame;
    u32 frames_since_restart;

    struct {
        u32 size;
        i32 x;
        u32 GRP;
        u32 GRP_cache;
        i32 hm;
    } p[2];

    struct {
        u32 size;
        i32 x;
        u32 enable;
        i32 hm;
    } m[2];

    struct {
        u32 size;
        i32 x;
        u32 enable;
        u32 enable_cache;
        i32 hm;
    } ball;


    struct {
        u32 vblank_in_lines;
        u32 display_line_start;
        u32 vblank_out_start;
        u32 vblank_out_lines;
    } timing;

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

        u32 pf;

        u8 COLUP0;
        u8 COLUP1;
        u8 COLUPF;
        u8 COLUBK;
        u8 CTRLPF;

        u32 ENABL_cache; // Vertical delay cache for ENABL
        u32 REFP[2]; // Player reflect

        u32 VDELP[2];
        u32 VDELBL;

        u32 RESMP[2];

        u32 hmoved; // was hmove triggered?

        u32 INPT[6];
    } io;

    struct atari_TIA_WQ_item {
        u32 active;
        u32 address;
        u32 data;
        u32 delay;
    } write_queue[TIA_WQ_MAX];
};

struct atari2600;

void TIA_init(struct atari_TIA* this);
void TIA_reset(struct atari_TIA* this);
void TIA_bus_cycle(struct atari_TIA* this, u32 addr, u32 *data, u32 rw);
void TIA_run_cycle(struct atari_TIA* this);

#endif //JSMOOCH_EMUS_TIA_H

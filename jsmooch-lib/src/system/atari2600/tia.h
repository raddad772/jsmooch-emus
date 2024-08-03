//
// Created by . on 4/14/24.
//

#ifndef JSMOOCH_EMUS_TIA_H
#define JSMOOCH_EMUS_TIA_H

#include "helpers/physical_io.h"
#include "helpers/int.h"
#include "helpers/cvec.h"

#define TIA_WQ_MAX 20

struct atari_TIA {
    // For output to display
    u8 *cur_output;

    u32 missed_vblank;

    struct physical_io_device* display;
    u32 cpu_RDY;

    u32 hcounter;
    u32 vcounter;
    u32 master_frame;
    u32 frames_since_restart;

    struct {
        u32 output;
    } playfield;

    struct TIA_p{
        u32 size;
        i32 counter;
        i32 pixel_counter;
        i32 width_counter;
        u32 GRP[2];
        i32 hm;
        i32 delay; // VDELP
        u32 reflect; // REFP

        u32 copy;
        u32 start_counter;
        u32 starting;

        u32 output; // current pixel output value
    } p[2];

    struct TIA_m {
        u32 size;
        i32 counter;
        u32 enable;
        i32 hm;
        u32 locked_to_player;

        u32 copy;
        u32 start_counter;
        u32 starting;

        i32 pixel_counter;
        i32 width_counter;

        u32 output; // current pixel output value
    } m[2];

    struct TIA_ball {
        u32 size;
        i32 counter;
        u32 enable[2];

        u32 output;
        i32 delay;

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
        union {
            struct {
                u8 mirror : 1;
                u8 score_mode: 1;
                u8 priority: 1;
                u8 : 1;
                u8 ball_size: 2;
            };
            u8 u;
        } CTRLPF;

        u32 ENABL_cache; // Vertical delay cache for ENABL

        u32 VDELBL;

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

void TIA_init(struct atari_TIA*);
void TIA_reset(struct atari_TIA*);
void TIA_bus_cycle(struct atari_TIA*, u32 addr, u32 *data, u32 rw);
void TIA_run_cycle(struct atari_TIA*);

#endif //JSMOOCH_EMUS_TIA_H

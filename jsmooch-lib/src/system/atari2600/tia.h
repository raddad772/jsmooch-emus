//
// Created by . on 4/14/24.
//

#ifndef JSMOOCH_EMUS_TIA_H
#define JSMOOCH_EMUS_TIA_H

#include "helpers_c/physical_io.h"
#include "helpers_c/int.h"
#include "helpers_c/cvec.h"

#define TIA_WQ_MAX 20

struct atari_TIA {
    // For output to display
    u8 *cur_output;

    u32 missed_vblank;

    struct JSM_DISPLAY* display;
    struct cvec_ptr display_ptr;
    u32 cpu_RDY;

    u32 hcounter;
    u32 pcounter;
    u32 vcounter;
    u32 CLK;
    u32 master_frame;
    u32 frames_since_restart;
    u32 clock_div;

    u32 hblank, x, hsync;

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

    struct atari_TIA_M {
        u32 hm;
        u32 size;
        i32 start_counter;
        i32 starting, counter;
        i32 width_counter;
        i32 output;
        i32 pixel_counter;
        u32 enable;
        u32 locked_to_player;
    } m[2];
    struct atari_TIA_ball {
        u32 counter, output, enable[2], delay, size, hm;
    } ball;

    struct atari_TIA_P {
        u32 copy, enable;
        u32 start_counter, starting, counter, pixel_counter, width_counter;
        u32 GRP[2], delay;
        u32 hm;

        u32 count, phase;
        u32 scan_counter, scan_counting;
        u32 reflect;
        u32 output; // 0 or 1
        u32 size;
        i32 scan_divisor, scan_duty;
    } p[2];

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

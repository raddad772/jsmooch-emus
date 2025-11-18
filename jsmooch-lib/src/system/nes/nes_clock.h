//
// Created by Dave on 2/5/2024.
//

#ifndef JSMOOCH_EMUS_NES_CLOCK_H
#define JSMOOCH_EMUS_NES_CLOCK_H

#include "helpers/int.h"

struct NES_clock {
    u64 master_clock;
    u64 master_frame;

    u32 frames_since_restart;

    u64 cpu_master_clock;
    u64 apu_master_clock;
    u64 ppu_master_clock;
    u64 trace_cycles;

    u64 nmi;
    u64 cpu_frame_cycle;
    u64 ppu_frame_cycle;

    struct {
        u32 fps;
        u32 apu_counter_rate;
        u32 cpu_divisor;
        u32 ppu_divisor;
        u32 bottom_rendered_line;
        u32 post_render_ppu_idle;
        u32 hblank_start;
        u32 vblank_start;
        u32 vblank_end;
        u32 ppu_pre_render;
        u32 pixels_per_scanline;
    } timing;

    i32 ppu_y;
    u32 frame_odd;
};

void NES_clock_init(struct NES_clock*);
void NES_clock_reset(struct NES_clock*);

#endif //JSMOOCH_EMUS_NES_CLOCK_H

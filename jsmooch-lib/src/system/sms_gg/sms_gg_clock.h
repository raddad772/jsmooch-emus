//
// Created by Dave on 2/7/2024.
//

#ifndef JSMOOCH_EMUS_SMS_GG_CLOCK_H
#define JSMOOCH_EMUS_SMS_GG_CLOCK_H

#include "helpers/int.h"
#include "helpers/sys_interface.h"

struct SMSGG_clock {
    enum jsm_systems variant;
    enum jsm_regions region;
    u64 master_cycles;
    u64 cpu_master_clock;
    u64 vdp_master_clock;
    u64 apu_master_clock;
    u64 apu_sample_clock;

    u64 frames_since_restart;

    u32 cpu_divisor;
    u32 vdp_divisor;
    u32 apu_divisor;

    u64 trace_cycles;

    u32 cpu_frame_cycle;
    u32 vdp_frame_cycle;

    u32 ccounter;
    u32 hpos;
    u32 vpos;
    i32 line_counter;

    struct SMSGG_clock_timing {
        float fps;
        u32 frame_lines;
        u32 cc_line;
        u32 bottom_rendered_line;
        u32 rendered_lines;
        u32 vblank_start;

        enum jsm_display_standards region;
    } timing;
};

void SMSGG_clock_init(struct SMSGG_clock*, enum jsm_systems variant, enum jsm_regions region);

#endif //JSMOOCH_EMUS_SMS_GG_CLOCK_H

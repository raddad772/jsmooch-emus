//
// Created by Dave on 2/7/2024.
//

#include "sms_gg_clock.h"

void SMSGG_clock_init(struct SMSGG_clock* this, enum jsm_systems variant, enum jsm_regions region)
{
    *this = (struct SMSGG_clock) {
        .variant = variant,
        .region = region,
        .master_cycles = 0,

        .cpu_master_clock = 0,
        .vdp_master_clock = 0,
        .apu_master_clock = 0,
        .frames_since_restart = 0,

        .cpu_divisor = 3,
        .vdp_divisor = 2,
        .apu_divisor = 48, // 50 too low, 46 a little too high,
        .trace_cycles = 0,
        .cpu_frame_cycle = 0,
        .ccounter = 0,
        .hpos = 0,
        .vpos = 0,
        .line_counter = 0,
        .timing = (struct SMSGG_clock_timing) {
            .fps = 60,
            .frame_lines = 262, // PAL 313
            .cc_line = 260, // PAL 311
            .bottom_rendered_line = 191,
            .rendered_lines = 192,
            .vblank_start = 192 // maybe?
        }
    };
    this->timing.region = ((this->region == REGION_USA) || (this->region == REGION_JAPAN)) ? NTSC : PAL;
    if (this->timing.region == PAL) {
        this->timing.frame_lines = 313;
        this->timing.cc_line = 311;
    }
}
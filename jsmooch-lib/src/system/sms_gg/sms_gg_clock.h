//
// Created by Dave on 2/7/2024.
//

#pragma once

#include "helpers/int.h"
#include "helpers/sys_interface.h"

namespace SMSGG {
struct clock {
    explicit clock(jsm::systems in_variant, jsm::regions in_region);
    jsm::systems variant{};
    jsm::regions region{};
    u64 master_cycles{};
    u64 cpu_master_clock{};
    u64 vdp_master_clock{};
    u64 apu_master_clock{};

    u64 frames_since_restart{};

    u32 cpu_divisor{3};
    u32 vdp_divisor{2};
    u32 apu_divisor{48};

    u64 trace_cycles{};

    u32 cpu_frame_cycle{};
    u32 vdp_frame_cycle{};

    u32 ccounter{};
    u32 hpos{};
    u32 vpos{};
    i32 line_counter{};

    struct SMSGG_clock_timing {
        float fps{60.0};
        u32 frame_lines{262};
        u32 cc_line{260};
        u32 bottom_rendered_line{191};
        u32 rendered_lines{192};
        u32 vblank_start{192};

        jsm::display_standards region{};
    } timing{};
};


}
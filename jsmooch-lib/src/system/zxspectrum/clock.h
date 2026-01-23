//
// Created by . on 1/23/26.
//

#pragma once

#include "helpers/int.h"
#include "zxspectrum.h"

namespace ZXSpectrum {
struct CLOCK {
    explicit CLOCK(variants variant_in);
    variants variant;
    u32 frames_since_restart{};
    u32 master_frame{};

    i32 ula_x{}, ula_y{};
    i32 screen_bottom;
    i32 screen_right;
    u32 scanlines_before_frame;
    u32 ula_frame_cycle{};

    u64 master_cycles{};

    struct {
        u32 bit{};
        u32 count{};
    } flash{};

    bool contended{};
};
}

//
// Created by Dave on 2/5/2024.
//

#pragma once
#include "helpers/int.h"

struct NES_clock {
    void reset();
    u64 master_clock{};
    u64 master_frame{};

    u32 frames_since_restart{};

    u64 cpu_master_clock{};
    u64 apu_master_clock{};
    u64 ppu_master_clock{};
    u64 trace_cycles{};

    u64 nmi{};
    u64 cpu_frame_cycle{};
    u64 ppu_frame_cycle{};

    struct {
        u32 fps = 60;
        u32 apu_counter_rate = 60;
        u32 cpu_divisor = 12;
        u32 ppu_divisor = 4;
        u32 bottom_rendered_line = 239;
        u32 post_render_ppu_idle = 240;
        u32 hblank_start = 280;
        u32 vblank_start = 261;
        u32 vblank_end = 261;
        u32 ppu_pre_render = 261;
        u32 pixels_per_scanline = 280;
    } timing{};

    i32 ppu_y{};
    u32 frame_odd{};

    void serialize(struct serialized_state &state) const;
    void deserialize(struct serialized_state &state);
};

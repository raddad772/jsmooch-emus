//
// Created by . on 6/1/24.
//

#pragma once
#include "helpers/int.h"
#include "helpers/enums.h"

namespace genesis {

struct clock {
    explicit clock(jsm::systems in_kind);
    void reset();
    void ntsc();
    void pal();
    u64 master_cycle_count{};
    u64 waitstates{};
    u64 master_frame{};
    u64 frames_since_reset{};

    u32 current_back_buffer{};
    u32 current_front_buffer{};

    u64 delta{};

    jsm::systems kind{};

    u32 mem_break{};

    struct {
        i32 hcount{}, vcount{};
        u32 hblank_active{}, vblank_active{};
        u32 hblank_fast{};
        u32 field{};
        i32 clock_divisor{};
        u32 vblank_on_line{};
        u32 bottom_rendered_line{};
        u32 bottom_max_rendered_line{};
        u64 frame_start{};
    } vdp{};

    struct {
        u64 next_clock{};
        i32 clock_divisor{};
    } psg{};

    struct {
        u64 next_clock{};
        i32 clock_divisor{};
    } ym2612{};

    struct {
        struct {
            u32 cycles_per{};
        } scanline{};

        struct {
            u32 cycles_per{};
            u32 scanlines_per{};
        } frame{};

        struct {
            u32 frames_per{};
            u32 cycles_per{};
        } second{};
    } timing{};
};

/*
  PAL  53.203Mhz, /7 = ~7.6004MHz M68k
                 /15 =  3.546894 MHz Z80
                 /4 =  ~13MHz       VDP
  NTSC 53.6931Mhz /7 = ~7.6704MHz M68k
                 /15 = 3.579545 MHz Z80,
                 / 4 = ~13MHz VDP
 */


}
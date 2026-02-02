//
// Created by . on 3/4/25.
//

#pragma once
#include "helpers/int.h"

namespace PS1 {
struct CLOCK {
    explicit CLOCK(bool is_ntsc_in);
    void reset();
    void init(bool is_ntsc_in);
    void setup_pal();
    void setup_ntsc();
    u32 in_hblank{}, in_vblank{};

    bool is_ntsc{};

    u64 master_cycle_count{};
    u64 frame_cycle_start{};
    u64 master_frame{};
    u64 waitstates{};
    i64 cycles_left_this_frame{};
    u32 vblank_scheduled{};

    struct {
        struct {
            u64 time{}, value{};
        } start{};
        u32 horizontal_px{};
        u32 vertical_px{};

        struct {
            float cpu_to_gpu{};
            float cpu_to_dotclock{};
        } ratio{};

    } dot{};

    struct {
        float fps{};
        struct {
            u64 hz{};
        } cpu{};

        struct {
            u64 hz{};
        } gpu{};

        struct {
            u64 cycles{};
            u64 lines{};

            struct {
                float ratio{};
                u64 cycles{};
                u64 start_on_line{};
            } vblank{};
        } frame{};

        struct {
            u64 cycles{};
            struct {
                u64 cycles{};
                float ratio{};
                u64 start_on_cycle{};
            } hblank{};
        } scanline{};

    } timing{};

    u32 hblank_clock{};
    u32 vblank_clock{};

    struct {
        u32 y{};
        u64 scanline_start{};
        u32 hblank_active{}, vblank_active{};
    } crt{};

private:
    void finish_timing();
};

}

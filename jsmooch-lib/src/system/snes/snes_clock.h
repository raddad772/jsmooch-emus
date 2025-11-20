//
// Created by . on 4/19/25.
//

#pragma once

#include <helpers/int.h>

struct SNES_clock {
    SNES_clock();
    void fill_timing_ntsc();

    u64 master_cycle_count{};
    u64 master_frame{};
    u32 rev{};


    struct {
        i32 divider{};
        i32 has{};
    } cpu{};

    struct {
        i32 divider{};
        i32 has{};
        u32 y{};
        u32 field{}, vblank_active{}, hblank_active{};
        u64 scanline_start{};
    } ppu{};

    struct {
        i32 divider{};
        long double has{};
        long double ratio{};

        struct {
            long double next{}, stride{}, pitch_ratio{};
        } sample{};

        struct {
            long double next, stride{};
        } cycle{};

        struct {
            long double stride{};
        } env{};

    } apu{};

    struct {
        u32 master_hz{};
        u32 apu_master_hz{};
        struct {
            u32 master_cycles{};
        } frame{};
        struct {
            u32 master_cycles{};

            u32 hblank_start{}, hblank_stop{};
            u32 dram_refresh{};

            u32 hdma_setup_position{}, hdma_position{};
        } line{};
        struct {
            u32 master_cycles{};
        } second{};

    } timing{};
};

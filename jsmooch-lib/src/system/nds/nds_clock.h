//
// Created by . on 12/4/24.
//

#pragma once

#include "helpers/int.h"

namespace NDS {

struct clock {
    explicit clock(u64 *current_transaction_in) : current_transaction(current_transaction_in) {}
    u64 *current_transaction;
    u64 frame_start_cycle{};
    i64 cycles_left_this_frame{};
    u64 master_cycle_count7{};
    u64 master_cycle_count9{};
    u64 master_frame{};

    [[nodiscard]] u64 current7() const {
        return master_cycle_count7 + *current_transaction;
    }

    [[nodiscard]] u64 current9() const {
        return master_cycle_count9 + *current_transaction;
    }

    u64 current9();

    i64 cycles7{};
    i64 cycles9{};

    struct {
        u32 x{}, y{};
        u64 scanline_start{};
        u32 hblank_active{}, vblank_active{};
    } ppu{};

    struct {

        struct {
            u32 hz{32768};
        } apu{};
        struct {
            u64 hz{33513982};
            u64 hz_2{33513982 >> 1};
        } arm7{};

        struct {
            u64 cycles{558566};
            float per_second{60};

            u64 vblank_down_on{262};
            u64 vblank_up_on{192};
        } frame{};

        struct {
            u64 cycles_total{2124};
            u64 cycle_of_hblank{1430};
            u64 number{263};
        } scanline{};
    } timing{};


};

}
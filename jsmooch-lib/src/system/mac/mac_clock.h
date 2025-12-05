#pragma once

#include "helpers/int.h"

namespace mac {

struct clock {
    u64 master_cycles{};
    u64 master_frame{};

    struct {
        u32 hpos{}, vpos{};
    } crt{};

    struct timing {
        static constexpr u64 cycles_per_frame = 704 * 370 * 60;// not quite right
        static constexpr u64 fps = 60;
        static constexpr u64 cycles_per_second = cycles_per_frame * fps;
    } timing{};
};

}
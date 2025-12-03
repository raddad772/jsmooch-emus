#pragma once

#include "helpers/int.h"

namespace mac {

struct clock {
    u64 master_cycles{};
    u64 master_frame{};

    struct {
        u32 hpos{}, vpos{};
    } crt{};

    struct {
        u64 cycles_per_frame{};
    } timing{};
};

}
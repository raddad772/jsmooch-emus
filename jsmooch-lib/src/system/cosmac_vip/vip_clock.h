//
// Created by . on 11/20/25.
//
#pragma once

#include "helpers/int.h"

namespace VIP {
struct clock {
    u64 master_cycle_count{};
    u64 master_frame{};
};
}
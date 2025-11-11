//
// Created by . on 2/15/25.
//

#pragma once
#include "helpers/int.h"

struct IRQ_multiplexer {
    u64 IF;
    u32 current_level;
    IRQ_multiplexer();
    u32 set_level(u32 level, u32 from);
    void clear();
};


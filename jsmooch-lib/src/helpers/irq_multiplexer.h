//
// Created by . on 2/15/25.
//

#ifndef JSMOOCH_EMUS_IRQ_MULTIPLEXER_CPP_H
#define JSMOOCH_EMUS_IRQ_MULTIPLEXER_CPP_H

#include "helpers/int.h"

struct IRQ_multiplexer {
    u64 IF;
    u32 current_level;
    IRQ_multiplexer();
    u32 set_level(u32 level, u32 from);
    void clear();
};

#endif //JSMOOCH_EMUS_IRQ_MULTIPLEXER_H

//
// Created by . on 2/15/25.
//

#ifndef JSMOOCH_EMUS_IRQ_MULTIPLEXER_H
#define JSMOOCH_EMUS_IRQ_MULTIPLEXER_H

#include "helpers_c/int.h"

struct IRQ_multiplexer {
    u64 IF;
    u32 current_level;
};

void IRQ_multiplexer_init(struct IRQ_multiplexer *);
u32 IRQ_multiplexer_set_level(struct IRQ_multiplexer *, u32 level, u32 from);
void IRQ_multiplexer_clear(struct IRQ_multiplexer *);

#endif //JSMOOCH_EMUS_IRQ_MULTIPLEXER_H

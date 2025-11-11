//
// Created by . on 2/26/25.
//

#ifndef JSMOOCH_EMUS_BETTER_IRQ_MULTIPLEXER_H
#define JSMOOCH_EMUS_BETTER_IRQ_MULTIPLEXER_H

#include "helpers/int.h"

#define MAX_IRQS_MULTIPLEXED 32

enum IRQ_multiplexer_b_kind {
    IRQMBK_flipflop, // just set to whatever level
    IRQMBK_edge_0_to_1, // triggered on 0->1
    IRQMBK_edge_1_to_0 // triggered on 1->0
};

struct IRQ_multiplexer_b {
    u64 IF;
    u64 max_irq;

    struct IRQ_multiplexer_b_irq {
        u32 input;
        u64 IF; // output signal
        u32 kind;
        char name[50];
    } irqs[MAX_IRQS_MULTIPLEXED];
};

void IRQ_multiplexer_b_init(IRQ_multiplexer_b *, u32 num_IRQs);
void IRQ_multiplexer_b_set_level(IRQ_multiplexer_b *this, u32 num, u32 new_level);
void IRQ_multiplexer_b_reset(IRQ_multiplexer_b *);
void IRQ_multiplexer_b_mask(IRQ_multiplexer_b *, u64 val);
void IRQ_multiplexer_b_setup_irq(IRQ_multiplexer_b *, u32 num, const char *name, enum IRQ_multiplexer_b_kind kind);

#endif //JSMOOCH_EMUS_BETTER_IRQ_MULTIPLEXER_H


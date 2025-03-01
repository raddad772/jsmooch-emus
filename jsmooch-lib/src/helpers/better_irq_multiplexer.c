//
// Created by . on 2/26/25.
//

#include <string.h>
#include <assert.h>
#include <stdio.h>
#include "better_irq_multiplexer.h"
#include "helpers/debug.h"

void IRQ_multiplexer_b_init(struct IRQ_multiplexer_b *this, u32 max_IRQ)
{
    memset(this, 0, sizeof(*this));
    this->max_irq = max_IRQ;
}

void IRQ_multiplexer_b_set_level(struct IRQ_multiplexer_b *this, u32 num, u32 new_level)
{
    struct IRQ_multiplexer_b_irq *irq = &this->irqs[num];
    this->IF &= ~(1 << num);
    u32 old_input = irq->input;
    irq->input = new_level;
    switch(irq->kind) {
        case IRQMBK_flipflop:
            irq->IF = new_level;
            break;
        case IRQMBK_edge_0_to_1:
            printif(better_irq_multiplexer, "\nEdge trigger %d to %d", old_input, irq->input);
            irq->IF |= (!old_input) && irq->input;
            break;
        case IRQMBK_edge_1_to_0:
            irq->IF |= old_input && (!irq->input);
            break;
    }
    this->IF |= irq->IF << num;
    printif(better_irq_multiplexer, "\nirq_multiplexer: %d set to %d, new IF: %lld", num, new_level, this->IF);
}

void IRQ_multiplexer_b_reset(struct IRQ_multiplexer_b *this)
{
    this->IF = 0;
    for (u32 i = 0; i < MAX_IRQS_MULTIPLEXED; i++) {
        struct IRQ_multiplexer_b_irq *irq = &this->irqs[i];
        irq->IF = irq->input = 0;
    }
}

// Basically this is for R3000 writes to I_STAT
void IRQ_multiplexer_b_mask(struct IRQ_multiplexer_b *this, u64 val)
{
    this->IF &= val;
    for (u64 i = 0; i < this->max_irq; i++) {
        u64 bit = (val >> i) & 1;
        this->irqs[i].IF &= bit;
    }
    //printf("\nIRQs masked to %02llx", this->IF);
}

void IRQ_multiplexer_b_setup_irq(struct IRQ_multiplexer_b *this, u32 num, const char *name, enum IRQ_multiplexer_b_kind kind)
{
    assert(num < MAX_IRQS_MULTIPLEXED);
    this->irqs[num].kind = kind;
    snprintf(this->irqs[num].name, sizeof(this->irqs[num].name), "%s", name);
}

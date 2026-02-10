//
// Created by . on 2/26/25.
//

#include <cstring>
#include <cassert>
#include <cstdio>
#include "better_irq_multiplexer.h"
#include "helpers/debug.h"

void IRQ_multiplexer_b::set_level(u32 num, u32 new_level)
{
    IRQ_multiplexer_b_irq *irq = &irqs[num];
    IF &= ~(1 << num);
    u32 old_input = irq->input;
    irq->input = new_level;
    switch(irq->kind) {
        case IRQMBK_flipflop:
            irq->IF = new_level;
            break;
        case IRQMBK_edge_0_to_1: {
            u32 old = irq->IF;
            irq->IF |= (!old_input) && irq->input;
            if (old != irq->IF) {
                if (clock) {
                    printif(better_irq_multiplexer, "\nEdge trigger %s @%lld", irq->name, *clock);
                }
                else {
                    printif(better_irq_multiplexer, "\nEdge trigger %s", irq->name);
                }
            }
            break; }
        case IRQMBK_edge_1_to_0:
            irq->IF |= old_input && (!irq->input);
            break;
    }
    IF |= irq->IF << num;
    //printif(better_irq_multiplexer, "\nirq_multiplexer: %d set to %d, new IF: %lld", num, new_level, IF);
}

void IRQ_multiplexer_b::reset()
{
    IF = 0;
    for (u32 i = 0; i < MAX_IRQS_MULTIPLEXED; i++) {
        IRQ_multiplexer_b_irq *irq = &irqs[i];
        irq->IF = irq->input = 0;
    }
}

// Basically this is for R3000 writes to I_STAT
void IRQ_multiplexer_b::mask(u64 val)
{
    IF &= val;
    for (u64 i = 0; i < max_irq; i++) {
        u64 bit = (val >> i) & 1;
        irqs[i].IF &= bit;
    }
    //printf("\nIRQs masked to %02llx", IF);
}

void IRQ_multiplexer_b::setup_irq(u32 num, const char *name, IRQ_multiplexer_b_kind kind)
{
    assert(num < MAX_IRQS_MULTIPLEXED);
    irqs[num].kind = kind;
    snprintf(irqs[num].name, sizeof(irqs[num].name), "%s", name);
    //printf("\nSETUP %c FOR NUM %d", name, num);
}

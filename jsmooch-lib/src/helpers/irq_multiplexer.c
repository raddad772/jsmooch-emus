//
// Created by . on 2/15/25.
//

#include "irq_multiplexer.h"

void IRQ_multiplexer_init(struct IRQ_multiplexer *this)
{
    this->IF = 0;
    this->current_level = 0;
}

u32 IRQ_multiplexer_set_level(struct IRQ_multiplexer *this, u32 level, u32 from)
{
    if (level == 0)
        this->IF &= ~(1L << from);
    else
        this->IF |= 1L << from;
    this->current_level = this->IF != 0;
    return this->current_level;
}

void IRQ_multiplexer_clear(struct IRQ_multiplexer *this)
{
    this->IF = 0;
}
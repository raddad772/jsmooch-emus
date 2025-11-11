//
// Created by . on 2/15/25.
//

#include "irq_multiplexer.h"

void IRQ_multiplexer_init(IRQ_multiplexer *this)
{
    this->IF = 0;
    this->current_level = 0;
}

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4334) // warning C4334: '<<': result of 32-bit shift implicitly converted to 64 bits (was 64-bit shift intended?)
#endif

u32 IRQ_multiplexer_set_level(IRQ_multiplexer *this, u32 level, u32 from)
{
    if (level == 0)
        this->IF &= ~(1L << from);
    else
        this->IF |= 1L << from;
    this->current_level = this->IF != 0;
    return this->current_level;
}

#ifdef _MSC_VER
#pragma warning(pop)
#endif

void IRQ_multiplexer_clear(IRQ_multiplexer *this)
{
    this->IF = 0;
}
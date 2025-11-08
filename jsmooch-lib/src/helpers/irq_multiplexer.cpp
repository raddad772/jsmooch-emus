//
// Created by . on 2/15/25.
//

#include "irq_multiplexer.h"

IRQ_multiplexer::IRQ_multiplexer()
{
    IF = 0;
    current_level = 0;
}

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4334) // warning C4334: '<<': result of 32-bit shift implicitly converted to 64 bits (was 64-bit shift intended?)
#endif

u32 IRQ_multiplexer::set_level(u32 level, u32 from)
{
    if (level == 0)
        IF &= ~(1L << from);
    else
        IF |= 1L << from;
    current_level = IF != 0;
    return current_level;
}

#ifdef _MSC_VER
#pragma warning(pop)
#endif

void IRQ_multiplexer::clear()
{
    IF = 0;
}
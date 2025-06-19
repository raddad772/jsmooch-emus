//
// Created by . on 6/18/25.
//

#include <printf.h>

#include "tg16_bus.h"

u32 TG16_bus_read(struct TG16 *this, u32 addr, u32 old, u32 has_effect)
{
    printf("\nUnservied bus read addr:%06x", addr);
    return 0;
}
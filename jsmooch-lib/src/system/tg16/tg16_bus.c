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

u32 TG16_bus_write(struct TG16 *this, u32 addr, u32 val)
{
    printf("\nUnserviced bus write addr%06x val:%02x", addr, val);
    return val;
}


u32 TG16_huc_read_mem(void *ptr, u32 addr, u32 old, u32 has_effect)
{
    return TG16_bus_read(ptr, addr, old, has_effect);
}

void TG16_huc_write_mem(void *ptr, u32 addr, u32 val)
{
    TG16_bus_write(ptr, addr, val);
}

u32 TG16_huc_read_io(void *ptr)
{
    printf("\nTG16 read IO not impl.");
    return 0;
}

void TG16_huc_write_io(void *ptr, u32 val)
{
    printf("\nTG16 write IO not impl.");
}

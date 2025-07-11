//
// Created by . on 6/18/25.
//

#include <printf.h>

#include "tg16_bus.h"

u32 TG16_bus_read(struct TG16 *this, u32 addr, u32 old, u32 has_effect)
{
    if (addr >= 0xFFE000) {
        if (addr < 0xFFE400) {
            return HUC6270_read(&this->vdc0, addr, old);
        }
        else if (addr < 0xFFE800) {
            return HUC6260_read(&this->vce, addr, old);
        }
        assert(1==2);
        printf("\nWHAT23");
        return 0;
    }
    if (addr < 0x800000) return TG16_cart_read(&this->cart, addr, old);
    else if ((addr >= 0xF80000) && (addr <= 0xFC0000)) {
        return this->RAM[addr & 0x1FFF];
    }

    printf("\nUnservied bus read addr:%06x", addr);
    return 0;
}

void TG16_bus_write(struct TG16 *this, u32 addr, u32 val)
{
    if (addr >= 0xFFE000) {
        if (addr < 0xFFE400) {
            return HUC6270_write(&this->vdc0, addr, val);
        }
        else if (addr < 0xFFE800) {
            return HUC6260_write(&this->vce, addr, val);
        }
        assert(1==2);
        printf("\nWHAT22");
        return;
    }
    if (addr < 0x800000) return TG16_cart_write(&this->cart, addr, val);
    else if ((addr >= 0xF80000) && (addr <= 0xFC0000)) {
        this->RAM[addr & 0x1FFF] = val;
        return;
    }

    printf("\nUnserviced bus write addr%06x val:%02x", addr, val);
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

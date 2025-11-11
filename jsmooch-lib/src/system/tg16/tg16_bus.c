//
// Created by . on 6/18/25.
//

#include <stdio.h>

#include "tg16_bus.h"

u32 TG16_bus_read(TG16 *this, u32 addr, u32 old, u32 has_effect)
{
    if (addr >= 0x1FE000) {
        if (addr < 0x1FE400) {
            return HUC6270_read(&this->vdc0, addr, old);
        }
        else if (addr < 0x1FE800) {
            return HUC6260_read(&this->vce, addr, old);
        }
        if (has_effect) {
            assert(1 == 2);
            printf("\nWHAT23");
        }
        return 0;
    }
    if (addr < 0x100000) return TG16_cart_read(&this->cart, addr, old);
    else if ((addr >= 0x1EE000) && (addr < 0x1F0000)) {
        return TG16_cart_read_SRAM(&this->cart, addr);
    }
    else if ((addr >= 0x1F0000) && (addr <= 0x1F8000)) {
        return this->RAM[addr & 0x1FFF];
    }

    printf("\nUnserviced bus read addr:%06x", addr);
    return 0;
}

void TG16_bus_write(TG16 *this, u32 addr, u32 val)
{
    if (this->clock.master_cycles > 172000) {
        int a =4;
        a++;
    }
    if (addr >= 0x1FE000) {
        if (addr < 0x1FE400) {
            return HUC6270_write(&this->vdc0, addr, val);
        }
        else if (addr < 0x1FE800) {
            return HUC6260_write(&this->vce, addr, val);
        }
        assert(1==2);
        printf("\nWHAT22");
        return;
    }
    if (addr < 0x100000) return TG16_cart_write(&this->cart, addr, val);
    else if ((addr >= 0x1EE000) && (addr < 0x1F0000)) {
        TG16_cart_write_SRAM(&this->cart, addr, val);
        return;
    }
    else if ((addr >= 0x1F0000) && (addr <= 0x1F8000)) {
        /*if (((addr & 0x1FFF) == 0xF) && (val == 0x12)) {
            dbg_break("\nF to 12", 0);
        }*/
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
    struct TG16 *this = (TG16 *)ptr;
    return TG16_controllerport_read_data(&this->controller_port) & 0x0F;
}

void TG16_huc_write_io(void *ptr, u32 val)
{
    struct TG16 *this = (TG16 *)ptr;
    TG16_controllerport_write_data(&this->controller_port, val & 3);
}

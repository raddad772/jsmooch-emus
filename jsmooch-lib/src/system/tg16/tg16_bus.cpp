//
// Created by . on 6/18/25.
//

#include <cassert>
#include <cstdio>

#include "tg16_bus.h"
namespace TG16 {
u8 core::bus_read(u32 addr, u8 old, bool has_effect)
{
    if (addr >= 0x1FE000) {
        if (addr < 0x1FE400) {
            return vdc0.read(addr, old);
        }
        else if (addr < 0x1FE800) {
            return vce.read(addr, old);
        }
        if (has_effect) {
            assert(1 == 2);
            printf("\nWHAT23");
        }
        return 0;
    }
    if (addr < 0x100000) return cart.read(addr, old);
    else if ((addr >= 0x1EE000) && (addr < 0x1F0000)) {
        return cart.read_SRAM(addr);
    }
    else if ((addr >= 0x1F0000) && (addr <= 0x1F8000)) {
        return RAM[addr & 0x1FFF];
    }

    printf("\nUnserviced bus read addr:%06x", addr);
    return 0;
}

void core::bus_write(u32 addr, u8 val)
{
    if (clock.master_cycles > 172000) {
        int a =4;
        a++;
    }
    if (addr >= 0x1FE000) {
        if (addr < 0x1FE400) {
            return vdc0.write(addr, val);
        }
        else if (addr < 0x1FE800) {
            return vce.write(addr, val);
        }
        assert(1==2);
        printf("\nWHAT22");
        return;
    }
    if (addr < 0x100000) return cart.write(addr, val);
    else if ((addr >= 0x1EE000) && (addr < 0x1F0000)) {
        cart.write_SRAM(addr, val);
        return;
    }
    else if ((addr >= 0x1F0000) && (addr <= 0x1F8000)) {
        /*if (((addr & 0x1FFF) == 0xF) && (val == 0x12)) {
            dbg_break("\nF to 12", 0);
        }*/
        RAM[addr & 0x1FFF] = val;
        return;
    }

    printf("\nUnserviced bus write addr%06x val:%02x", addr, val);
}


u8 core::huc_read_mem(void *ptr, u32 addr, u8 old, bool has_effect)
{
    auto *th = static_cast<core *>(ptr);
    return th->bus_read(addr, old, has_effect);
}

void core::huc_write_mem(void *ptr, u32 addr, u8 val)
{
    auto *th = static_cast<core *>(ptr);
    th->bus_write(addr, val);
}

u8 core::huc_read_io(void *ptr)
{
    auto *th = static_cast<core *>(ptr);
    return th->controller_port.read_data() & 0x0F;
}

void core::huc_write_io(void *ptr, u8 val)
{
    auto *th = static_cast<core *>(ptr);
    th->controller_port.write_data(val & 3);
}
}
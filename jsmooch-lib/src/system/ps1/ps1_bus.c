//
// Created by . on 2/11/25.
//

#include <stdio.h>

#include "ps1_bus.h"
#include "ps1_device.h"

void PS1_bus_init(struct PS1 *this)
{
    this->dma.control = 0x7654321;
    for (u32 i = 0; i < 7; i++) {
        this->dma.channels[i].num = i;
        this->dma.channels[i].step = PS1D_increment;
        this->dma.channels[i].sync = PS1D_manual;
        this->dma.channels[i].direction = PS1D_to_ram;
    }

    PS1_peripheral_init(&this->controller1, PS1PK_gamepad);
    PS1_peripheral_init(&this->controller2, PS1PK_gamepad);
}

void PS1_bus_reset(struct PS1 *this)
{
    this->io.cached_isolated = 0;
}

u32 PS1_mainbus_read(void *ptr, u32 addr, u32 sz, u32 has_effect)
{
    struct PS1* this = (struct PS1*)ptr;
    printf("\nUNHANDLED MAINBUS READ %08x", addr);
    return 0;
}

void PS1_mainbus_write(void *ptr, u32 addr, u32 sz, u32 val)
{
    struct PS1* this = (struct PS1*)ptr;
    printf("\nUNHANDLED MAINBUS WRITE %08x: %08x", addr, val);
}

u32 PS1_mainbus_fetchins(void *ptr, u32 addr, u32 sz)
{
    return PS1_mainbus_read(ptr, addr, sz, 1);
}

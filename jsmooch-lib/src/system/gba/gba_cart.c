//
// Created by . on 12/4/24.
//

#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "helpers/debug.h"
#include "helpers/physical_io.h"
#include "helpers/multisize_memaccess.c"
#include "gba_bus.h"
#include "gba_cart.h"

void GBA_cart_init(struct GBA_cart* this)
{
    *this = (struct GBA_cart) {}; // Set all fields to 0

    buf_init(&this->ROM);
}

void GBA_cart_delete(struct GBA_cart *this)
{
    buf_delete(&this->ROM);
}

u32 GBA_cart_read(struct GBA_cart *this, u32 addr, u32 sz, u32 access, u32 has_effect)
{
    addr &= 0x01FFFFFF;
    addr %= this->ROM.size;
    return cR[sz](this->ROM.ptr, addr);
}

void GBA_cart_write(struct GBA_cart *this, u32 addr, u32 sz, u32 val)
{

}

u32 GBA_cart_read_wait0(struct GBA *this, u32 addr, u32 sz, u32 access, u32 has_effect)
{
    return GBA_cart_read(&this->cart, addr, sz, access, has_effect);
}

u32 GBA_cart_read_wait1(struct GBA *this, u32 addr, u32 sz, u32 access, u32 has_effect)
{
    return GBA_cart_read(&this->cart, addr, sz, access, has_effect);
}

u32 GBA_cart_read_wait2(struct GBA *this, u32 addr, u32 sz, u32 access, u32 has_effect)
{
    return GBA_cart_read(&this->cart, addr, sz, access, has_effect);
}

u32 GBA_cart_read_sram(struct GBA *this, u32 addr, u32 sz, u32 access, u32 has_effect)
{
    printf("\nUH OH READ SRAM!");
    assert(1==2);
    return 0;
}

void GBA_cart_write_sram(struct GBA *this, u32 addr, u32 sz, u32 access, u32 val)
{
    printf("\nUH OH WRITE SRAM!");
    assert(1==2);
}

u32 GBA_cart_load_ROM_from_RAM(struct GBA_cart* this, char* fil, u64 fil_sz, struct physical_io_device *pio, u32 *SRAM_enable) {
    buf_allocate(&this->ROM, fil_sz);
    memcpy(this->ROM.ptr, fil, fil_sz);
    *SRAM_enable = 0;

    pio->cartridge_port.SRAM.requested_size = 0;
    pio->cartridge_port.SRAM.ready_to_use = 0;
    pio->cartridge_port.SRAM.dirty = 0;
    this->RAM.store = &pio->cartridge_port.SRAM;

    return 1;
}
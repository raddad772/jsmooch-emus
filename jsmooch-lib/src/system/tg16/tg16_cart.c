#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "helpers/debug.h"
#include "helpers/physical_io.h"
#include "tg16_cart.h"

void TG16_cart_init(struct TG16_cart *this)
{
    memset(this, 0, sizeof(*this));
    buf_init(&this->ROM);
}

void TG16_cart_delete(struct TG16_cart *this)
{
    buf_delete(&this->ROM);
}

void TG16_cart_reset(struct TG16_cart *this)
{

}

void TG16_cart_write(struct TG16_cart *this, u32 addr, u32 val)
{
    printf("\nWARN cart write %06x %02x", addr, val);
    //dbg_break("WHATHO", 0);
}

u32 TG16_cart_read(struct TG16_cart *this, u32 addr, u32 old)
{
    return ((u8 *)this->ROM.ptr)[addr % this->ROM.size];
}

void TG16_cart_load_ROM_from_RAM(struct TG16_cart *this, void *ptr, u64 sz, struct physical_io_device *pio)
{
    buf_allocate(&this->ROM, sz);
    memcpy(this->ROM.ptr, ptr, sz);
    this->SRAM = &pio->cartridge_port.SRAM;
    if (this->SRAM) {
        this->SRAM->persistent = 1;
        this->SRAM->fill_value = 0xFF;
        this->SRAM->dirty = 1;
        this->SRAM->requested_size = 8192;
    }
}

u8 TG16_cart_read_SRAM(struct TG16_cart *this, u32 addr)
{
    if (!this->SRAM) return 0;
    return ((u8 *)this->SRAM->data)[addr & 0x1FFF];
}

void TG16_cart_write_SRAM(struct TG16_cart *this, u32 addr, u8 val)
{
    if (this->SRAM) {
        ((u8 *)this->SRAM->data)[addr & 0x1FFF] = val;
        this->SRAM->dirty = 1;
    }
}

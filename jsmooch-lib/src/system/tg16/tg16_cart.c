#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "helpers/debug.h"
#include "helpers/physical_io.h"
#include "tg16_cart.h"

void TG16_cart_init(TG16_cart *this)
{
    memset(this, 0, sizeof(*this));
    buf_init(&this->ROM);
}

void TG16_cart_delete(TG16_cart *this)
{
    buf_delete(&this->ROM);
}

void TG16_cart_reset(TG16_cart *this)
{

}

void TG16_cart_write(TG16_cart *this, u32 addr, u32 val)
{
    // NEUTOPIA!!!
    printf("\nWARN cart write %06x %02x", addr, val);
    /*if (addr == 0x02b802) {
        dbg_break("CARTWRITE", 0);
    }*/
    //dbg_break("WHATHO", 0);
}

u32 TG16_cart_read(TG16_cart *this, u32 addr, u32 old)
{
    u32 bank = (addr >> 17) & 7;
    u32 inner = addr & 0x1FFFF;
    return this->cart_ptrs[bank][inner];
}

void TG16_cart_load_ROM_from_RAM(TG16_cart *this, void *ptr, u64 sz, physical_io_device *pio)
{
    buf_allocate(&this->ROM, sz);
    memcpy(this->ROM.ptr, ptr, sz);
    u32 num_banks = (sz >> 17) & 7;
    // 384, 512, 768...
    u32 offsets[8];
    switch(num_banks) {
        case 3: {
            // 384kb: 256 + 128x2
            u32 o[8] = {0, 1, 0, 1, 2, 2, 2, 2};
            memcpy(offsets, o, sizeof(offsets));
            break; }
        case 4: {
            // 512kb: 256 + 256x3
            u32 o[8] = {0, 1, 2, 3, 2, 3, 2, 3};
            memcpy(offsets, o, sizeof(offsets));
            break; }
        case 6: {
            // 768kb: 512 + 256x2
            u32 o[8] = {0, 1, 2, 3, 4, 5, 4, 5};
            memcpy(offsets, o, sizeof(offsets));
            break;}
        default: {
            // Elsewise
            u32 o[8] = {0, 1, 2, 3, 4, 5, 6, 7};
            memcpy(offsets, o, sizeof(offsets));
            break; }
    }
    for (u32 i = 0; i < 8; i++) {
        this->cart_ptrs[i] = ((u8*)this->ROM.ptr) + ((offsets[i] * 0x20000) % sz);
    }
    this->SRAM = &pio->cartridge_port.SRAM;
    if (this->SRAM) {
        this->SRAM->persistent = 1;
        this->SRAM->fill_value = 0xFF;
        this->SRAM->dirty = 1;
        this->SRAM->requested_size = 8192;
    }
}

u8 TG16_cart_read_SRAM(TG16_cart *this, u32 addr)
{
    if (!this->SRAM) return 0;
    return ((u8 *)this->SRAM->data)[addr & 0x1FFF];
}

void TG16_cart_write_SRAM(TG16_cart *this, u32 addr, u8 val)
{
    if (this->SRAM) {
        ((u8 *)this->SRAM->data)[addr & 0x1FFF] = val;
        this->SRAM->dirty = 1;
    }
}

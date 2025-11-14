//
// Created by Dave on 4/14/24.
//

#include <cstdio>
#include <cstring>
#include "cart.h"

void atari2600_cart_init(atari2600_cart* this)
{
    // TODO
    buf_init(&this->ROM);
}

void atari2600_cart_delete(atari2600_cart* this)
{
    buf_delete(&this->ROM);
}

void atari2600_cart_bus_cycle(atari2600_cart* this, u32 addr, u32 *data, u32 rw)
{
    addr &= this->addr_mask;
    if (rw == 0) {
        const u8 *rom_ptr = this->ROM.ptr;
        *data = rom_ptr[addr];
    }
    else {
        printf("\nUNSUPPORTED WRITE TO CART %03x", addr);
    }
}

void atari2600_cart_load_ROM_from_RAM(atari2600_cart* this, char *fil, u64 file_sz, char* file_name)
{
    printf("\nAllocate cart %lld", file_sz);
    buf_allocate(&this->ROM, file_sz);
    memcpy(this->ROM.ptr, fil, file_sz);
    printf("\nROM SIZE %lld", this->ROM.size);
    if (file_sz == 2048) {
        this->addr_mask = 0x7FF;
    }
    else if (file_sz == 4096) {
        this->addr_mask = 0xFFF;
    }
    else {
        printf("\nUnsupported cart sixe %lld", file_sz);
        return;
    }
}

void atari2600_cart_reset(atari2600_cart* this)
{

}
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

#include "../nds_bus.h"
#include "nds_cart.h"

void NDS_cart_init(struct NDS_cart* this)
{
    *this = (struct NDS_cart) {}; // Set all fields to 0

    buf_init(&this->ROM);
}

void NDS_cart_delete(struct NDS_cart *this)
{
    buf_delete(&this->ROM);
}

static const u32 masksz[5] = { 0, 0xFF, 0xFFFF, 0, 0xFFFFFFFF };

u32 NDS_cart_read(struct NDS *this, u32 addr, u32 sz, u32 access, u32 has_effect, u32 ws) {
    // TODO: this
    return 0;
}

void NDS_cart_write(struct NDS *this, u32 addr, u32 sz, u32 access, u32 val)
{
    // TODO: this
}


u32 NDS_cart_load_ROM_from_RAM(struct NDS_cart* this, char* fil, u64 fil_sz, struct physical_io_device *pio, u32 *SRAM_enable) {
    buf_allocate(&this->ROM, fil_sz);
    memcpy(this->ROM.ptr, fil, fil_sz);
    return 1;
}
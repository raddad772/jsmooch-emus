//
// Created by . on 4/21/25.
//

#ifndef JSMOOCH_EMUS_SNES_CART_H
#define JSMOOCH_EMUS_SNES_CART_H

#include "helpers/int.h"
#include "helpers/buf.h"
#include "helpers/physical_io.h"

struct SNES_cart {
    struct buf ROM;
    struct SNES_cart_header {
        u32 mapping_mode;
        u32 version;
        u32 hi_speed;
        u32 rom_size, sram_size;
        u32 sram_mask;
        u32 rom_sizebit, sram_sizebit;
        char internal_name[21];
        u32 lorom, hirom;
        u32 offset;
    } header;
    struct persistent_store *SRAM;
};

struct SNES;
void SNES_cart_init(SNES *);
void SNES_cart_delete(SNES *);
u32 SNES_cart_load_ROM_from_RAM(SNES *, char* fil, u64 fil_sz, physical_io_device *pio);
#endif //JSMOOCH_EMUS_SNES_CART_H

//
// Created by . on 12/4/24.
//

#ifndef JSMOOCH_EMUS_GBA_CART_H
#define JSMOOCH_EMUS_GBA_CART_H


#include "helpers/buf.h"
#include "helpers/int.h"

struct GBA_cart {
    struct buf ROM;
    struct {
        u32 mask, size, present, persists;
        struct persistent_store *store;
    } RAM;
};

void GBA_cart_init(struct GBA_cart*);
void GBA_cart_delete(struct GBA_cart *);
u32 GBA_cart_read(struct GBA_cart *, u32 addr, u32 sz, u32 has_effect, u32 open_bus);
void GBA_cart_write(struct GBA_cart *, u32 addr, u32 sz, u32 val);
u32 GBA_cart_load_ROM_from_RAM(struct GBA_cart*, char* fil, u64 fil_sz, struct physical_io_device *pio, u32 *SRAM_enable);
#endif //JSMOOCH_EMUS_GBA_CART_H

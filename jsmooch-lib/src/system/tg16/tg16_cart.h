//
// Created by . on 7/6/25.
//

#ifndef JSMOOCH_EMUS_TG16_CART_H
#define JSMOOCH_EMUS_TG16_CART_H

#include "helpers/int.h"
#include "helpers/buf.h"
#include "helpers/physical_io.h"

struct TG16_cart {
    struct buf ROM;
    struct persistent_store *SRAM;
    u8 *cart_ptrs[8];
};

void TG16_cart_init(TG16_cart *);
void TG16_cart_delete(TG16_cart *);
void TG16_cart_reset(TG16_cart *);
void TG16_cart_write(TG16_cart *, u32 addr, u32 val);
u32 TG16_cart_read(TG16_cart *, u32 addr, u32 old);
void TG16_cart_load_ROM_from_RAM(TG16_cart *, void *ptr, u64 sz, physical_io_device *pio);
u8 TG16_cart_read_SRAM(TG16_cart *, u32 addr);
void TG16_cart_write_SRAM(TG16_cart *, u32 addr, u8 val);
#endif //JSMOOCH_EMUS_TG16_CART_H

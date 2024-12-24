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
u32 GBA_cart_read(struct GBA_cart *this, u32 addr, u32 sz, u32 access, u32 has_effect);
u32 GBA_cart_load_ROM_from_RAM(struct GBA_cart*, char* fil, u64 fil_sz, struct physical_io_device *pio, u32 *SRAM_enable);

struct GBA;
u32 GBA_cart_read_wait0(struct GBA*, u32 addr, u32 sz, u32 access, u32 has_effect);
u32 GBA_cart_read_wait1(struct GBA*, u32 addr, u32 sz, u32 access, u32 has_effect);
u32 GBA_cart_read_wait2(struct GBA*, u32 addr, u32 sz, u32 access, u32 has_effect);
u32 GBA_cart_read_sram(struct GBA*, u32 addr, u32 sz, u32 access, u32 has_effect);
void GBA_cart_write_sram(struct GBA*, u32 addr, u32 sz, u32 access, u32 val);
#endif //JSMOOCH_EMUS_GBA_CART_H

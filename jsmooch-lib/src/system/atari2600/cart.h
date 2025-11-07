//
// Created by Dave on 4/14/24.
//

#ifndef JSMOOCH_EMUS_CART_H
#define JSMOOCH_EMUS_CART_H

#include "helpers_c/int.h"
#include "helpers_c/buf.h"

struct atari2600_cart {
    struct buf ROM;
    u32 addr_mask;

};

void atari2600_cart_init(struct atari2600_cart*);
void atari2600_cart_delete(struct atari2600_cart*);
void atari2600_cart_bus_cycle(struct atari2600_cart*, u32 addr, u32 *data, u32 rw);
void atari2600_cart_load_ROM_from_RAM(struct atari2600_cart*, char *fil, u64 file_sz, char* file_name);
void atari2600_cart_reset(struct atari2600_cart*);

#endif //JSMOOCH_EMUS_CART_H

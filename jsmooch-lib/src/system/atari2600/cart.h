//
// Created by Dave on 4/14/24.
//

#ifndef JSMOOCH_EMUS_CART_H
#define JSMOOCH_EMUS_CART_H

#include "helpers/int.h"
#include "helpers/buf.h"

struct atari2600_cart {
    struct buf ROM;
    u32 addr_mask;

};

void atari2600_cart_init(struct atari2600_cart* this);
void atari2600_cart_delete(struct atari2600_cart* this);
void atari2600_cart_bus_cycle(struct atari2600_cart* this, u32 addr, u32 *data, u32 rw);
void atari2600_cart_load_ROM_from_RAM(struct atari2600_cart* this, char *fil, u64 file_sz, char* file_name);

#endif //JSMOOCH_EMUS_CART_H

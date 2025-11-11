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

void atari2600_cart_init(atari2600_cart*);
void atari2600_cart_delete(atari2600_cart*);
void atari2600_cart_bus_cycle(atari2600_cart*, u32 addr, u32 *data, u32 rw);
void atari2600_cart_load_ROM_from_RAM(atari2600_cart*, char *fil, u64 file_sz, char* file_name);
void atari2600_cart_reset(atari2600_cart*);

#endif //JSMOOCH_EMUS_CART_H

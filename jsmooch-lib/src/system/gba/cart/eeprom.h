//
// Created by . on 1/17/25.
//

#ifndef JSMOOCH_EMUS_EEPROM_H
#define JSMOOCH_EMUS_EEPROM_H

#include "gba_cart.h"

void GBA_cart_init_eeprom(GBA_cart *this);
void GBA_cart_write_eeprom(GBA*this, u32 addr, u32 sz, u32 access, u32 val);
u32 GBA_cart_read_eeprom(GBA*this, u32 addr, u32 sz, u32 access, u32 has_effect);

#endif //JSMOOCH_EMUS_EEPROM_H

//
// Created by . on 6/1/24.
//

#ifndef JSMOOCH_EMUS_GENESIS_VDP_H
#define JSMOOCH_EMUS_GENESIS_VDP_H


#include "helpers/int.h"
#include "helpers/physical_io.h"

#include "genesis_bus.h"

void genesis_VDP_init(struct genesis* this);
void genesis_VDP_delete(struct genesis* this);
u16 genesis_VDP_mainbus_read(struct genesis* this, u32 addr, u16 old, u16 mask, u32 has_effect);
void genesis_VDP_mainbus_write(struct genesis* this, u32 addr, u16 val, u16 mask);
void genesis_VDP_cycle(struct genesis* this);
u8 genesis_VDP_z80_read(struct genesis* this, u32 addr, u8 old, u32 has_effect);
void genesis_VDP_z80_write(struct genesis* this, u32 addr, u8 val);
void genesis_VDP_reset(struct genesis* this);

#endif //JSMOOCH_EMUS_GENESIS_VDP_H

#ifndef _JSMOOCH_GB_NOMAPPER_H
#define _JSMOOCH_GB_NOMAPPER_H

#include "helpers/int.h"
#include "mapper.h"
#include "../gb_clock.h"
#include "../gb_bus.h"

struct GB_mapper_none {
    struct GB_clock *clock;
    struct GB_bus *bus;

    u8 *ROM;
    u32 ROM_bank_offset;
    u32 RAM_mask;
    u32 has_RAM;
    struct GB_cart* cart;
};

void GB_mapper_none_new(GB_mapper *parent, GB_clock *clock, GB_bus *bus);
void GB_mapper_none_delete(GB_mapper *parent);
void GBMN_reset(GB_mapper* parent);
void GBMN_set_cart(GB_mapper* parent, GB_cart* cart);

u32 GBMN_CPU_read(GB_mapper* parent, u32 addr, u32 val, u32 has_effect);
void GBMN_CPU_write(GB_mapper* parent, u32 addr, u32 val);

#endif
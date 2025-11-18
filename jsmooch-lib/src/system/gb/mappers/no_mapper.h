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

void GB_mapper_none_new(struct GB_mapper *parent, struct GB_clock *clock, struct GB_bus *bus);
void GB_mapper_none_delete(struct GB_mapper *parent);
void GBMN_reset(struct GB_mapper* parent);
void GBMN_set_cart(struct GB_mapper* parent, struct GB_cart* cart);

u32 GBMN_CPU_read(struct GB_mapper* parent, u32 addr, u32 val, u32 has_effect);
void GBMN_CPU_write(struct GB_mapper* parent, u32 addr, u32 val);

#endif
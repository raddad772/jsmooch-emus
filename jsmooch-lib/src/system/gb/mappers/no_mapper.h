#pragma once
#include "helpers/int.h"
#include "mapper.h"
#include "../gb_bus.h"
namespace GB {
struct GB_mapper_none {
    clock *clock;
    core *bus;

    u8 *ROM;
    u32 ROM_bank_offset;
    u32 RAM_mask;
    u32 has_RAM;
    cart* cart;
};

void GB_mapper_none_new(MAPPER *parent, clock *clock_in, core *bus_in);
void GB_mapper_none_delete(MAPPER *parent);
void GBMN_reset(MAPPER* parent);
void GBMN_set_cart(MAPPER* parent, cart* cart);

u32 GBMN_CPU_read(MAPPER* parent, u32 addr, u32 val, u32 has_effect);
void GBMN_CPU_write(MAPPER* parent, u32 addr, u32 val);

}
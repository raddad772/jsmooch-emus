#pragma once

#include "helpers/int.h"
#include "mapper.h"
#include "../gb_clock.h"
#include "../gb_bus.h"

namespace GB {
struct GB_mapper_MBC2 {
    core *bus;
    clock *clock;

    u8 *ROM;
    u32 RAM_mask;
    u32 has_RAM;
    cart* cart;

    u32 ROM_bank_hi_offset;// = 16384;

    u32 num_ROM_banks;
    struct {
        u32 ROMB;
        u32 ext_RAM_enable;
    } regs;
};

void GB_mapper_MBC2_new(MAPPER *parent, clock *clock, core *bus);
void GB_mapper_MBC2_delete(MAPPER *parent);
void GBMBC2_reset(MAPPER* parent);
void GBMBC2_set_cart(MAPPER* parent, cart* cart);

u32 GBMBC2_CPU_read(MAPPER* parent, u32 addr, u32 val, u32 has_effect);
void GBMBC2_CPU_write(MAPPER* parent, u32 addr, u32 val);

}
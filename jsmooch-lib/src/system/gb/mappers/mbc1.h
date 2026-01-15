#pragma once

#include "helpers/int.h"
#include "mapper.h"
#include "../gb_clock.h"
#include "../gb_bus.h"

namespace GB {

struct GB_mapper_MBC1 {
    core *bus;
    clock *clock;

    u8 *ROM;
    u32 RAM_mask;
    u32 has_RAM;
    cart* cart;

    //
    u32 ROM_bank_lo_offset;// = 0;
    u32 ROM_bank_hi_offset;// = 16384;

    u32 num_ROM_banks;
    u32 num_RAM_banks;
    struct {
        u32 banking_mode;
        u32 BANK1;
        u32 BANK2;
        u32 ext_RAM_enable;
    } regs;
    u32 cartRAM_offset;
};

void GB_mapper_MBC1_new(MAPPER *parent, clock *clock_in, core *bus_in);
void GB_mapper_MBC1_delete(MAPPER *parent);
void GBMBC1_reset(MAPPER* parent);
void GBMBC1_set_cart(MAPPER* parent, cart* cart);

u32 GBMBC1_CPU_read(MAPPER* parent, u32 addr, u32 val, u32 has_effect);
void GBMBC1_CPU_write(MAPPER* parent, u32 addr, u32 val);

}
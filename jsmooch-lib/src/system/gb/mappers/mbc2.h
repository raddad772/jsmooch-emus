//
// Created by Dave on 2/2/2024.
//

#ifndef JSMOOCH_EMUS_MBC2_H
#define JSMOOCH_EMUS_MBC2_H

#include "helpers_c/int.h"
#include "mapper.h"
#include "../gb_clock.h"
#include "../gb_bus.h"

struct GB_mapper_MBC2 {
    struct GB_bus *bus;
    struct GB_clock *clock;

    u8 *ROM;
    u32 RAM_mask;
    u32 has_RAM;
    struct GB_cart* cart;

    u32 ROM_bank_hi_offset;// = 16384;

    u32 num_ROM_banks;
    struct {
        u32 ROMB;
        u32 ext_RAM_enable;
    } regs;
};

void GB_mapper_MBC2_new(struct GB_mapper *parent, struct GB_clock *clock, struct GB_bus *bus);
void GB_mapper_MBC2_delete(struct GB_mapper *parent);
void GBMBC2_reset(struct GB_mapper* parent);
void GBMBC2_set_cart(struct GB_mapper* parent, struct GB_cart* cart);

u32 GBMBC2_CPU_read(struct GB_mapper* parent, u32 addr, u32 val, u32 has_effect);
void GBMBC2_CPU_write(struct GB_mapper* parent, u32 addr, u32 val);

#endif //JSMOOCH_EMUS_MBC2_H

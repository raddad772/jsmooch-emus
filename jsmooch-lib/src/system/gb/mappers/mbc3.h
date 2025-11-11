//
// Created by Dave on 2/2/2024.
//

#ifndef JSMOOCH_EMUS_MBC3_H
#define JSMOOCH_EMUS_MBC3_H

#include "helpers/int.h"
#include "mapper.h"
#include "../gb_clock.h"
#include "../gb_bus.h"

struct GB_mapper_MBC3 {
    struct GB_bus *bus;
    struct GB_clock *clock;

    u8 *ROM;
    u32 has_RAM;
    struct GB_cart* cart;

    struct {
        u32 ext_RAM_enable;
        u32 ROM_bank_lo;
        u32 ROM_bank_hi;
        u32 RAM_bank;
        u32 last_RTC_latch_write;
        u32 RTC_latched[5];
        u64 RTC_start;
    } regs;
    u32 ROM_bank_offset_hi;
    u32 RAM_bank_offset;
    u32 num_ROM_banks;
    u32 num_RAM_banks;
};

void GB_mapper_MBC3_new(struct GB_mapper *parent, GB_clock *clock, GB_bus *bus);
void GB_mapper_MBC3_delete(struct GB_mapper *parent);
void GBMBC3_reset(struct GB_mapper* parent);
void GBMBC3_set_cart(struct GB_mapper* parent, GB_cart* cart);

u32 GBMBC3_CPU_read(struct GB_mapper* parent, u32 addr, u32 val, u32 has_effect);
void GBMBC3_CPU_write(struct GB_mapper* parent, u32 addr, u32 val);

#endif //JSMOOCH_EMUS_MBC3_H

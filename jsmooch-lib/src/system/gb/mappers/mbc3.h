//
// Created by Dave on 2/2/2024.
//
#pragma once

#include "helpers/int.h"
#include "mapper.h"
#include "../cart.h"
#include "../gb_clock.h"
#include "../gb_bus.h"
namespace GB {
struct GB_mapper_MBC3 {
    core *bus;
    clock *clock;

    u8 *ROM;
    u32 has_RAM;
    cart* cart;

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

void GB_mapper_MBC3_new(MAPPER *parent, clock *clock_in, core *bus_in);
void GB_mapper_MBC3_delete(MAPPER *parent);
void GBMBC3_reset(MAPPER* parent);
void GBMBC3_set_cart(MAPPER* parent, cart* cart);

u32 GBMBC3_CPU_read(MAPPER* parent, u32 addr, u32 val, u32 has_effect);
void GBMBC3_CPU_write(MAPPER* parent, u32 addr, u32 val);
}

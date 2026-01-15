//
// Created by Dave on 2/2/2024.
//
//
// Created by Dave on 1/27/2024.
//

#include <cstdlib>
#include <cassert>
#include "stdio.h"
#include <cstring>
#include "time.h"

#include "helpers/serialize/serialize.h"

#include "mbc3.h"
namespace GB {
#define SELF struct GB_mapper_MBC3* self = (GB_mapper_MBC3*)parent->ptr

static void serialize(MAPPER *parent, serialized_state &state)
{
    SELF;
#define S(x) Sadd(state, &(self-> x), sizeof(self-> x))
    S(regs.ext_RAM_enable);
    S(regs.ROM_bank_lo);
    S(regs.ROM_bank_hi);
    S(regs.RAM_bank);
    S(regs.last_RTC_latch_write);
    S(regs.RTC_latched[0]);
    S(regs.RTC_latched[1]);
    S(regs.RTC_latched[2]);
    S(regs.RTC_latched[3]);
    S(regs.RTC_latched[4]);
    S(regs.RTC_start);
    S(ROM_bank_offset_hi);
    S(RAM_bank_offset);
#undef S
}

static void deserialize(MAPPER *parent, serialized_state &state)
{
    SELF;
#define L(x) Sload(state, &(self-> x), sizeof(self-> x))
    L(regs.ext_RAM_enable);
    L(regs.ROM_bank_lo);
    L(regs.ROM_bank_hi);
    L(regs.RAM_bank);
    L(regs.last_RTC_latch_write);
    L(regs.RTC_latched[0]);
    L(regs.RTC_latched[1]);
    L(regs.RTC_latched[2]);
    L(regs.RTC_latched[3]);
    L(regs.RTC_latched[4]);
    L(regs.RTC_start);
    L(ROM_bank_offset_hi);
    L(RAM_bank_offset);
#undef L
}


void GB_mapper_MBC3_new(MAPPER *parent, clock *clock, core *bus)
{
    auto *self = static_cast<GB_mapper_MBC3 *>(malloc(sizeof(GB_mapper_MBC3)));
    parent->ptr = static_cast<void *>(self);

    self->ROM = nullptr;
    self->bus = bus;
    self->clock = clock;
    self->cart = nullptr;

    parent->CPU_read = &GBMBC3_CPU_read;
    parent->CPU_write = &GBMBC3_CPU_write;
    parent->reset = &GBMBC3_reset;
    parent->set_cart = &GBMBC3_set_cart;
    parent->serialize = &serialize;
    parent->deserialize = &deserialize;

    self->num_ROM_banks = 0;
    self->num_RAM_banks = 0;
    self->regs.ext_RAM_enable = 1;
    self->regs.ROM_bank_lo = 0;
    self->regs.ROM_bank_hi = 1;
    self->regs.RAM_bank = 0;
    self->regs.last_RTC_latch_write = 0xFF;
    for (u32 i = 0; i < 5; i++) {
        self->regs.RTC_latched[i] = 0;
    }
    self->regs.RTC_start = time(nullptr);
    self->RAM_bank_offset = 0;
}

void GB_mapper_MBC3_delete(MAPPER *parent)
{
    if (parent->ptr == nullptr) return;
    SELF;

    if(self->ROM != nullptr) {
        free(self->ROM);
        self->ROM = nullptr;
    }

    free(parent->ptr);
}

void GBMBC3_reset(MAPPER* parent)
{
    SELF;
    self->ROM_bank_offset_hi = 16384;

    self->regs.ext_RAM_enable = 1;
    self->regs.ROM_bank_hi = 1;
    self->regs.ROM_bank_lo = 0;
    self->regs.RAM_bank = 0;
    self->RAM_bank_offset = 0;
    self->regs.last_RTC_latch_write = 0xFF;
}

static void GBMBC3_remap(GB_mapper_MBC3 *self)
{
    self->regs.ROM_bank_hi %= self->num_ROM_banks;

    self->ROM_bank_offset_hi = self->regs.ROM_bank_hi * 16384;
    u32 rb = self->regs.RAM_bank;
    if (self->cart->header.timer_present) {
        if (self->regs.RAM_bank <= 3) self->regs.RAM_bank %= self->num_RAM_banks;
        if (rb <= 3) {
            if (self->has_RAM) self->RAM_bank_offset = rb * 8192;
        } else {
            //console.log('SET TO RTC!', rb);
            // RTC registers handled during read/write ops to the area
        }
        if (self->has_RAM) self->RAM_bank_offset = rb * 8192;
    }
    else {
        rb &= 3;
        if (self->has_RAM) self->RAM_bank_offset = rb * 8192;
    }
}

u32 GBMBC3_CPU_read(MAPPER* parent, u32 addr, u32 val, u32 has_effect)
{
    SELF;
    if (addr < 0x4000) // ROM lo bank
        return self->ROM[addr & 0x3FFF];
    if (addr < 0x8000) // ROM hi bank
        return self->ROM[(addr & 0x3FFF) + self->ROM_bank_offset_hi];
    if ((addr >= 0xA000) && (addr < 0xC000)) { // cartRAM if there
        if (!self->regs.ext_RAM_enable) return 0xFF;
        if (self->regs.RAM_bank < 4) {
            if (!self->has_RAM) return 0xFF;
            return static_cast<u8 *>(self->cart->SRAM->data)[(addr & 0x1FFF) + self->RAM_bank_offset];
        }
        if ((self->regs.RAM_bank >= 8) && (self->regs.RAM_bank <= 0x0C) && self->cart->header.timer_present)
            return self->regs.RTC_latched[self->regs.RAM_bank - 8];
        return 0xFF;
    }
    assert(1!=0);
    return 0xFF;
}

void GBMBC3_RTC_latch(GB_mapper_MBC3 *self)
{
    printf("\nNO MBC3 LATCH YET");
}

void GBMBC3_CPU_write(MAPPER* parent, u32 addr, u32 val)
{
    SELF;
    if (addr < 0x2000) { // RAM and timer enable, write-only
        self->regs.ext_RAM_enable = (val & 0x0F) == 0x0A;
        return;
    }
    if (addr < 0x4000) {
        // 16KB hi ROM bank number, 7 bits. 0 = 1, otherwise it's normal
        val &= 0x7F;
        if (val == 0) val = 1;
        self->regs.ROM_bank_hi = val;
        GBMBC3_remap(self);
        return;
    }
    if (addr < 0x6000) { // RAM bank, 0-3. 8-C maps RTC in the same range
        self->regs.RAM_bank = val & 0x0F;
        GBMBC3_remap(self);
        return;
    }
    if (addr < 0x8000) { // // Write 0 then 1 to latch RTC clock data, no effect if no clock
        if ((self->regs.last_RTC_latch_write == 0) && (val == 1))
            GBMBC3_RTC_latch(self);
        self->regs.last_RTC_latch_write = val;
        return;
    }

    if ((addr >= 0xA000) && (addr < 0xC000)) { // cart RAM
        if ((self->has_RAM) && (self->regs.RAM_bank < 4)) {
            ((u8 *)self->cart->SRAM->data)[(addr & 0x1FFF) + self->RAM_bank_offset] = val;
            self->cart->SRAM->dirty = true;
        }
        return;
    }
}

void GBMBC3_set_cart(MAPPER* parent, cart* cart)
{
    SELF;
    printf("Loading MBC3...");
    self->cart = cart;

    if (self->ROM != nullptr) free(self->ROM);
    self->ROM = static_cast<u8 *>(malloc(cart->header.ROM_size));
    memcpy(self->ROM, cart->ROM, cart->header.ROM_size);

    self->has_RAM = self->cart->header.RAM_size > 0;

    self->num_RAM_banks = self->cart->header.RAM_size / 8192;
    self->num_ROM_banks = cart->header.ROM_size >> 14;
}
}
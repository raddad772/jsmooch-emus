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

#define THIS struct GB_mapper_MBC3* this = (GB_mapper_MBC3*)parent->ptr

static void serialize(GB_mapper *parent, serialized_state *state)
{
    THIS;
#define S(x) Sadd(state, &(this-> x), sizeof(this-> x))
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

static void deserialize(GB_mapper *parent, serialized_state *state)
{
    THIS;
#define L(x) Sload(state, &(this-> x), sizeof(this-> x))
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


void GB_mapper_MBC3_new(GB_mapper *parent, GB_clock *clock, GB_bus *bus)
{
    struct GB_mapper_MBC3 *this = (GB_mapper_MBC3 *)malloc(sizeof(GB_mapper_MBC3));
    parent->ptr = (void *)this;

    this->ROM = NULL;
    this->bus = bus;
    this->clock = clock;
    this->cart = NULL;

    parent->CPU_read = &GBMBC3_CPU_read;
    parent->CPU_write = &GBMBC3_CPU_write;
    parent->reset = &GBMBC3_reset;
    parent->set_cart = &GBMBC3_set_cart;
    parent->serialize = &serialize;
    parent->deserialize = &deserialize;

    this->num_ROM_banks = 0;
    this->num_RAM_banks = 0;
    this->regs.ext_RAM_enable = 1;
    this->regs.ROM_bank_lo = 0;
    this->regs.ROM_bank_hi = 1;
    this->regs.RAM_bank = 0;
    this->regs.last_RTC_latch_write = 0xFF;
    for (u32 i = 0; i < 5; i++) {
        this->regs.RTC_latched[i] = 0;
    }
    this->regs.RTC_start = time(NULL);
    this->RAM_bank_offset = 0;
}

void GB_mapper_MBC3_delete(GB_mapper *parent)
{
    if (parent->ptr == NULL) return;
    THIS;

    if(this->ROM != NULL) {
        free(this->ROM);
        this->ROM = NULL;
    }

    free(parent->ptr);
}

void GBMBC3_reset(GB_mapper* parent)
{
    THIS;
    this->ROM_bank_offset_hi = 16384;

    this->regs.ext_RAM_enable = 1;
    this->regs.ROM_bank_hi = 1;
    this->regs.ROM_bank_lo = 0;
    this->regs.RAM_bank = 0;
    this->RAM_bank_offset = 0;
    this->regs.last_RTC_latch_write = 0xFF;
}

static void GBMBC3_remap(GB_mapper_MBC3 *this)
{
    this->regs.ROM_bank_hi %= this->num_ROM_banks;

    this->ROM_bank_offset_hi = this->regs.ROM_bank_hi * 16384;
    u32 rb = this->regs.RAM_bank;
    if (this->cart->header.timer_present) {
        if (this->regs.RAM_bank <= 3) this->regs.RAM_bank %= this->num_RAM_banks;
        if (rb <= 3) {
            if (this->has_RAM) this->RAM_bank_offset = rb * 8192;
        } else {
            //console.log('SET TO RTC!', rb);
            // RTC registers handled during read/write ops to the area
        }
        if (this->has_RAM) this->RAM_bank_offset = rb * 8192;
    }
    else {
        rb &= 3;
        if (this->has_RAM) this->RAM_bank_offset = rb * 8192;
    }
}

u32 GBMBC3_CPU_read(GB_mapper* parent, u32 addr, u32 val, u32 has_effect)
{
    THIS;
    if (addr < 0x4000) // ROM lo bank
        return this->ROM[addr & 0x3FFF];
    if (addr < 0x8000) // ROM hi bank
        return this->ROM[(addr & 0x3FFF) + this->ROM_bank_offset_hi];
    if ((addr >= 0xA000) && (addr < 0xC000)) { // cartRAM if there
        if (!this->regs.ext_RAM_enable) return 0xFF;
        if (this->regs.RAM_bank < 4) {
            if (!this->has_RAM) return 0xFF;
            return ((u8 *)this->cart->SRAM->data)[(addr & 0x1FFF) + this->RAM_bank_offset];
        }
        if ((this->regs.RAM_bank >= 8) && (this->regs.RAM_bank <= 0x0C) && this->cart->header.timer_present)
            return this->regs.RTC_latched[this->regs.RAM_bank - 8];
        return 0xFF;
    }
    assert(1!=0);
    return 0xFF;
}

void GBMBC3_RTC_latch(GB_mapper_MBC3 *this)
{
    printf("\nNO MBC3 LATCH YET");
}

void GBMBC3_CPU_write(GB_mapper* parent, u32 addr, u32 val)
{
    THIS;
    if (addr < 0x2000) { // RAM and timer enable, write-only
        this->regs.ext_RAM_enable = (val & 0x0F) == 0x0A;
        return;
    }
    if (addr < 0x4000) {
        // 16KB hi ROM bank number, 7 bits. 0 = 1, otherwise it's normal
        val &= 0x7F;
        if (val == 0) val = 1;
        this->regs.ROM_bank_hi = val;
        GBMBC3_remap(this);
        return;
    }
    if (addr < 0x6000) { // RAM bank, 0-3. 8-C maps RTC in the same range
        this->regs.RAM_bank = val & 0x0F;
        GBMBC3_remap(this);
        return;
    }
    if (addr < 0x8000) { // // Write 0 then 1 to latch RTC clock data, no effect if no clock
        if ((this->regs.last_RTC_latch_write == 0) && (val == 1))
            GBMBC3_RTC_latch(this);
        this->regs.last_RTC_latch_write = val;
        return;
    }

    if ((addr >= 0xA000) && (addr < 0xC000)) { // cart RAM
        if ((this->has_RAM) && (this->regs.RAM_bank < 4)) {
            ((u8 *)this->cart->SRAM->data)[(addr & 0x1FFF) + this->RAM_bank_offset] = val;
            this->cart->SRAM->dirty = 1;
        }
        return;
    }
}

void GBMBC3_set_cart(GB_mapper* parent, GB_cart* cart)
{
    THIS;
    printf("Loading MBC3...");
    this->cart = cart;
    GB_bus_set_cart(this->bus, cart);

    if (this->ROM != NULL) free(this->ROM);
    this->ROM = malloc(cart->header.ROM_size);
    memcpy(this->ROM, cart->ROM, cart->header.ROM_size);

    this->has_RAM = this->cart->header.RAM_size > 0;

    this->num_RAM_banks = this->cart->header.RAM_size / 8192;
    this->num_ROM_banks = cart->header.ROM_size >> 14;
}

//
// Created by Dave on 2/2/2024.
//
//
// Created by Dave on 1/27/2024.
//

#include "stdlib.h"
#include "assert.h"
#include "stdio.h"
#include "string.h"

#include "helpers/serialize/serialize.h"

#include "mbc2.h"


#define THIS struct GB_mapper_MBC2* this = (struct GB_mapper_MBC2*)parent->ptr

static void GBMBC2_update_banks(struct GB_mapper_MBC2 *this);

static void serialize(struct GB_mapper *parent, serialized_state *state)
{
    THIS;
#define S(x) Sadd(state, &(this-> x), sizeof(this-> x))
    S(ROM_bank_hi_offset);
    S(regs.ROMB);
    S(regs.ext_RAM_enable);
#undef S
}

static void deserialize(struct GB_mapper *parent, serialized_state *state)
{
    THIS;
#define L(x) Sload(state, &(this-> x), sizeof(this-> x))
    L(ROM_bank_hi_offset);
    L(regs.ROMB);
    L(regs.ext_RAM_enable);
#undef L
}


void GB_mapper_MBC2_new(struct GB_mapper *parent, GB_clock *clock, GB_bus *bus)
{
    struct GB_mapper_MBC2 *this = (struct GB_mapper_MBC2 *)malloc(sizeof(struct GB_mapper_MBC2));
    parent->ptr = (void *)this;

    this->ROM = NULL;
    this->bus = bus;
    this->clock = clock;
    this->RAM_mask = 0;
    this->cart = NULL;

    parent->CPU_read = &GBMBC2_CPU_read;
    parent->CPU_write = &GBMBC2_CPU_write;
    parent->reset = &GBMBC2_reset;
    parent->set_cart = &GBMBC2_set_cart;
    parent->serialize = &serialize;
    parent->deserialize = &deserialize;

    this->ROM_bank_hi_offset = 16384;

    this->num_ROM_banks = 0;
    this->regs.ROMB = 1;
    this->regs.ext_RAM_enable = 0;
}

void GB_mapper_MBC2_delete(struct GB_mapper *parent)
{
    if (parent->ptr == NULL) return;
    THIS;

    if(this->ROM != NULL) {
        free(this->ROM);
        this->ROM = NULL;
    }

    free(parent->ptr);
}

void GBMBC2_reset(struct GB_mapper* parent)
{
    THIS;
    this->ROM_bank_hi_offset = 16384;

    this->num_ROM_banks = 0;
    this->regs.ROMB = 1;
    this->regs.ext_RAM_enable = 0;
    GBMBC2_update_banks(this);
}

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable: 4724) // warning C4724: potential mod by 0
#endif

static void GBMBC2_update_banks(struct GB_mapper_MBC2 *this)
{
    this->ROM_bank_hi_offset = (this->regs.ROMB % this->num_ROM_banks) * 16384;
}

#if defined(_MSC_VER)
#pragma warning(pop)
#endif

u32 GBMBC2_CPU_read(struct GB_mapper* parent, u32 addr, u32 val, u32 has_effect)
{
    THIS;
    if (addr < 0x4000) // ROM lo bank
        return this->ROM[addr];
    if (addr < 0x8000) // ROM hi bank
        return this->ROM[(addr & 0x3FFF) + this->ROM_bank_hi_offset];
    if ((addr >= 0xA000) && (addr < 0xC000)) {
        if (!this->regs.ext_RAM_enable)
            return 0xFF;
        return ((u8 *)this->cart->SRAM->data)[addr & 0x1FF] & 0x0F;
    }
    assert(1!=0);
    return 0xFF;
}

void GBMBC2_CPU_write(struct GB_mapper* parent, u32 addr, u32 val)
{
    THIS;
    if (addr < 0x4000) {
        switch (addr & 0x100) {
            case 0x0000: // RAM write enable
                this->regs.ext_RAM_enable = +((val & 0x0F) == 0x0A);
                return;
            case 0x0100: // ROM bank number
                val &= 0x0F;
                if (val == 0) val = 1;
                this->regs.ROMB = val;
                GBMBC2_update_banks(this);
                return;
        }
    }
    else if ((addr >= 0xA000) && (addr < 0xC000)) { // cart RAM
        if (this->regs.ext_RAM_enable) {
            ((u8 *)this->cart->SRAM->data)[addr & 0x1FF] = val & 0x0F;
            this->cart->SRAM->dirty = 1;
        }
        return;
    }
}

void GBMBC2_set_cart(struct GB_mapper* parent, GB_cart* cart)
{
    THIS;
    this->cart = cart;
    GB_bus_set_cart(this->bus, cart);

    if (this->ROM != NULL) free(this->ROM);
    this->ROM = malloc(cart->header.ROM_size);
    memcpy(this->ROM, cart->ROM, cart->header.ROM_size);

    this->RAM_mask = 0x1FF;

    this->num_ROM_banks = cart->header.ROM_size / 16384;
    printf("\nNUMBER OF ROM BANKS %d", this->num_ROM_banks);
}

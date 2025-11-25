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

#include "helpers/serialize/serialize.h"

#include "mbc5.h"


#define THIS struct GB_mapper_MBC5* this = (GB_mapper_MBC5*)parent->ptr

static void GBMBC5_update_banks(GB_mapper_MBC5 *this);

static void serialize(GB_mapper *parent, serialized_state *state)
{
    THIS;
#define S(x) Sadd(state, &(this-> x), sizeof(this-> x))
    S(ROM_bank_lo_offset);
    S(ROM_bank_hi_offset);
    S(regs.ROMB0);
    S(regs.ROMB1);
    S(regs.RAMB);
    S(regs.ext_RAM_enable);
    S(cartRAM_offset);
#undef S
}

static void deserialize(GB_mapper *parent, serialized_state *state)
{
    THIS;
#define L(x) Sload(state, &(this-> x), sizeof(this-> x))
    L(ROM_bank_lo_offset);
    L(ROM_bank_hi_offset);
    L(regs.ROMB0);
    L(regs.ROMB1);
    L(regs.RAMB);
    L(regs.ext_RAM_enable);
    L(cartRAM_offset);
#undef L
}


void GB_mapper_MBC5_new(GB_mapper *parent, GB_clock *clock, GB_bus *bus)
{
    struct GB_mapper_MBC5 *this = (GB_mapper_MBC5 *)malloc(sizeof(GB_mapper_MBC5));
    parent->ptr = (void *)this;

    this->ROM = NULL;
    this->bus = bus;
    this->clock = clock;
    this->RAM_mask = 0;
    this->cart = NULL;

    parent->CPU_read = &GBMBC5_CPU_read;
    parent->CPU_write = &GBMBC5_CPU_write;
    parent->reset = &GBMBC5_reset;
    parent->set_cart = &GBMBC5_set_cart;
    parent->serialize = &serialize;
    parent->deserialize = &deserialize;

    this->ROM_bank_lo_offset = 0;
    this->ROM_bank_hi_offset = 16384;

    this->num_ROM_banks = 0;
    this->num_RAM_banks = 0;
    this->regs.ROMB0 = 0;
    this->regs.ROMB1 = 0;
    this->regs.RAMB = 0;
    this->regs.ext_RAM_enable = 0;
    this->cartRAM_offset = 0;
}

void GB_mapper_MBC5_delete(GB_mapper *parent)
{
    if (parent->ptr == NULL) return;
    THIS;

    if(this->ROM != NULL) {
        free(this->ROM);
        this->ROM = NULL;
    }

    free(parent->ptr);
}

void GBMBC5_reset(GB_mapper* parent)
{
    THIS;
    this->ROM_bank_lo_offset = 0;
    this->ROM_bank_hi_offset = 16384;
    this->cartRAM_offset = 0;
    this->regs.ROMB0 = 1;
    this->regs.ROMB1 = 0;
    this->regs.ext_RAM_enable = 0;
    this->regs.RAMB = 0;
    GBMBC5_update_banks(this);
}

static void GBMBC5_update_banks(GB_mapper_MBC5 *this)
{
    this->cartRAM_offset = (this->regs.RAMB % this->num_RAM_banks) * 8192;
    this->ROM_bank_lo_offset = 0;
    this->ROM_bank_hi_offset = (((this->regs.ROMB1 << 8) | this->regs.ROMB0) % this->num_ROM_banks) * 16384;
}

u32 GBMBC5_CPU_read(GB_mapper* parent, u32 addr, u32 val, u32 has_effect)
{
    THIS;
    if (addr < 0x4000) // ROM lo bank
        return this->ROM[addr + this->ROM_bank_lo_offset];
    if (addr < 0x8000) // ROM hi bank
        return this->ROM[(addr & 0x3FFF) + this->ROM_bank_hi_offset];
    if ((addr >= 0xA000) && (addr < 0xC000)) { // cart RAM if it's there
        if ((!this->has_RAM) || (!this->regs.ext_RAM_enable))
            return 0xFF;
        return ((u8 *)this->cart->SRAM->data)[((addr - 0xA000) & this->RAM_mask) + this->cartRAM_offset];
    }
    assert(1!=0);
    return 0xFF;
}

void GBMBC5_CPU_write(GB_mapper* parent, u32 addr, u32 val)
{
    THIS;
    if (addr < 0x8000) {
        switch(addr & 0xF000) {
            case 0x0000: // RAM write enable
            case 0x1000:
                this->regs.ext_RAM_enable = val == 0x0A;
                return;
            case 0x2000: // ROM bank number0
                this->regs.ROMB0 = val;
                GBMBC5_update_banks(this);
                return;
            case 0x3000: // ROM bank number1, 1 bit
                this->regs.ROMB1 = val & 1;
                GBMBC5_update_banks(this);
                return;
            case 0x4000: // RAMB bank number, 4 bits
            case 0x5000:
                if (this->cart->header.rumble_present)
                    this->regs.RAMB = val & 0x07;
                else
                    this->regs.RAMB = val & 0x0F;
                GBMBC5_update_banks(this);
                return;
        }
    }
    if ((addr >= 0xA000) && (addr < 0xC000)) { // cart RAM
        if ((!this->has_RAM) || (!this->regs.ext_RAM_enable)) return;
        ((u8 *)this->cart->SRAM->data)[((addr - 0xA000) & this->RAM_mask) + this->cartRAM_offset] = val;
        this->cart->SRAM->dirty = 1;
    }
}

void GBMBC5_set_cart(GB_mapper* parent, GB_cart* cart)
{
    THIS;
    this->cart = cart;
    GB_bus_set_cart(this->bus, cart);

    if (this->ROM != NULL) free(this->ROM);
    this->ROM = malloc(cart->header.ROM_size);
    memcpy(this->ROM, cart->ROM, cart->header.ROM_size);

    this->num_RAM_banks = this->cart->header.RAM_size / 8192;
    this->RAM_mask = cart->header.RAM_mask;

    this->num_ROM_banks = cart->header.ROM_size / 16384;
    this->has_RAM = this->cart->header.RAM_size > 0;
    this->num_ROM_banks = this->cart->header.ROM_size / 16384;
    printf("\nNUMBER OF ROM BANKS %d", this->num_ROM_banks);
}

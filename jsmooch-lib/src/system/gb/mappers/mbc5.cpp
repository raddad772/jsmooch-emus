//
// Created by Dave on 2/2/2024.
//
//
// Created by Dave on 1/27/2024.
//

#include <cstdlib>
#include <cassert>

#include "helpers/serialize/serialize.h"

#include "mbc5.h"

namespace GB {
#define SELF struct GB_mapper_MBC5* self = (GB_mapper_MBC5*)parent->ptr

static void GBMBC5_update_banks(GB_mapper_MBC5 *self);

static void serialize(MAPPER *parent, serialized_state &state)
{
    SELF;
#define S(x) Sadd(state, &(self-> x), sizeof(self-> x))
    S(ROM_bank_lo_offset);
    S(ROM_bank_hi_offset);
    S(regs.ROMB0);
    S(regs.ROMB1);
    S(regs.RAMB);
    S(regs.ext_RAM_enable);
    S(cartRAM_offset);
#undef S
}

static void deserialize(MAPPER *parent, serialized_state &state)
{
    SELF;
#define L(x) Sload(state, &(self-> x), sizeof(self-> x))
    L(ROM_bank_lo_offset);
    L(ROM_bank_hi_offset);
    L(regs.ROMB0);
    L(regs.ROMB1);
    L(regs.RAMB);
    L(regs.ext_RAM_enable);
    L(cartRAM_offset);
#undef L
}


void GB_mapper_MBC5_new(MAPPER *parent, clock *clock, core *bus)
{
    auto *self = static_cast<GB_mapper_MBC5 *>(malloc(sizeof(GB_mapper_MBC5)));
    parent->ptr = static_cast<void *>(self);

    self->ROM = nullptr;
    self->bus = bus;
    self->clock = clock;
    self->RAM_mask = 0;
    self->cart = nullptr;

    parent->CPU_read = &GBMBC5_CPU_read;
    parent->CPU_write = &GBMBC5_CPU_write;
    parent->reset = &GBMBC5_reset;
    parent->set_cart = &GBMBC5_set_cart;
    parent->serialize = &serialize;
    parent->deserialize = &deserialize;

    self->ROM_bank_lo_offset = 0;
    self->ROM_bank_hi_offset = 16384;

    self->num_ROM_banks = 0;
    self->num_RAM_banks = 0;
    self->regs.ROMB0 = 0;
    self->regs.ROMB1 = 0;
    self->regs.RAMB = 0;
    self->regs.ext_RAM_enable = 0;
    self->cartRAM_offset = 0;
}

void GB_mapper_MBC5_delete(MAPPER *parent)
{
    if (parent->ptr == nullptr) return;
    SELF;

    if(self->ROM != nullptr) {
        free(self->ROM);
        self->ROM = nullptr;
    }

    free(parent->ptr);
}

void GBMBC5_reset(MAPPER* parent)
{
    SELF;
    self->ROM_bank_lo_offset = 0;
    self->ROM_bank_hi_offset = 16384;
    self->cartRAM_offset = 0;
    self->regs.ROMB0 = 1;
    self->regs.ROMB1 = 0;
    self->regs.ext_RAM_enable = 0;
    self->regs.RAMB = 0;
    GBMBC5_update_banks(self);
}

static void GBMBC5_update_banks(GB_mapper_MBC5 *self)
{
    self->cartRAM_offset = (self->regs.RAMB % self->num_RAM_banks) * 8192;
    self->ROM_bank_lo_offset = 0;
    self->ROM_bank_hi_offset = (((self->regs.ROMB1 << 8) | self->regs.ROMB0) % self->num_ROM_banks) * 16384;
}

u32 GBMBC5_CPU_read(MAPPER* parent, u32 addr, u32 val, u32 has_effect)
{
    SELF;
    if (addr < 0x4000) // ROM lo bank
        return self->ROM[addr + self->ROM_bank_lo_offset];
    if (addr < 0x8000) // ROM hi bank
        return self->ROM[(addr & 0x3FFF) + self->ROM_bank_hi_offset];
    if ((addr >= 0xA000) && (addr < 0xC000)) { // cart RAM if it's there
        if ((!self->has_RAM) || (!self->regs.ext_RAM_enable))
            return 0xFF;
        return ((u8 *)self->cart->SRAM->data)[((addr - 0xA000) & self->RAM_mask) + self->cartRAM_offset];
    }
    assert(1!=0);
    return 0xFF;
}

void GBMBC5_CPU_write(MAPPER* parent, u32 addr, u32 val)
{
    SELF;
    if (addr < 0x8000) {
        switch(addr & 0xF000) {
            case 0x0000: // RAM write enable
            case 0x1000:
                self->regs.ext_RAM_enable = val == 0x0A;
                return;
            case 0x2000: // ROM bank number0
                self->regs.ROMB0 = val;
                GBMBC5_update_banks(self);
                return;
            case 0x3000: // ROM bank number1, 1 bit
                self->regs.ROMB1 = val & 1;
                GBMBC5_update_banks(self);
                return;
            case 0x4000: // RAMB bank number, 4 bits
            case 0x5000:
                if (self->cart->header.rumble_present)
                    self->regs.RAMB = val & 0x07;
                else
                    self->regs.RAMB = val & 0x0F;
                GBMBC5_update_banks(self);
                return;
        }
    }
    if ((addr >= 0xA000) && (addr < 0xC000)) { // cart RAM
        if ((!self->has_RAM) || (!self->regs.ext_RAM_enable)) return;
        ((u8 *)self->cart->SRAM->data)[((addr - 0xA000) & self->RAM_mask) + self->cartRAM_offset] = val;
        self->cart->SRAM->dirty = true;
    }
}

void GBMBC5_set_cart(MAPPER* parent, cart* cart)
{
    SELF;
    self->cart = cart;

    if (self->ROM != nullptr) free(self->ROM);
    self->ROM = static_cast<u8 *>(malloc(cart->header.ROM_size));
    memcpy(self->ROM, cart->ROM, cart->header.ROM_size);

    self->num_RAM_banks = self->cart->header.RAM_size / 8192;
    self->RAM_mask = cart->header.RAM_mask;

    self->num_ROM_banks = cart->header.ROM_size / 16384;
    self->has_RAM = self->cart->header.RAM_size > 0;
    self->num_ROM_banks = self->cart->header.ROM_size / 16384;
    printf("\nNUMBER OF ROM BANKS %d", self->num_ROM_banks);
}
}
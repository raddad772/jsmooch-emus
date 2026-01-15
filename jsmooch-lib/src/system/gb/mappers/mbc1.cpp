//
// Created by Dave on 1/27/2024.
//

#include <cstdlib>
#include <cassert>
#include "stdio.h"
#include <cstring>

#include "helpers/serialize/serialize.h"
#include "mbc1.h"

#define SELF GB_mapper_MBC1* self = static_cast<GB_mapper_MBC1*>(parent->ptr)
namespace GB {
static void GBMBC1_update_banks(GB_mapper_MBC1 *self);

static void serialize(MAPPER *parent, serialized_state &state)
{
    SELF;
#define S(x) Sadd(state, &(self->x), sizeof(self->x))
    S(ROM_bank_lo_offset);
    S(ROM_bank_hi_offset);
    S(regs.banking_mode);
    S(regs.BANK1);
    S(regs.BANK2);
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
    L(regs.banking_mode);
    L(regs.BANK1);
    L(regs.BANK2);
    L(regs.ext_RAM_enable);
    L(cartRAM_offset);
#undef L
}



void GB_mapper_MBC1_new(MAPPER *parent, clock *clock, core *bus)
{
    GB_mapper_MBC1 *self = (GB_mapper_MBC1 *)malloc(sizeof(GB_mapper_MBC1));
    parent->ptr = (void *)self;

    self->ROM = nullptr;
    self->bus = bus;
    self->clock = clock;
    self->RAM_mask = 0;
    self->has_RAM = false;
    self->cart = nullptr;

    parent->CPU_read = &GBMBC1_CPU_read;
    parent->CPU_write = &GBMBC1_CPU_write;
    parent->reset = &GBMBC1_reset;
    parent->set_cart = &GBMBC1_set_cart;
    parent->serialize = &serialize;
    parent->deserialize = &deserialize;

    self->ROM_bank_lo_offset = 0;
    self->ROM_bank_hi_offset = 16384;

    self->num_ROM_banks = 0;
    self->num_RAM_banks = 1;
    self->regs.banking_mode = 0;
    self->regs.BANK1 = 1;
    self->regs.BANK2 = 0;
    self->cartRAM_offset = 0;
    self->regs.ext_RAM_enable = 0;
}

void GB_mapper_MBC1_delete(MAPPER *parent)
{
    if (parent->ptr == nullptr) return;
    SELF;

    if(self->ROM != nullptr) {
        free(self->ROM);
        self->ROM = nullptr;
    }

    free(parent->ptr);
}

void GBMBC1_reset(MAPPER* parent)
{
    SELF;
    self->ROM_bank_lo_offset = 0;
    self->ROM_bank_hi_offset = 16384;

    self->num_ROM_banks = 0;
    self->num_RAM_banks = 1;
    self->regs.banking_mode = 0;
    self->regs.BANK1 = 1;
    self->regs.BANK2 = 0;
    self->regs.ext_RAM_enable = 0;
    self->cartRAM_offset = 0;
    GBMBC1_update_banks(self);
}

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable: 4724) // warning C4724: potential mod by 0
#endif

static void GBMBC1_update_banks(GB_mapper_MBC1 *self)
{
    if (self->regs.banking_mode == 0) {
        // Mode 0, easy-mode
        self->ROM_bank_lo_offset = 0;
        self->cartRAM_offset = 0;
    } else {
        // Mode 1, hard-mode!
        self->ROM_bank_lo_offset = ((self->regs.BANK2 << 5) % self->num_ROM_banks) << 14;
        self->cartRAM_offset = (self->regs.BANK2 % self->num_RAM_banks) << 13;
    }
    self->ROM_bank_hi_offset = (((self->regs.BANK2 << 5) | self->regs.BANK1) % self->num_ROM_banks) << 14;
}

#if defined(_MSC_VER)
#pragma warning(pop)
#endif

u32 GBMBC1_CPU_read(MAPPER* parent, u32 addr, u32 val, u32 has_effect)
{
    SELF;
    if (addr < 0x4000) // ROM lo bank
        return self->ROM[addr + self->ROM_bank_lo_offset];
    if (addr < 0x8000) // ROM hi bank
        return self->ROM[(addr & 0x3FFF) + self->ROM_bank_hi_offset];
    if ((addr >= 0xA000) && (addr < 0xC000)) {
        if ((!self->has_RAM) || (!self->regs.ext_RAM_enable))
            return 0xFF;
        return static_cast<u8 *>(self->cart->SRAM->data)[((addr - 0xA000) & self->RAM_mask) + self->cartRAM_offset];
    }
    assert(1!=0);
    return 0xFF;
}

void GBMBC1_CPU_write(MAPPER* parent, u32 addr, u32 val)
{
    SELF;
    if (addr < 0x8000) {
        switch (addr & 0xE000) {
            case 0x0000: // RAM write enable
                self->regs.ext_RAM_enable = +((val & 0x0F) == 0x0A);
                return;
            case 0x2000: // ROM bank number
                val &= 0x1F; // 5 bits
                if (val == 0) val = 1; // can't be 0
                self->regs.BANK1 = val;
                GBMBC1_update_banks(self);
                return;
            case 0x4000: // RAM or ROM banks...
                self->regs.BANK2 = val & 3;
                GBMBC1_update_banks(self);
                return;
            case 0x6000: // Control
                self->regs.banking_mode = val & 1;
                GBMBC1_update_banks(self);
                return;
        }
    }
    if ((addr >= 0xA000) && (addr < 0xC000)) { // cart RAM
        if (self->has_RAM && self->regs.ext_RAM_enable) {
            static_cast<u8 *>(self->cart->SRAM->data)[((addr - 0xA000) & self->RAM_mask) + self->cartRAM_offset] = val;
            self->cart->SRAM->dirty = true;
        }
        return;
    }
}

void GBMBC1_set_cart(MAPPER* parent, cart* cart)
{
    SELF;
    self->cart = cart;

    if (self->ROM != nullptr) free(self->ROM);
    self->ROM = static_cast<u8 *>(malloc(cart->header.ROM_size));
    memcpy(self->ROM, cart->ROM, cart->header.ROM_size);

    self->num_RAM_banks = (cart->header.RAM_size / 8192);

    self->RAM_mask = cart->header.RAM_mask;
    self->has_RAM = cart->header.RAM_size > 0;

    self->num_ROM_banks = cart->header.ROM_size / 16384;
}
}
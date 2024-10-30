//
// Created by Dave on 1/27/2024.
//

#include "stdlib.h"
#include "assert.h"
#include "stdio.h"
#include "string.h"

#include "mbc1.h"


#define THIS struct GB_mapper_MBC1* this = (struct GB_mapper_MBC1*)parent->ptr

static void GBMBC1_update_banks(struct GB_mapper_MBC1 *this);

void GB_mapper_MBC1_new(struct GB_mapper *parent, struct GB_clock *clock, struct GB_bus *bus)
{
    struct GB_mapper_MBC1 *this = (struct GB_mapper_MBC1 *)malloc(sizeof(struct GB_mapper_MBC1));
    parent->ptr = (void *)this;

    this->ROM = NULL;
    this->bus = bus;
    this->clock = clock;
    this->ROM_bank_offset = 16384;
    this->cartRAM = NULL;
    this->RAM_mask = 0;
    this->has_RAM = false;
    this->cart = NULL;

    parent->CPU_read = &GBMBC1_CPU_read;
    parent->CPU_write = &GBMBC1_CPU_write;
    parent->reset = &GBMBC1_reset;
    parent->set_cart = &GBMBC1_set_cart;

    this->ROM_bank_lo_offset = 0;
    this->ROM_bank_hi_offset = 16384;

    this->num_ROM_banks = 0;
    this->num_RAM_banks = 1;
    this->regs.banking_mode = 0;
    this->regs.BANK1 = 1;
    this->regs.BANK2 = 0;
    this->cartRAM_offset = 0;
    this->regs.ext_RAM_enable = 0;
}

void GB_mapper_MBC1_delete(struct GB_mapper *parent)
{
    if (parent->ptr == NULL) return;
    THIS;

    if(this->ROM != NULL) {
        free(this->ROM);
        this->ROM = NULL;
    }

    if (this->cartRAM != NULL) {
        free(this->cartRAM);
        this->cartRAM = NULL;
    }

    free(parent->ptr);
}

void GBMBC1_reset(struct GB_mapper* parent)
{
    THIS;
    this->ROM_bank_lo_offset = 0;
    this->ROM_bank_hi_offset = 16384;

    this->num_ROM_banks = 0;
    this->num_RAM_banks = 1;
    this->regs.banking_mode = 0;
    this->regs.BANK1 = 1;
    this->regs.BANK2 = 0;
    this->regs.ext_RAM_enable = 0;
    this->cartRAM_offset = 0;
    GBMBC1_update_banks(this);
}

static void GBMBC1_update_banks(struct GB_mapper_MBC1 *this)
{
    if (this->regs.banking_mode == 0) {
        // Mode 0, easy-mode
        this->ROM_bank_lo_offset = 0;
        this->cartRAM_offset = 0;
    } else {
        // Mode 1, hard-mode!
        this->ROM_bank_lo_offset = ((this->regs.BANK2 << 5) % this->num_ROM_banks) << 14;
        this->cartRAM_offset = (this->regs.BANK2 % this->num_RAM_banks) << 13;
    }
    this->ROM_bank_hi_offset = (((this->regs.BANK2 << 5) | this->regs.BANK1) % this->num_ROM_banks) << 14;
}

u32 GBMBC1_CPU_read(struct GB_mapper* parent, u32 addr, u32 val, u32 has_effect)
{
    THIS;
    if (addr < 0x4000) // ROM lo bank
        return this->ROM[addr + this->ROM_bank_lo_offset];
    if (addr < 0x8000) // ROM hi bank
        return this->ROM[(addr & 0x3FFF) + this->ROM_bank_hi_offset];
    if ((addr >= 0xA000) && (addr < 0xC000)) {
        if ((!this->has_RAM) || (!this->regs.ext_RAM_enable))
            return 0xFF;
        return this->cartRAM[((addr - 0xA000) & this->RAM_mask) + this->cartRAM_offset];
    }
    assert(1!=0);
    return 0xFF;
}

void GBMBC1_CPU_write(struct GB_mapper* parent, u32 addr, u32 val)
{
    THIS;
    if (addr < 0x8000) {
        switch (addr & 0xE000) {
            case 0x0000: // RAM write enable
                this->regs.ext_RAM_enable = +((val & 0x0F) == 0x0A);
                return;
            case 0x2000: // ROM bank number
                val &= 0x1F; // 5 bits
                if (val == 0) val = 1; // can't be 0
                this->regs.BANK1 = val;
                GBMBC1_update_banks(this);
                return;
            case 0x4000: // RAM or ROM banks...
                this->regs.BANK2 = val & 3;
                GBMBC1_update_banks(this);
                return;
            case 0x6000: // Control
                this->regs.banking_mode = val & 1;
                GBMBC1_update_banks(this);
                return;
        }
    }
    if ((addr >= 0xA000) && (addr < 0xC000)) { // cart RAM
        if (this->has_RAM && this->regs.ext_RAM_enable)
            this->cartRAM[((addr - 0xA000) & this->RAM_mask) + this->cartRAM_offset] = val;
        return;
    }
}

void GBMBC1_set_cart(struct GB_mapper* parent, struct GB_cart* cart)
{
    THIS;
    this->cart = cart;
    GB_bus_set_cart(this->bus, cart);

    if (this->ROM != NULL) free(this->ROM);
    this->ROM = malloc(cart->header.ROM_size);
    memcpy(this->ROM, cart->ROM, cart->header.ROM_size);

    this->num_RAM_banks = (cart->header.RAM_size / 8192);
    if (this->cartRAM != NULL) {
        free(this->cartRAM);
        this->cartRAM = NULL;
    }
    if (cart->header.RAM_size > 0) {
        this->cartRAM = malloc(cart->header.RAM_size);
    }

    this->RAM_mask = cart->header.RAM_mask;
    this->has_RAM = cart->header.RAM_size > 0;

    this->num_ROM_banks = cart->header.ROM_size / 16384;
}

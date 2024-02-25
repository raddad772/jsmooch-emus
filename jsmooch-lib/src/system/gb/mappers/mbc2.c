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


#include "mbc2.h"


#define THIS struct GB_mapper_MBC2* this = (struct GB_mapper_MBC2*)parent->ptr

static void GBMBC2_update_banks(struct GB_mapper_MBC2 *this);


void GB_mapper_MBC2_new(struct GB_mapper *parent, struct GB_clock *clock, struct GB_bus *bus)
{
    struct GB_mapper_MBC2 *this = (struct GB_mapper_MBC2 *)malloc(sizeof(struct GB_mapper_MBC2));
    parent->ptr = (void *)this;

    this->ROM = NULL;
    this->bus = bus;
    this->clock = clock;
    this->ROM_bank_offset = 16384;
    this->cartRAM = NULL;
    this->RAM_mask = 0;
    this->cart = NULL;

    parent->CPU_read = &GBMBC2_CPU_read;
    parent->CPU_write = &GBMBC2_CPU_write;
    parent->reset = &GBMBC2_reset;
    parent->set_cart = &GBMBC2_set_cart;

    this->ROM_bank_lo_offset = 0;
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

    if (this->cartRAM != NULL) {
        free(this->cartRAM);
        this->cartRAM = NULL;
    }

    free(parent->ptr);
}

void GBMBC2_reset(struct GB_mapper* parent)
{
    THIS;
    this->ROM_bank_lo_offset = 0;
    this->ROM_bank_hi_offset = 16384;

    this->num_ROM_banks = 0;
    this->regs.ROMB = 1;
    this->regs.ext_RAM_enable = 0;
    GBMBC2_update_banks(this);
}

static void GBMBC2_update_banks(struct GB_mapper_MBC2 *this)
{
    this->ROM_bank_lo_offset = 0;
    this->ROM_bank_hi_offset = (this->regs.ROMB % this->num_ROM_banks) * 16384;
}

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
        return this->cartRAM[addr & 0x1FF] & 0x0F;
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
        if (this->regs.ext_RAM_enable)
            this->cartRAM[addr & 0x1FF] = val & 0x0F;
        return;
    }
}

void GBMBC2_set_cart(struct GB_mapper* parent, struct GB_cart* cart)
{
    THIS;
    this->cart = cart;
    GB_bus_set_cart(this->bus, cart);

    if (this->ROM != NULL) free(this->ROM);
    this->ROM = malloc(cart->header.ROM_size);
    memcpy(this->ROM, cart->ROM, cart->header.ROM_size);

    if (this->cartRAM != NULL) {
        free(this->cartRAM);
        this->cartRAM = NULL;
    }
    this->cartRAM = malloc(512);
    this->RAM_mask = 0x1FF;

    this->num_ROM_banks = cart->header.ROM_size / 16384;
    printf("\nNUMBER OF ROM BANKS %d", this->num_ROM_banks);
}

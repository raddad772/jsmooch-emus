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


#include "mbc5.h"


#define THIS struct GB_mapper_MBC5* this = (struct GB_mapper_MBC5*)parent->ptr

static void GBMBC5_update_banks(struct GB_mapper_MBC5 *this);

void GB_mapper_MBC5_new(struct GB_mapper *parent, struct GB_clock *clock, struct GB_bus *bus)
{
    struct GB_mapper_MBC5 *this = (struct GB_mapper_MBC5 *)malloc(sizeof(struct GB_mapper_MBC5));
    parent->ptr = (void *)this;

    this->ROM = NULL;
    this->bus = bus;
    this->clock = clock;
    this->ROM_bank_offset = 16384;
    this->cartRAM = NULL;
    this->RAM_mask = 0;
    this->cart = NULL;

    parent->CPU_read = &GBMBC5_CPU_read;
    parent->CPU_write = &GBMBC5_CPU_write;
    parent->reset = &GBMBC5_reset;
    parent->set_cart = &GBMBC5_set_cart;

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

void GB_mapper_MBC5_delete(struct GB_mapper *parent)
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

void GBMBC5_reset(struct GB_mapper* parent)
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

static void GBMBC5_update_banks(struct GB_mapper_MBC5 *this)
{
    this->cartRAM_offset = (this->regs.RAMB % this->num_RAM_banks) * 8192;
    this->ROM_bank_lo_offset = 0;
    this->ROM_bank_hi_offset = (((this->regs.ROMB1 << 8) | this->regs.ROMB0) % this->num_ROM_banks) * 16384;
}

u32 GBMBC5_CPU_read(struct GB_mapper* parent, u32 addr, u32 val, u32 has_effect)
{
    THIS;
    if (addr < 0x4000) // ROM lo bank
        return this->ROM[addr + this->ROM_bank_lo_offset];
    if (addr < 0x8000) // ROM hi bank
        return this->ROM[(addr & 0x3FFF) + this->ROM_bank_hi_offset];
    if ((addr >= 0xA000) && (addr < 0xC000)) { // cart RAM if it's there
        if ((!this->has_RAM) || (!this->regs.ext_RAM_enable))
            return 0xFF;
        return this->cartRAM[((addr - 0xA000) & this->RAM_mask) + this->cartRAM_offset];
    }
    assert(1!=0);
    return 0xFF;
}

void GBMBC5_CPU_write(struct GB_mapper* parent, u32 addr, u32 val)
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
        this->cartRAM[((addr - 0xA000) & this->RAM_mask) + this->cartRAM_offset] = val;
    }
}

void GBMBC5_set_cart(struct GB_mapper* parent, struct GB_cart* cart)
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
    this->cartRAM = malloc(this->cart->header.RAM_size);

    this->num_RAM_banks = this->cart->header.RAM_size / 8192;

    this->num_ROM_banks = cart->header.ROM_size / 16384;
    this->has_RAM = this->cart->header.RAM_size > 0;
    this->num_ROM_banks = this->cart->header.ROM_size / 16384;
    printf("\nNUMBER OF ROM BANKS %d", this->num_ROM_banks);
}

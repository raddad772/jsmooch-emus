#include <string.h>
#include "stdlib.h"

#include "system/gb/mappers/mapper.h"
#include "system/gb/mappers/no_mapper.h"
#include "../gb_clock.h"
#include "../gb_bus.h"
#include "../cart.h"
#include <stdio.h>

#define THIS struct GB_mapper_none* this = (struct GB_mapper_none*)parent->ptr

void GB_mapper_none_new(struct GB_mapper *parent, struct GB_clock *clock, struct GB_bus *bus)
{
    struct GB_mapper_none *this = (struct GB_mapper_none *)malloc(sizeof(struct GB_mapper_none));
    parent->ptr = (void *)this;
    this->ROM = NULL;
    this->bus = bus;
    this->clock = clock;
    this->ROM_bank_offset = 16384;
    this->cartRAM = NULL;
    this->RAM_mask = 0;
    this->has_RAM = false;
    this->cart = NULL;

    parent->CPU_read = &GBMN_CPU_read;
    parent->CPU_write = &GBMN_CPU_write;
    parent->reset = &GBMN_reset;
    parent->set_cart = &GBMN_set_cart;
}

void GB_mapper_none_delete(struct GB_mapper *parent)
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

void GBMN_reset(struct GB_mapper* parent)
{
    THIS;
    this->ROM_bank_offset = 16384;
    GB_bus_reset(this->bus);
}

void GBMN_set_cart(struct GB_mapper* parent, struct GB_cart* cart)
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

    if (cart->header.RAM_size > 0) {
        this->cartRAM = malloc(cart->header.RAM_size);
    }
    this->has_RAM = cart->header.RAM_size > 0;
}

u32 GBMN_CPU_read(struct GB_mapper* parent, u32 addr, u32 val, u32 has_effect)
{
    THIS;
    if (addr < 0x4000) { // ROM lo bank
        return this->ROM[addr];
    }
    if (addr < 0x8000) // ROM hi bank
        return this->ROM[(addr & 0x3FFF) + this->ROM_bank_offset];
    if ((addr >= 0xA000) && (addr < 0xC000)) { // // cart RAM if it's there
        if (!this->has_RAM) return 0xFF;
        return this->cartRAM[(addr - 0xA000) & this->RAM_mask];
    }
    return val;
}

void GBMN_CPU_write(struct GB_mapper* parent, u32 addr, u32 val)
{
    THIS;
    if ((addr >= 0xA000) && (addr < 0xC000)) { // cart RAM
        if (!this->has_RAM) return;
        this->cartRAM[(addr - 0xA000) & this->RAM_mask] = val;
        return;
    }
}

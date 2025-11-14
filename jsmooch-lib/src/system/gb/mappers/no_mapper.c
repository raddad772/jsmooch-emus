#include <cstring>
#include <cstdio>
#include <cstdlib>

#include "helpers/serialize/serialize.h"

#include "system/gb/mappers/mapper.h"
#include "system/gb/mappers/no_mapper.h"
#include "../gb_clock.h"
#include "../gb_bus.h"
#include "../cart.h"

#define THIS struct GB_mapper_none* this = (GB_mapper_none*)parent->ptr

static void serialize(GB_mapper *parent, serialized_state *state)
{
    THIS;
#define S(x) Sadd(state, &(this-> x), sizeof(this-> x))
    S(ROM_bank_offset);
#undef S
}

static void deserialize(GB_mapper *parent, serialized_state *state)
{
    THIS;
#define L(x) Sload(state, &(this-> x), sizeof(this-> x))
    L(ROM_bank_offset);
#undef L
}


void GB_mapper_none_new(GB_mapper *parent, GB_clock *clock, GB_bus *bus)
{
    struct GB_mapper_none *this = (GB_mapper_none *)malloc(sizeof(GB_mapper_none));
    parent->ptr = (void *)this;
    this->ROM = NULL;
    this->bus = bus;
    this->clock = clock;
    this->ROM_bank_offset = 16384;
    this->RAM_mask = 0;
    this->has_RAM = false;
    this->cart = NULL;

    parent->CPU_read = &GBMN_CPU_read;
    parent->CPU_write = &GBMN_CPU_write;
    parent->reset = &GBMN_reset;
    parent->set_cart = &GBMN_set_cart;
    parent->serialize = &serialize;
    parent->deserialize = &deserialize;
}

void GB_mapper_none_delete(GB_mapper *parent)
{
    if (parent->ptr == NULL) return;
    THIS;

    if(this->ROM != NULL) {
        free(this->ROM);
        this->ROM = NULL;
    }

    free(parent->ptr);
}

void GBMN_reset(GB_mapper* parent)
{
    THIS;
    this->ROM_bank_offset = 16384;
    GB_bus_reset(this->bus);
}

void GBMN_set_cart(GB_mapper* parent, GB_cart* cart)
{
    THIS;
    this->cart = cart;
    GB_bus_set_cart(this->bus, cart);

    if (this->ROM != NULL) free(this->ROM);
    this->ROM = malloc(cart->header.ROM_size);
    memcpy(this->ROM, cart->ROM, cart->header.ROM_size);

    this->has_RAM = cart->header.RAM_size > 0;
}

u32 GBMN_CPU_read(GB_mapper* parent, u32 addr, u32 val, u32 has_effect)
{
    THIS;
    if (addr < 0x4000) { // ROM lo bank
        return this->ROM[addr];
    }
    if (addr < 0x8000) // ROM hi bank
        return this->ROM[(addr & 0x3FFF) + this->ROM_bank_offset];
    if ((addr >= 0xA000) && (addr < 0xC000)) { // // cart RAM if it's there
        if (!this->has_RAM) return 0xFF;
        return ((u8 *)this->cart->SRAM->data)[(addr - 0xA000) & this->RAM_mask];
    }
    return val;
}

void GBMN_CPU_write(GB_mapper* parent, u32 addr, u32 val)
{
    THIS;
    if ((addr >= 0xA000) && (addr < 0xC000)) { // cart RAM
        if (!this->has_RAM) return;
        ((u8 *)this->cart->SRAM->data)[(addr - 0xA000) & this->RAM_mask] = val;
        this->cart->SRAM->dirty = 1;
        return;
    }
}

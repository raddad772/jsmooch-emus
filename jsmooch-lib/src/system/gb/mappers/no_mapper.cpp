#include <cstring>
#include <cstdio>
#include <cstdlib>

#include "helpers/serialize/serialize.h"

#include "../gb_clock.h"
#include "../gb_bus.h"
#include "../cart.h"
#include "system/gb/mappers/mapper.h"
#include "system/gb/mappers/no_mapper.h"

#define SELF struct GB_mapper_none* self = (GB_mapper_none*)parent->ptr
namespace GB {
static void serialize(MAPPER *parent, serialized_state &state)
{
    SELF;
#define S(x) Sadd(state, &(self-> x), sizeof(self-> x))
    S(ROM_bank_offset);
#undef S
}

static void deserialize(MAPPER *parent, serialized_state &state)
{
    SELF;
#define L(x) Sload(state, &(self-> x), sizeof(self-> x))
    L(ROM_bank_offset);
#undef L
}


void GB_mapper_none_new(MAPPER *parent, clock *clock, core *bus)
{
    auto *self = static_cast<GB_mapper_none *>(malloc(sizeof(GB_mapper_none)));
    parent->ptr = static_cast<void *>(self);
    self->ROM = nullptr;
    self->bus = bus;
    self->clock = clock;
    self->ROM_bank_offset = 16384;
    self->RAM_mask = 0;
    self->has_RAM = false;
    self->cart = nullptr;

    parent->CPU_read = &GBMN_CPU_read;
    parent->CPU_write = &GBMN_CPU_write;
    parent->reset = &GBMN_reset;
    parent->set_cart = &GBMN_set_cart;
    parent->serialize = &serialize;
    parent->deserialize = &deserialize;
}

void GB_mapper_none_delete(MAPPER *parent)
{
    if (parent->ptr == nullptr) return;
    SELF;

    if(self->ROM != nullptr) {
        free(self->ROM);
        self->ROM = nullptr;
    }

    free(parent->ptr);
}

void GBMN_reset(MAPPER* parent)
{
    SELF;
    self->ROM_bank_offset = 16384;
    //self->bus->reset();
}

void GBMN_set_cart(MAPPER* parent, cart* cart)
{
    SELF;
    self->cart = cart;

    if (self->ROM != nullptr) free(self->ROM);
    self->ROM = static_cast<u8 *>(malloc(cart->header.ROM_size));
    memcpy(self->ROM, cart->ROM, cart->header.ROM_size);

    self->has_RAM = cart->header.RAM_size > 0;
}

u32 GBMN_CPU_read(MAPPER* parent, u32 addr, u32 val, u32 has_effect)
{
    SELF;
    if (addr < 0x4000) { // ROM lo bank
        return self->ROM[addr];
    }
    if (addr < 0x8000) // ROM hi bank
        return self->ROM[(addr & 0x3FFF) + self->ROM_bank_offset];
    if ((addr >= 0xA000) && (addr < 0xC000)) { // // cart RAM if it's there
        if (!self->has_RAM) return 0xFF;
        return static_cast<u8 *>(self->cart->SRAM->data)[(addr - 0xA000) & self->RAM_mask];
    }
    return val;
}

void GBMN_CPU_write(MAPPER* parent, u32 addr, u32 val)
{
    SELF;
    if ((addr >= 0xA000) && (addr < 0xC000)) { // cart RAM
        if (!self->has_RAM) return;
        static_cast<u8 *>(self->cart->SRAM->data)[(addr - 0xA000) & self->RAM_mask] = val;
        self->cart->SRAM->dirty = true;
        return;
    }
}
}
//
// Created by . on 9/27/24.
//

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include "helpers/debugger/debugger.h"

#include "system/nes/nes.h"

#include "mapper.h"
#include "uxrom.h"

#define THISM struct UXROM *this = (struct UXROM *)bus->ptr

struct UXROM {
    struct NES *nes;

    struct {
        u32 bank_num;
    } io;
};

#define READONLY 1
#define READWRITE 0

static void remap(struct NES_mapper *bus, u32 is_boot)
{
    THISM;
    NES_bus_map_PRG16K(bus, 0x8000, 0xBFFF, &bus->PRG_ROM, this->io.bank_num, READONLY);
    if (is_boot) {
        NES_bus_map_PRG16K(bus, 0xC000, 0xFFFF, &bus->PRG_ROM, bus->num_PRG_ROM_banks16K - 1, READONLY);
        NES_bus_map_CHR8K(bus, 0x0000, 0x1FFF, &bus->CHR_RAM, 0, READWRITE);
    }
}

static void serialize(struct NES_mapper *bus, struct serialized_state *state)
{
    THISM;
#define S(x) Sadd(state, &this-> x, sizeof(this-> x))
    S(io.bank_num);
#undef S
}

static void deserialize(struct NES_mapper *bus, struct serialized_state *state)
{
    THISM;
#define L(x) Sload(state, &this-> x, sizeof(this-> x))
    L(io.bank_num);
#undef L
    remap(bus, 0);
}

static void UXROM_destruct(struct NES_mapper *bus)
{

}

static void UXROM_reset(struct NES_mapper *bus)
{
    printf("\nUXROM Resetting, so remapping bus...");
    remap(bus, 1);
}

static void UXROM_writecart(struct NES_mapper *bus, u32 addr, u32 val, u32 *do_write)
{
    THISM;
    *do_write = 1;
    if (addr >= 0x8000) {
        u32 old_num = this->io.bank_num;
        this->io.bank_num = val % bus->num_PRG_ROM_banks16K;
        remap(bus, 0);
    }
}

static u32 UXROM_readcart(struct NES_mapper *bus, u32 addr, u32 old_val, u32 has_effect, u32 *do_read)
{
    *do_read = 1;
    return old_val;
}

static void UXROM_setcart(struct NES_mapper *bus, struct NES_cart *cart)
{
    bus->ppu_mirror_mode = cart->header.mirroring;
    NES_bus_PPU_mirror_set(bus);
}

void UXROM_init(struct NES_mapper *bus, struct NES *nes)
{
    if (bus->ptr != NULL) free(bus->ptr);
    bus->ptr = malloc(sizeof(struct UXROM));
    struct UXROM *this = (struct UXROM*)bus->ptr;

    this->io.bank_num = 0;

    this->nes = nes;

    bus->destruct = &UXROM_destruct;
    bus->reset = &UXROM_reset;
    bus->writecart = &UXROM_writecart;
    bus->readcart = &UXROM_readcart;
    bus->setcart = &UXROM_setcart;
    bus->cpu_cycle = NULL;
    bus->a12_watch = NULL;
    bus->serialize = &serialize;
    bus->deserialize = &deserialize;

}
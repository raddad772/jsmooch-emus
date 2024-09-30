//
// Created by . on 9/27/24.
//

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include "mapper.h"
#include "nrom.h"

#define THISM struct NROM *this = (struct NROM *)bus->ptr

struct NROM {
    struct NES *nes;
    // Literally no state is needed for this actually...
};

#define READONLY 1
#define READWRITE 0

static void remap(struct NES_bus *bus)
{
    THISM;
    NES_bus_map_PRG32K(bus, 0x8000, 0xFFFF, &bus->PRG_ROM, 0, READONLY);
    NES_bus_map_CHR1K(bus, 0x0000, 0x1FFF, &bus->CHR_ROM, 0, READONLY);
    NES_bus_PPU_mirror_set(bus);
}


static void NROM_destruct(struct NES_bus *bus)
{

}

static void NROM_reset(struct NES_bus *bus)
{
    printf("\nNROM Resetting, so remapping bus...");
    remap(bus);
}

static void NROM_writecart(struct NES_bus *bus, u32 addr, u32 val, u32 *do_write)
{
    *do_write = 1;
}

static u32 NROM_readcart(struct NES_bus *bus, u32 addr, u32 old_val, u32 has_effect, u32 *do_read)
{
    *do_read = 1;
    return old_val;
}

static void NROM_setcart(struct NES_bus *bus, struct NES_cart *cart)
{
    bus->ppu_mirror_mode = cart->header.mirroring;
}

void NROM_init(struct NES_bus *bus, struct NES *nes)
{
    if (bus->ptr != NULL) free(bus->ptr);
    bus->ptr = malloc(sizeof(struct NROM));
    struct NROM *this = (struct NROM*)bus->ptr;

    this->nes = nes;

    bus->destruct = &NROM_destruct;
    bus->reset = &NROM_reset;
    bus->writecart = &NROM_writecart;
    bus->readcart = &NROM_readcart;
    bus->setcart = &NROM_setcart;
    bus->cpu_cycle = NULL;
    bus->a12_watch = NULL;
}
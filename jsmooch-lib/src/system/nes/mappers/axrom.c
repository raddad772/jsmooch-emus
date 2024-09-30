//
// Created by . on 9/29/24.
//


//
// Created by . on 9/27/24.
//

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include "system/nes/nes.h"
#include "helpers/debugger/debugger.h"

#include "mapper.h"
#include "axrom.h"

#define THISM struct NROM *this = (struct NROM *)bus->ptr

struct NROM {
    struct NES *nes;
    // Literally no state is needed for this actually...

    struct {
        u32 PRG_bank;
        u32 nametable;
    } io;

};

#define READONLY 1
#define READWRITE 0

static void remap(struct NES_bus *bus)
{
    THISM;
    NES_bus_map_PRG32K(bus, 0x8000, 0xFFFF, &bus->PRG_ROM, this->io.PRG_bank, READONLY);
    NES_bus_PPU_mirror_set(bus);
}


static void AXROM_destruct(struct NES_bus *bus)
{

}

static void AXROM_reset(struct NES_bus *bus)
{
    THISM;
    printf("\nAXROM Resetting, so remapping bus...");
    this->io.PRG_bank = bus->num_PRG_ROM_banks32K - 1; // so like we're 7/8. = 14/16, 28/32.
    bus->ppu_mirror_mode = this->io.nametable ? PPUM_ScreenBOnly : PPUM_ScreenAOnly;
    remap(bus);

    NES_bus_map_CHR8K(bus, 0x0000, 0x1FFF, &bus->CHR_RAM, 0, READWRITE);
}

static void AXROM_writecart(struct NES_bus *bus, u32 addr, u32 val, u32 *do_write)
{
    THISM;
    if (addr >= 0x8000) {
        if (this->io.PRG_bank != (val & 7)) {
            debugger_interface_dirty_mem(bus->nes->dbg.interface, NESMEM_CPUBUS, 0x8000, 0xFFFF);
        }
        this->io.PRG_bank = val & 7;
        this->io.nametable = (val >> 4) & 1;
        remap(bus);
    }
    *do_write = 1;
}

static u32 AXROM_readcart(struct NES_bus *bus, u32 addr, u32 old_val, u32 has_effect, u32 *do_read)
{
    *do_read = 1;
    return old_val;
}

static void AXROM_setcart(struct NES_bus *bus, struct NES_cart *cart)
{
    bus->ppu_mirror_mode = PPUM_ScreenAOnly;
    NES_bus_PPU_mirror_set(bus);
}

void AXROM_init(struct NES_bus *bus, struct NES *nes)
{
    if (bus->ptr != NULL) free(bus->ptr);
    bus->ptr = malloc(sizeof(struct NROM));
    struct NROM *this = (struct NROM*)bus->ptr;

    this->nes = nes;

    bus->destruct = &AXROM_destruct;
    bus->reset = &AXROM_reset;
    bus->writecart = &AXROM_writecart;
    bus->readcart = &AXROM_readcart;
    bus->setcart = &AXROM_setcart;
    bus->cpu_cycle = NULL;
    bus->a12_watch = NULL;
}
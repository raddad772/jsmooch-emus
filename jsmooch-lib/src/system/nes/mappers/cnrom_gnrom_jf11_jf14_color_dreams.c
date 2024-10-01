//
// Created by . on 9/30/24.
//

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include "helpers/debugger/debugger.h"

#include "../nes.h"

#include "mapper.h"
#include "cnrom_gnrom_jf11_jf14_color_dreams.h"

#define THISM struct GNROM *this = (struct GNROM *)bus->ptr

struct GNROM {
    struct NES *nes;
    enum NES_mappers kind;

    struct {
        u32 CHR_bank_num;
        u32 PRG_bank_num;
    } io;
};

#define READONLY 1
#define READWRITE 0

static void remap(struct NES_bus *bus)
{
    THISM;
    NES_bus_map_PRG32K(bus, 0x8000, 0xFFFF, &bus->PRG_ROM, this->io.PRG_bank_num, READONLY);
    NES_bus_map_CHR8K(bus, 0x0000, 0x1FFF, &bus->CHR_ROM, this->io.CHR_bank_num, READONLY);
}


static void GNROM_destruct(struct NES_bus *bus)
{

}

static void GNROM_reset(struct NES_bus *bus)
{
    printf("\nGNROM/JF11/JF14/Color Dreams Resetting, so remapping bus...");
    THISM;
    NES_bus_PPU_mirror_set(bus);
    this->io.CHR_bank_num = 0;
    this->io.PRG_bank_num = 0;
    remap(bus);
}

static void GNROM_writecart(struct NES_bus *bus, u32 addr, u32 val, u32 *do_write)
{
    *do_write = 1;
    THISM;
    u32 dirty_RAM = 0;
    switch(this->kind) {
        case NESM_JF11_JF14:
            if ((addr >= 0x6000) && (addr <= 0x7FFF)) {
                dirty_RAM = this->io.PRG_bank_num != ((val >> 4) & 3);
                this->io.PRG_bank_num = (val >> 4) & 3;
                this->io.CHR_bank_num = val & 15;
                remap(bus);
            }
            break;
        case NESM_GNROM:
            if (addr >= 0x8000) {
                dirty_RAM = this->io.PRG_bank_num != ((val >> 4) & 3);
                this->io.PRG_bank_num = (val >> 4) & 3;
                this->io.CHR_bank_num = val & 3;
                remap(bus);
            }
            break;
        case NESM_CNROM:
            if (addr >= 0x8000) {
                this->io.CHR_bank_num = val % bus->num_CHR_ROM_banks8K;
                remap(bus);
            }
            break;
        case NESM_COLOR_DREAMS:
            if (addr >= 0x8000) {
                dirty_RAM = this->io.PRG_bank_num != (val & 3);
                this->io.PRG_bank_num = val & 3;
                this->io.CHR_bank_num = (val >> 4) & 15;
                remap(bus);
            }
        default:
            assert(1==2);
    }
    if (dirty_RAM) {
        debugger_interface_dirty_mem(bus->nes->dbg.interface, NESMEM_CPUBUS, 0x8000, 0xFFFF);
    }
}

static u32 GNROM_readcart(struct NES_bus *bus, u32 addr, u32 old_val, u32 has_effect, u32 *do_read)
{
    *do_read = 1;
    return old_val;
}

static void GNROM_setcart(struct NES_bus *bus, struct NES_cart *cart)
{
    bus->ppu_mirror_mode = cart->header.mirroring ^ 1;
}

void GNROM_JF11_JF14_color_dreams_init(struct NES_bus *bus, struct NES *nes, enum NES_mappers kind)
{
    if (bus->ptr != NULL) free(bus->ptr);
    bus->ptr = malloc(sizeof(struct GNROM));
    struct GNROM *this = (struct GNROM*)bus->ptr;

    this->nes = nes;
    this->kind = kind;

    this->io.CHR_bank_num = 0;
    this->io.PRG_bank_num = 0;

    bus->destruct = &GNROM_destruct;
    bus->reset = &GNROM_reset;
    bus->writecart = &GNROM_writecart;
    bus->readcart = &GNROM_readcart;
    bus->setcart = &GNROM_setcart;
    bus->cpu_cycle = NULL;
    bus->a12_watch = NULL;
}
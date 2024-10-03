//
// Created by . on 9/27/24.
//

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include "helpers/debugger/debugger.h"
#include "../nes.h"

#include "mapper.h"
#include "mmc1_sxrom.h"

// TODO: emulate SXROM better
// according to this: https://www.nesdev.org/wiki/MMC1#Banks

#define THISM struct SXROM *this = (struct SXROM *)bus->ptr

struct SXROM {
    struct NES *nes;
    // Literally no state is needed for this actually...

    u64 last_cycle_write;
    struct {
        u32 shift_pos;
        u32 shift_value;
        u32 ppu_bank00;
        u32 ppu_bank10;
        u32 bank;
        u32 ctrl;
        u32 prg_bank_mode;
        u32 chr_bank_mode;
    } io;
};

#define READONLY 1
#define READWRITE 0

static void remap(struct NES_bus *bus, u32 boot, u32 may_contain_PRG)
{
    THISM;
    if (boot) {
        NES_bus_map_PRG8K(bus, 0x6000, 0x7FFF, &bus->PRG_RAM, 0, READWRITE);

        NES_bus_map_PRG32K(bus, 0x8000, 0xFFFF, &bus->PRG_ROM, 0, READONLY);
        if (bus->CHR_RAM.sz > 0) {
            NES_bus_map_CHR8K(bus, 0x0000, 0x1FFF, &bus->CHR_RAM, 0, READWRITE);
        }
        else {
            NES_bus_map_CHR8K(bus, 0x0000, 0x1FFF, &bus->CHR_ROM, 0, READONLY);
        }
    }

    switch(this->io.prg_bank_mode) {
        case 0:
        case 1: // 32k at 0x8000
            NES_bus_map_PRG16K(bus, 0x8000, 0xBFFF, &bus->PRG_ROM, this->io.bank & 0xFE, READONLY);
            NES_bus_map_PRG16K(bus, 0xC000, 0xFFFF, &bus->PRG_ROM, this->io.bank | 1, READONLY);
            break;
        case 2:
            NES_bus_map_PRG16K(bus, 0x8000, 0xBFFF, &bus->PRG_ROM, 0, READONLY);
            NES_bus_map_PRG16K(bus, 0xC000, 0xFFFF, &bus->PRG_ROM, this->io.bank, READONLY);
            break;
        case 3:
            NES_bus_map_PRG16K(bus, 0x8000, 0xBFFF, &bus->PRG_ROM, this->io.bank, READONLY);
            NES_bus_map_PRG16K(bus, 0xC000, 0xFFFF, &bus->PRG_ROM, bus->num_PRG_ROM_banks16K - 1, READONLY);
            break;
    }

    if (bus->CHR_RAM.sz == 0) {
        switch (this->io.chr_bank_mode) {
            case 0: // 8kb switch at a time
                NES_bus_map_CHR4K(bus, 0x0000, 0x0FFF, &bus->CHR_ROM, this->io.ppu_bank00 & 0xFE, READONLY);
                NES_bus_map_CHR4K(bus, 0x1000, 0x1FFF, &bus->CHR_ROM, this->io.ppu_bank00 | 1, READONLY);
                break;
            case 1: // 4kb switch at a time
                NES_bus_map_CHR4K(bus, 0x0000, 0x0FFF, &bus->CHR_ROM, this->io.ppu_bank00, READONLY);
                NES_bus_map_CHR4K(bus, 0x1000, 0x1FFF, &bus->CHR_ROM, this->io.ppu_bank10, READONLY);
                break;
        }
    }

}


static void SXROM_destruct(struct NES_bus *bus)
{

}

static void SXROM_reset(struct NES_bus *bus)
{
    printf("\nSXROM Resetting, so remapping bus...");
    THISM;
    this->io.prg_bank_mode = 3;
    remap(bus, 1, 1);
}

static void SXROM_writecart(struct NES_bus *bus, u32 addr, u32 val, u32 *do_write)
{
    *do_write = 1;
    THISM;
    if (addr < 0x8000) return;
    if (val & 0x80) {
        // Writes ctrl | 0x0C
        this->io.shift_pos = 4;
        this->io.shift_value = this->io.ctrl | 0x0C;
        this->last_cycle_write = bus->nes->clock.cpu_master_clock;
        addr = 0x8000;
    } else {
        if (bus->nes->clock.cpu_master_clock == (this->last_cycle_write + bus->nes->clock.timing.cpu_divisor)) {
            // writes on consecutive cycles fail
            this->last_cycle_write = bus->nes->clock.cpu_master_clock;
            printf("\nCONSECUTIVE WRITE ISSUE");
            return;
        } else {
            this->io.shift_value = (this->io.shift_value >> 1) | ((val & 1) << 4);
        }
    }

    this->last_cycle_write = bus->nes->clock.cpu_master_clock;

    this->io.shift_pos++;
    if (this->io.shift_pos == 5) {
        addr &= 0xE000;
        val = this->io.shift_value;
        switch (addr) {
            case 0x8000: // control register
                this->io.ctrl = this->io.shift_value;
                switch(val & 3) {
                    case 0: bus->ppu_mirror_mode = PPUM_ScreenAOnly; break;
                    case 1: bus->ppu_mirror_mode = PPUM_ScreenBOnly; break;
                    case 2: bus->ppu_mirror_mode = PPUM_Vertical; break;
                    case 3: bus->ppu_mirror_mode = PPUM_Horizontal; break;
                }
                NES_bus_PPU_mirror_set(bus);
                this->io.prg_bank_mode = (val >> 2) & 3;
                this->io.chr_bank_mode = (val >> 4) & 1;
                printf("\nline:%03d cycle:%d   (CTRL)  mirror_mode:%d  chr_bank_mode:%d", this->nes->clock.ppu_y, this->nes->ppu.line_cycle, val & 3, this->io.chr_bank_mode);
                remap(bus, 0, 1);
                break;
            case 0xA000: // CHR bank 0x0000
                this->io.ppu_bank00 = this->io.shift_value % bus->num_CHR_ROM_banks4K;
                remap(bus, 0, 0);
                break;
            case 0xC000: // CHR bank 1
                this->io.ppu_bank10 = this->io.shift_value % bus->num_CHR_ROM_banks4K;
                remap(bus, 0, 0);
                break;
            case 0xE000: // PRG bank
                this->io.bank = this->io.shift_value & 0x0F;
                if (this->io.shift_value & 0x10)
                    printf("WARNING50!");
                remap(bus, 0, 1);
                break;
        }
        this->io.shift_value = 0;
        this->io.shift_pos = 0;
    }
}

static u32 SXROM_readcart(struct NES_bus *bus, u32 addr, u32 old_val, u32 has_effect, u32 *do_read)
{
    *do_read = 1;
    return old_val;
}

static void SXROM_setcart(struct NES_bus *bus, struct NES_cart *cart)
{
    bus->ppu_mirror_mode = cart->header.mirroring;
}

void SXROM_init(struct NES_bus *bus, struct NES *nes)
{
    if (bus->ptr != NULL) free(bus->ptr);
    bus->ptr = malloc(sizeof(struct SXROM));
    struct SXROM *this = (struct SXROM*)bus->ptr;

    this->nes = nes;

    this->io.shift_pos = 0;
    this->io.shift_value = 0;
    this->io.ppu_bank00 = this->io.ppu_bank10 = 0;
    this->io.bank = this->io.ctrl = 0;
    this->io.prg_bank_mode = this->io.chr_bank_mode = 0;
    bus->ppu_mirror_mode = PPUM_Horizontal;
    NES_bus_PPU_mirror_set(bus);

    bus->destruct = &SXROM_destruct;
    bus->reset = &SXROM_reset;
    bus->writecart = &SXROM_writecart;
    bus->readcart = &SXROM_readcart;
    bus->setcart = &SXROM_setcart;
    bus->cpu_cycle = NULL;
    bus->a12_watch = NULL;
}
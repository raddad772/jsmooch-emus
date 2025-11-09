//
// Created by . on 9/27/24.
//

#include "helpers/debugger/debugger.h"
#include "../nes.h"

#include "mapper.h"
#include "mmc1_sxrom.h"

// TODO: emulate SXROM better
// according to this: https://www.nesdev.org/wiki/MMC1#Banks

#define THISM SXROM* th = static_cast<SXROM *>(bus->mapper_ptr);


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

static void remap(NES_mapper *bus, u32 boot, u32 may_contain_PRG)
{
    THISM;
    if (boot) {
        NES_bus_map_PRG8K(bus, 0x6000, 0x7FFF, &bus->fake_PRG_RAM, 0, READWRITE);

        NES_bus_map_PRG32K(bus, 0x8000, 0xFFFF, &bus->PRG_ROM, 0, READONLY);
        if (bus->CHR_RAM.sz > 0) {
            NES_bus_map_CHR8K(bus, 0x0000, 0x1FFF, &bus->CHR_RAM, 0, READWRITE);
        }
        else {
            NES_bus_map_CHR8K(bus, 0x0000, 0x1FFF, &bus->CHR_ROM, 0, READONLY);
        }
    }

    switch(th->io.prg_bank_mode) {
        case 0:
        case 1: // 32k at 0x8000
            NES_bus_map_PRG16K(bus, 0x8000, 0xBFFF, &bus->PRG_ROM, th->io.bank & 0xFE, READONLY);
            NES_bus_map_PRG16K(bus, 0xC000, 0xFFFF, &bus->PRG_ROM, th->io.bank | 1, READONLY);
            break;
        case 2:
            NES_bus_map_PRG16K(bus, 0x8000, 0xBFFF, &bus->PRG_ROM, 0, READONLY);
            NES_bus_map_PRG16K(bus, 0xC000, 0xFFFF, &bus->PRG_ROM, th->io.bank, READONLY);
            break;
        case 3:
            NES_bus_map_PRG16K(bus, 0x8000, 0xBFFF, &bus->PRG_ROM, th->io.bank, READONLY);
            NES_bus_map_PRG16K(bus, 0xC000, 0xFFFF, &bus->PRG_ROM, bus->num_PRG_ROM_banks16K - 1, READONLY);
            break;
        default: break;
    }

    if (bus->CHR_RAM.sz == 0) {
        switch (th->io.chr_bank_mode) {
            case 0: // 8kb switch at a time
                NES_bus_map_CHR4K(bus, 0x0000, 0x0FFF, &bus->CHR_ROM, th->io.ppu_bank00 & 0xFE, READONLY);
                NES_bus_map_CHR4K(bus, 0x1000, 0x1FFF, &bus->CHR_ROM, th->io.ppu_bank00 | 1, READONLY);
                break;
            case 1: // 4kb switch at a time
                NES_bus_map_CHR4K(bus, 0x0000, 0x0FFF, &bus->CHR_ROM, th->io.ppu_bank00, READONLY);
                NES_bus_map_CHR4K(bus, 0x1000, 0x1FFF, &bus->CHR_ROM, th->io.ppu_bank10, READONLY);
                break;
            default: break;
        }
    }

}

static void serialize(NES_mapper *bus, serialized_state &state)
{
    THISM;
#define S(x) Sadd(state, &th-> x, sizeof(th-> x))
    S(last_cycle_write);
    S(io);
#undef S
}

static void deserialize(NES_mapper *bus, serialized_state &state)
{
    THISM;
#define L(x) Sload(state, &th-> x, sizeof(th-> x))
    L(last_cycle_write);
    L(io);
#undef L
    remap(bus, 0, 1);
}


static void SXROM_destruct(NES_mapper *bus)
{

}

static void SXROM_reset(NES_mapper *bus)
{
    printf("\nSXROM Resetting, so remapping bus...");
    THISM;
    th->io.prg_bank_mode = 3;
    remap(bus, 1, 1);
}

static void SXROM_writecart(NES_mapper *bus, u32 addr, u32 val, u32 *do_write)
{
    *do_write = 1;
    THISM;
    if (addr < 0x8000) return;
    if (val & 0x80) {
        // Writes ctrl | 0x0C
        th->io.shift_pos = 4;
        th->io.shift_value = th->io.ctrl | 0x0C;
        th->last_cycle_write = bus->nes->clock.cpu_master_clock;
        addr = 0x8000;
    } else {
        if (bus->nes->clock.cpu_master_clock == (th->last_cycle_write + bus->nes->clock.timing.cpu_divisor)) {
            // writes on consecutive cycles fail
            th->last_cycle_write = bus->nes->clock.cpu_master_clock;
            printf("\nCONSECUTIVE WRITE ISSUE");
            return;
        } else {
            th->io.shift_value = (th->io.shift_value >> 1) | ((val & 1) << 4);
        }
    }

    th->last_cycle_write = bus->nes->clock.cpu_master_clock;

    th->io.shift_pos++;
    if (th->io.shift_pos == 5) {
        addr &= 0xE000;
        val = th->io.shift_value;
        switch (addr) {
            case 0x8000: // control register
                th->io.ctrl = th->io.shift_value;
                switch(val & 3) {
                    case 0: bus->ppu_mirror_mode = PPUM_ScreenAOnly; break;
                    case 1: bus->ppu_mirror_mode = PPUM_ScreenBOnly; break;
                    case 2: bus->ppu_mirror_mode = PPUM_Vertical; break;
                    case 3: bus->ppu_mirror_mode = PPUM_Horizontal; break;
                }
                NES_bus_PPU_mirror_set(bus);
                th->io.prg_bank_mode = (val >> 2) & 3;
                th->io.chr_bank_mode = (val >> 4) & 1;
                //printf("\nline:%03d cycle:%d   (CTRL)  mirror_mode:%d  chr_bank_mode:%d", th->nes->clock.ppu_y, th->nes->ppu.line_cycle, val & 3, th->io.chr_bank_mode);
                remap(bus, 0, 1);
                break;
            case 0xA000: // CHR bank 0x0000
                th->io.ppu_bank00 = th->io.shift_value % bus->num_CHR_ROM_banks4K;
                remap(bus, 0, 0);
                break;
            case 0xC000: // CHR bank 1
                th->io.ppu_bank10 = th->io.shift_value % bus->num_CHR_ROM_banks4K;
                remap(bus, 0, 0);
                break;
            case 0xE000: // PRG bank
                th->io.bank = th->io.shift_value & 0x0F;
                if (th->io.shift_value & 0x10)
                    printf("WARNING50!");
                remap(bus, 0, 1);
                break;
            default: break;
        }
        th->io.shift_value = 0;
        th->io.shift_pos = 0;
    }
}

static u32 SXROM_readcart(NES_mapper *bus, u32 addr, u32 old_val, u32 has_effect, u32 *do_read)
{
    *do_read = 1;
    return old_val;
}

static void SXROM_setcart(NES_mapper *bus, NES_cart *cart)
{
    bus->ppu_mirror_mode = cart->header.mirroring;
}

void SXROM_init(NES_mapper *bus, NES *nes)
{
    if (bus->ptr != nullptr) free(bus->ptr);
    bus->ptr = malloc(sizeof(SXROM));
    THISM;

    th->nes = nes;

    th->io.shift_pos = 0;
    th->io.shift_value = 0;
    th->io.ppu_bank00 = th->io.ppu_bank10 = 0;
    th->io.bank = th->io.ctrl = 0;
    th->io.prg_bank_mode = th->io.chr_bank_mode = 0;
    bus->ppu_mirror_mode = PPUM_Horizontal;
    NES_bus_PPU_mirror_set(bus);

    bus->destruct = &SXROM_destruct;
    bus->reset = &SXROM_reset;
    bus->writecart = &SXROM_writecart;
    bus->readcart = &SXROM_readcart;
    bus->setcart = &SXROM_setcart;
    bus->cpu_cycle = nullptr;
    bus->a12_watch = nullptr;
    bus->serialize = &serialize;
    bus->deserialize = &deserialize;
}
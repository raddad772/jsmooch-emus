//
// Created by . on 9/27/24.
//

#include "helpers/debugger/debugger.h"
#include "../nes.h"

#include "mapper.h"
#include "mmc1_sxrom.h"

// TODO: emulate SXROM better
// according to this: https://www.nesdev.org/wiki/MMC1#Banks

#define READONLY 1
#define READWRITE 0

void SXROM::remap(u32 boot)
{
    if (boot) {
        bus->map_PRG8K( 0x6000, 0x7FFF, &bus->fake_PRG_RAM, 0, READWRITE);

        bus->map_PRG32K( 0x8000, 0xFFFF, &bus->PRG_ROM, 0, READONLY);
        if (bus->CHR_RAM.sz > 0) {
            bus->map_CHR8K(0x0000, 0x1FFF, &bus->CHR_RAM, 0, READWRITE);
        }
        else {
            bus->map_CHR8K(0x0000, 0x1FFF, &bus->CHR_ROM, 0, READONLY);
        }
    }

    switch(io.prg_bank_mode) {
        case 0:
        case 1: // 32k at 0x8000
            bus->map_PRG16K( 0x8000, 0xBFFF, &bus->PRG_ROM, io.bank & 0xFE, READONLY);
            bus->map_PRG16K( 0xC000, 0xFFFF, &bus->PRG_ROM, io.bank | 1, READONLY);
            break;
        case 2:
            bus->map_PRG16K( 0x8000, 0xBFFF, &bus->PRG_ROM, 0, READONLY);
            bus->map_PRG16K( 0xC000, 0xFFFF, &bus->PRG_ROM, io.bank, READONLY);
            break;
        case 3:
            bus->map_PRG16K( 0x8000, 0xBFFF, &bus->PRG_ROM, io.bank, READONLY);
            bus->map_PRG16K( 0xC000, 0xFFFF, &bus->PRG_ROM, bus->num_PRG_ROM_banks16K - 1, READONLY);
            break;
        default: break;
    }

    if (bus->CHR_RAM.sz == 0) {
        switch (io.chr_bank_mode) {
            case 0: // 8kb switch at a time
                bus->map_CHR4K(0x0000, 0x0FFF, &bus->CHR_ROM, io.ppu_bank00 & 0xFE, READONLY);
                bus->map_CHR4K(0x1000, 0x1FFF, &bus->CHR_ROM, io.ppu_bank00 | 1, READONLY);
                break;
            case 1: // 4kb switch at a time
                bus->map_CHR4K(0x0000, 0x0FFF, &bus->CHR_ROM, io.ppu_bank00, READONLY);
                bus->map_CHR4K(0x1000, 0x1FFF, &bus->CHR_ROM, io.ppu_bank10, READONLY);
                break;
            default: break;
        }
    }

}

void SXROM::serialize(serialized_state &state)
{
#define S(x) Sadd(state, & x, sizeof( x))
    S(last_cycle_write);
    S(io);
#undef S
}

void SXROM::deserialize(serialized_state &state)
{
#define L(x) Sload(state, & x, sizeof( x))
    L(last_cycle_write);
    L(io);
#undef L
    remap(0);
}

void SXROM::reset()
{
    printf("\nSXROM Resetting, so remapping bus...");
    io.prg_bank_mode = 3;
    remap(1);
}

void SXROM::writecart(u32 addr, u32 val, u32 &do_write)
{
    do_write = 1;
    if (addr < 0x8000) return;
    if (val & 0x80) {
        // Writes ctrl | 0x0C
        io.shift_pos = 4;
        io.shift_value = io.ctrl | 0x0C;
        last_cycle_write = bus->nes->clock.cpu_master_clock;
        addr = 0x8000;
    } else {
        if (bus->nes->clock.cpu_master_clock == (last_cycle_write + bus->nes->clock.timing.cpu_divisor)) {
            // writes on consecutive cycles fail
            last_cycle_write = bus->nes->clock.cpu_master_clock;
            printf("\nCONSECUTIVE WRITE ISSUE");
            return;
        } else {
            io.shift_value = (io.shift_value >> 1) | ((val & 1) << 4);
        }
    }

    last_cycle_write = bus->nes->clock.cpu_master_clock;

    io.shift_pos++;
    if (io.shift_pos == 5) {
        addr &= 0xE000;
        val = io.shift_value;
        switch (addr) {
            case 0x8000: // control register
                io.ctrl = io.shift_value;
                switch(val & 3) {
                    case 0: bus->ppu_mirror_mode = PPUM_ScreenAOnly; break;
                    case 1: bus->ppu_mirror_mode = PPUM_ScreenBOnly; break;
                    case 2: bus->ppu_mirror_mode = PPUM_Vertical; break;
                    case 3: bus->ppu_mirror_mode = PPUM_Horizontal; break;
                }
                bus->PPU_mirror_set();
                io.prg_bank_mode = (val >> 2) & 3;
                io.chr_bank_mode = (val >> 4) & 1;
                //printf("\nline:%03d cycle:%d   (CTRL)  mirror_mode:%d  chr_bank_mode:%d", nes->clock.ppu_y, nes->ppu.line_cycle, val & 3, io.chr_bank_mode);
                remap(0);
                break;
            case 0xA000: // CHR bank 0x0000
                io.ppu_bank00 = io.shift_value % bus->num_CHR_ROM_banks4K;
                remap(0);
                break;
            case 0xC000: // CHR bank 1
                io.ppu_bank10 = io.shift_value % bus->num_CHR_ROM_banks4K;
                remap(0);
                break;
            case 0xE000: // PRG bank
                io.bank = io.shift_value & 0x0F;
                if (io.shift_value & 0x10)
                    printf("WARNING50!");
                remap(0);
                break;
            default: break;
        }
        io.shift_value = 0;
        io.shift_pos = 0;
    }
}

u32 SXROM::readcart(u32 addr, u32 old_val, u32 has_effect, u32 &do_read)
{
    do_read = 1;
    return old_val;
}

void SXROM::setcart(NES_cart &cart)
{
    bus->ppu_mirror_mode = cart.header.mirroring;
}


SXROM::SXROM(NES_bus *bus) : NES_mapper(bus)
{
    this->overrides_PPU = false;
    bus->ppu_mirror_mode = PPUM_Horizontal;
    bus->PPU_mirror_set();
}
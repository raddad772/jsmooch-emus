//
// Created by . on 10/3/24.
//

#include <cstdlib>
#include <cassert>

#include "helpers/debugger/debugger.h"
#include "system/nes/nes.h"
#include "nes_memmap.h"

#include "mapper.h"
#include "mmc5.h"

#define READONLY 1
#define READWRITE 0

void MMC5::map_CHR1K(u32 sprites_status, u32 range_start, u32 range_end, simplebuf8 *buf, u32 bank, u32 is_readonly)
{
    NES_memmap_map(PPU_map[sprites_status], 10, range_start, range_end, buf, bank << 10, is_readonly, nullptr, 1, bus->SRAM);
}

void MMC5::map_CHR2K(u32 sprites_status, u32 range_start, u32 range_end, simplebuf8 *buf, u32 bank, u32 is_readonly)
{
    NES_memmap_map(PPU_map[sprites_status], 10, range_start, range_end, buf, bank << 11, is_readonly, nullptr, 1, bus->SRAM);
}

void MMC5::map_CHR4K(u32 sprites_status, u32 range_start, u32 range_end, simplebuf8 *buf, u32 bank, u32 is_readonly)
{
    NES_memmap_map(PPU_map[sprites_status], 10, range_start, range_end, buf, bank << 12, is_readonly, nullptr, 1, bus->SRAM);
}

void MMC5::map_CHR8K(u32 sprites_status, u32 range_start, u32 range_end, simplebuf8 *buf, u32 bank, u32 is_readonly)
{
    NES_memmap_map(PPU_map[sprites_status], 10, range_start, range_end, buf, bank << 13, is_readonly, nullptr, 1, bus->SRAM);
}

void MMC5::remap_CHR()
{
    switch(io.CHR_page_size) {
        case 0: // 8KB
            map_CHR8K(0, 0x0000, 0x1FFF, &bus->CHR_ROM, io.CHR_bank[7], READONLY);

            map_CHR8K(1, 0x0000, 0x1FFF, &bus->CHR_ROM, io.CHR_bank[11], READONLY);
            break;
        case 1: // 4KB
            map_CHR4K(0, 0x0000, 0x0FFF, &bus->CHR_ROM, io.CHR_bank[3], READONLY);
            map_CHR4K(0, 0x1000, 0x1FFF, &bus->CHR_ROM, io.CHR_bank[7], READONLY);

            map_CHR4K(1, 0x0000, 0x0FFF, &bus->CHR_ROM, io.CHR_bank[11], READONLY);
            map_CHR4K(1, 0x1000, 0x1FFF, &bus->CHR_ROM, io.CHR_bank[11], READONLY);
            break;
        case 2: // 2KB
            map_CHR2K(0, 0x0000, 0x07FF, &bus->CHR_ROM, io.CHR_bank[1], READONLY);
            map_CHR2K(0, 0x0800, 0x0FFF, &bus->CHR_ROM, io.CHR_bank[3], READONLY);
            map_CHR2K(0, 0x1000, 0x17FF, &bus->CHR_ROM, io.CHR_bank[5], READONLY);
            map_CHR2K(0, 0x1800, 0x1FFF, &bus->CHR_ROM, io.CHR_bank[7], READONLY);

            map_CHR2K(1, 0x0000, 0x07FF, &bus->CHR_ROM, io.CHR_bank[9], READONLY);
            map_CHR2K(1, 0x0800, 0x0FFF, &bus->CHR_ROM, io.CHR_bank[11], READONLY);
            map_CHR2K(1, 0x1000, 0x17FF, &bus->CHR_ROM, io.CHR_bank[9], READONLY);
            map_CHR2K(1, 0x1800, 0x1FFF, &bus->CHR_ROM, io.CHR_bank[11], READONLY);
            break;
        case 3: // 1KB
            map_CHR1K(0, 0x0000, 0x03FF, &bus->CHR_ROM, io.CHR_bank[0], READONLY);
            map_CHR1K(0, 0x0400, 0x07FF, &bus->CHR_ROM, io.CHR_bank[1], READONLY);
            map_CHR1K(0, 0x0800, 0x0BFF, &bus->CHR_ROM, io.CHR_bank[2], READONLY);
            map_CHR1K(0, 0x0C00, 0x0FFF, &bus->CHR_ROM, io.CHR_bank[3], READONLY);
            map_CHR1K(0, 0x1000, 0x13FF, &bus->CHR_ROM, io.CHR_bank[4], READONLY);
            map_CHR1K(0, 0x1400, 0x17FF, &bus->CHR_ROM, io.CHR_bank[5], READONLY);
            map_CHR1K(0, 0x1800, 0x1BFF, &bus->CHR_ROM, io.CHR_bank[6], READONLY);
            map_CHR1K(0, 0x1C00, 0x1FFF, &bus->CHR_ROM, io.CHR_bank[7], READONLY);

            map_CHR1K(1, 0x0000, 0x03FF, &bus->CHR_ROM, io.CHR_bank[8], READONLY);
            map_CHR1K(1, 0x1000, 0x13FF, &bus->CHR_ROM, io.CHR_bank[8], READONLY);

            map_CHR1K(1, 0x0400, 0x07FF, &bus->CHR_ROM, io.CHR_bank[9], READONLY);
            map_CHR1K(1, 0x1400, 0x17FF, &bus->CHR_ROM, io.CHR_bank[9], READONLY);

            map_CHR1K(1, 0x0800, 0x0BFF, &bus->CHR_ROM, io.CHR_bank[10], READONLY);
            map_CHR1K(1, 0x1800, 0x1BFF, &bus->CHR_ROM, io.CHR_bank[10], READONLY);

            map_CHR1K(1, 0x0C00, 0x0FFF, &bus->CHR_ROM, io.CHR_bank[11], READONLY);
            map_CHR1K(1, 0x1C00, 0x1FFF, &bus->CHR_ROM, io.CHR_bank[11], READONLY);
            break;
        default:
            assert(1==2);
    }
}

void MMC5::map_PRG(u32 start_addr, u32 end_addr, u32 val)
{
    simplebuf8 *bptr = val & 0x80 ? &bus->PRG_ROM : &bus->fake_PRG_RAM;
    u32 readwrite = val & 0x80 ? READONLY : READWRITE;

    bus->map_PRG8K(start_addr, end_addr, bptr, val & 0x7F, readwrite);
}

void MMC5::remap_PRG()
{
    switch(io.PRG_bank_mode) {
        case 0: {
            // 32KB
            map_PRG(0x6000, 0x7FFF, io.PRG_bank[0] & 0x7C);
            u32 b = (io.PRG_bank[4] & 0x7C) | 0x80; // ROM-only in mode 0
            map_PRG(0x8000, 0x9FFF, b);
            map_PRG(0xA000, 0xBFFF, b | 1);
            map_PRG(0xC000, 0xDFFF, b | 2);
            map_PRG(0xE000, 0xFFFF, b | 3);
            break; }
        case 1: // 16KB
            map_PRG(0x6000, 0x7FFF, io.PRG_bank[0] & 0x7E);

            // first two 5115 (2)
            // next two 5117 (4)
            map_PRG(0x8000, 0x9FFF, io.PRG_bank[2] & 0xFE);
            map_PRG(0xA000, 0xBFFF, io.PRG_bank[2] | 1);
            map_PRG(0xC000, 0xDFFF, io.PRG_bank[4] & 0xFE);
            map_PRG(0xE000, 0xFFFF, io.PRG_bank[4] | 1);
            break;
        case 2: // semi-16kb
            map_PRG(0x6000, 0x7FFF, io.PRG_bank[0] & 0x7E);

            map_PRG(0x8000, 0x9FFF, io.PRG_bank[2] & 0xFE);
            map_PRG(0xA000, 0xBFFF, io.PRG_bank[2] | 1);
            map_PRG(0xC000, 0xDFFF, io.PRG_bank[3]);
            map_PRG(0xE000, 0xFFFF, io.PRG_bank[4]);

            break;

        case 3: // 8kb all around!
            map_PRG(0x6000, 0x7FFF, io.PRG_bank[0] & 0x7E);

            map_PRG(0x8000, 0x9FFF, io.PRG_bank[1]);
            map_PRG(0xA000, 0xBFFF, io.PRG_bank[2]);
            map_PRG(0xC000, 0xDFFF, io.PRG_bank[3]);
            map_PRG(0xE000, 0xFFFF, io.PRG_bank[4]);
            break;
        default:
            assert(1==2);
    }
}

void remap_exram()
{

}

void MMC5::remap(bool boot)
{
    if (boot) {
        //assert(1 == 2);
    }

    remap_CHR();
    remap_PRG();
    remap_exram();
}

void MMC5::serialize(serialized_state &state)
{
#define S(x) Sadd(state, & x, sizeof(x))
#undef S
    printf("\nWARN MMC5 SERIALIZE NOT IMPLEMENT");
}

void MMC5::deserialize(serialized_state &state)
{
#define L(x) Sload(state, & x, sizeof(x))
#undef L
    printf("\nWARN MMC5 DESERIALIZE NOT IMPLEMENT");
}

void MMC5::reset()
{
    printf("\nMMC5 Resetting, so remapping bus->..");
    io.PRG_bank_mode = 3;
    io.upper_CHR_bits = 0;
    io.PRG_bank[4] = 0xFF;
    io.multiplier = io.multplicand = 0xFF;
    remap(true);
}


void MMC5::writecart(u32 addr, u32 val, u32 &do_write)
{
    do_write = 1;
    // EXRAM
    if ((addr >= 0x5C00) && (addr < 0x6000)) {
        switch(io.exram_mode) {
            case 0:
            case 1: // only during blanking
                if (blanking) {
                    exram.ptr[addr - 0x5C00] = val;
                    do_write = 0;
                }
                return;
            case 2: // any time
                exram.ptr[addr - 0x5C00] = val;
                do_write = 0;
                return;
            case 3: // read-only
                return;
            default: break;
        }
    }
    switch(addr) {
        case 0x2000:
            io.ppu.sprites_8x16 = (val >> 5) & 1;
            return;
        case 0x2001:
            io.ppu.substitutions_enabled = ((val >> 3) & 3) != 0;
            return;
        case 0x5100: // PRG bank mode
            io.PRG_bank_mode = val & 3;
            remap_PRG();
            break;
        case 0x5101: // CHR page size
            io.CHR_page_size = val & 3;
            remap_CHR();
            return;
        case 0x5102: // PRG RAM protect 1
            io.PRG_RAM_protect[0] = (val & 3) == 2;
            io.PRG_RAM_protect_mux = io.PRG_RAM_protect[0] && io.PRG_RAM_protect[1];
            remap_PRG();
            return;
        case 0x5103:
            io.PRG_RAM_protect[1] = (val & 3) == 1;
            io.PRG_RAM_protect_mux = io.PRG_RAM_protect[0] && io.PRG_RAM_protect[1];
            remap_PRG();
            return;
        case 0x5104: // Internal extended RAM mode
            io.exram_mode = val & 3;
            remap_exram();
            return;
        case 0x5105: // PPU mirroring mode
            nametables[0] = val & 3;
            nametables[1] = (val >> 2) & 3;
            nametables[2] = (val >> 4) & 3;
            nametables[3] = (val >> 6) & 3;
            return;
        case 0x5106:
            io.fill_tile = val;
            return;
        case 0x5107: {
            u32 v = val & 3;
            io.fill_color = v | (v << 2) | (v << 4) | (v << 6);
            return; }
        case 0x5113: case 0x5114: case 0x5115: case 0x5116: case 0x5117: // PRG RAM bank
            io.PRG_bank[addr - 0x5113] = val;
            remap_PRG();
            return;
        case 0x5120: case 0x5121: case 0x5122: case 0x5123: case 0x5124: case 0x5125: // CHR RAM bank
        case 0x5126: case 0x5127: case 0x5128: case 0x5129: case 0x512A: case 0x512B:
            io.CHR_bank[addr - 0x5120] = io.upper_CHR_bits | val;
            remap_CHR();
            return;
        case 0x5130:
            io.upper_CHR_bits = (val & 3) << 8;
            return;
        case 0x5203:
            io.irq_scanline = val;
            return;
        case 0x5204:
            io.irq_enabled = (val >> 7) & 1;
            return;
        case 0x5205: //
            io.multplicand = val;
            return;
        case 0x5206:
            io.multiplier = val;
            return;
        default:
            return;
    }
}

u32 MMC5::readcart(u32 addr, u32 old_val, u32 has_effect, u32 &do_read)
{
    do_read = 1;
    if ((addr >= 0x5C00) && (addr < 0x6000)) {
        do_read = 0;
        switch(io.exram_mode) {
            case 0:
            case 1: // only during blanking
                return old_val;
            case 2: // any time
            case 3:
                return exram.ptr[addr - 0x5C00];
            default:
        }
    }
    switch(addr) {
        case 0x5204:
            do_read = 0;
            return (irq.in_frame << 7) | (irq.pending << 6);
        case 0x5205:
            do_read = 0;
            return (io.multiplier * io.multplicand) & 0xFF;
        case 0x5206:
            do_read = 0;
            return ((io.multiplier * io.multplicand) >> 8) & 0xFF;
        default:
    }
    return old_val;
}

void MMC5::setcart(NES_cart &cart)
{
    bus->ppu_mirror_mode = cart.header.mirroring;
    //1simplebuf8_allocate(&bus->PRG_RAM, 64 * 1024); // Re-allocate to 64KB RAM
    // TODO: was this important?
}

u32 MMC5::PPU_read_override(u32 addr, u32 has_effect)
{
    addr &= 0x3FFF;
    if (addr < 0x2000) // Do
        return PPU_map[rendering_sprites][addr >> 10].read(addr, 0);
    addr = addr & 0xFFF;
    u32 nt_base = addr & 0xC00;
    u32 nt = nt_base >> 10; // 3
    switch(nametables[nt]) {
        case 0: // CIRAM page 0
            return bus->CIRAM.ptr[addr & 0x3FF];
        case 1:
            return bus->CIRAM.ptr[(addr & 0x3FF) + 0x400];
        case 2:
            return exram.ptr[addr & 0x3FF];
        case 3:
            addr &= 0x3FF;
            if (io.exram_mode != 1) {
                // All attribute fetches get fill mode
                if (addr >= 0x3C0) return io.fill_tile;
            }
            if (addr < 0x3C0) {
                return exram.ptr[addr];
            }
            else {
                if (io.exram_mode == 0) {
                    return exram.ptr[addr];
                }
                return 0xFF; // ??? TODO: fix this
            }
        default:
    }
    return 0xFF;
}

void MMC5::PPU_write_override(u32 addr, u32 val)
{
    addr &= 0x3FFF;
    if (addr < 0x2000) // Do
        return PPU_map[rendering_sprites][addr >> 10].write(addr, val);
    addr = addr & 0xFFF;
    u32 nt_base = addr & 0xC00;
    u32 nt = nt_base >> 10; // 3
    switch(nametables[nt]) {
        case 0: // CIRAM page 0
            bus->CIRAM.ptr[addr & 0x3FF] = val;
            return;
        case 1:
            bus->CIRAM.ptr[(addr & 0x3FF) + 0x400] = val;
            return;
        case 2:
            exram.ptr[addr & 0x3FF] = val;
            return;
        case 3:
            return;
        default:
    }
}


void MMC5::a12_watch(u32 addr) {
    if ((addr >= 0x2000) && (addr <= 0x2FFF) && (addr == irq.addr))
        irq.reads++;
    if (irq.reads == 2) {
        if (irq.in_frame == 0) {
            irq.in_frame = 1;
            irq.scanline = 0;
        } else {
            irq.scanline++;
            //printf("\nNEW SCANLINE! PPUY: %d MYY:%d", bus->nes->clock.ppu_y, irq.scanline); //bus->ppu.line_cycle)
            if (irq.scanline == io.irq_scanline) {
                irq.pending = 1;
            }
        }
    }
    else
        irq.reads = 0;
    irq.addr = addr;
    //ppuIsReading := true
}

MMC5::MMC5(NES_bus *bus) : NES_mapper(bus)
{
    this->overrides_PPU = true;
    exram.allocate(1024);
    NES_memmap_init_empty(PPU_map[0], 0x0000, 0x3FFF, 10);
    NES_memmap_init_empty(PPU_map[1], 0x0000, 0x3FFF, 10);
}

//
// Created by . on 10/3/24.
//

#include <cstdlib>
#include <cassert>

#include "helpers/debugger/debugger.h"
#include "system/nes/nes.h"

#include "mapper.h"
#include "mmc5.h"

#define THISM MMC5 *th = static_cast<MMC5 *>(bus->ptr)

struct MMC5 {
    NES *nes;
    simplebuf8 exram;

    struct {
        struct {
            u32 sprites_8x16;
            u32 substitutions_enabled;
        } ppu;

        u32 PRG_bank_mode, CHR_page_size;

        u32 PRG_RAM_protect[2], PRG_RAM_protect_mux;

        u32 exram_mode;

        u32 upper_CHR_bits;

        u32 CHR_bank[12];
        u32 PRG_bank[5];

        u32 multplicand, multiplier;

        u32 fill_tile, fill_color;

        u32 irq_scanline, irq_enabled;
    } io;
    u32 rendering_sprites;
    u32 blanking;

    u32 nametables[4];
    struct {
        u32 reads;
        u32 addr;
        u32 in_frame, scanline;
        u32 pending;
    } irq;
    struct NES_memmap PPU_map[2][0x4000 / 0x400];
};

#define READONLY 1
#define READWRITE 0

static void map_CHR1K(MMC5 *mapper, u32 sprites_status, u32 range_start, u32 range_end, simplebuf8 *buf, u32 bank, u32 is_readonly)
{
    NES_memmap_map(mapper->PPU_map[sprites_status], 10, range_start, range_end, buf, bank << 10, is_readonly, nullptr, 1, mapper->nes->bus.SRAM);
}

static void map_CHR2K(MMC5 *mapper, u32 sprites_status, u32 range_start, u32 range_end, simplebuf8 *buf, u32 bank, u32 is_readonly)
{
    NES_memmap_map(mapper->PPU_map[sprites_status], 10, range_start, range_end, buf, bank << 11, is_readonly, nullptr, 1, mapper->nes->bus.SRAM);
}

static void map_CHR4K(MMC5 *mapper, u32 sprites_status, u32 range_start, u32 range_end, simplebuf8 *buf, u32 bank, u32 is_readonly)
{
    NES_memmap_map(mapper->PPU_map[sprites_status], 10, range_start, range_end, buf, bank << 12, is_readonly, nullptr, 1, mapper->nes->bus.SRAM);
}

static void map_CHR8K(MMC5 *mapper, u32 sprites_status, u32 range_start, u32 range_end, simplebuf8 *buf, u32 bank, u32 is_readonly)
{
    NES_memmap_map(mapper->PPU_map[sprites_status], 10, range_start, range_end, buf, bank << 13, is_readonly, nullptr, 1, mapper->nes->bus.SRAM);
}

static void remap_CHR(NES_mapper *bus)
{
    THISM;
    switch(th->io.CHR_page_size) {
        case 0: // 8KB
            map_CHR8K(th, 0, 0x0000, 0x1FFF, &bus->CHR_ROM, th->io.CHR_bank[7], READONLY);

            map_CHR8K(th, 1, 0x0000, 0x1FFF, &bus->CHR_ROM, th->io.CHR_bank[11], READONLY);
            break;
        case 1: // 4KB
            map_CHR4K(th, 0, 0x0000, 0x0FFF, &bus->CHR_ROM, th->io.CHR_bank[3], READONLY);
            map_CHR4K(th, 0, 0x1000, 0x1FFF, &bus->CHR_ROM, th->io.CHR_bank[7], READONLY);

            map_CHR4K(th, 1, 0x0000, 0x0FFF, &bus->CHR_ROM, th->io.CHR_bank[11], READONLY);
            map_CHR4K(th, 1, 0x1000, 0x1FFF, &bus->CHR_ROM, th->io.CHR_bank[11], READONLY);
            break;
        case 2: // 2KB
            map_CHR2K(th, 0, 0x0000, 0x07FF, &bus->CHR_ROM, th->io.CHR_bank[1], READONLY);
            map_CHR2K(th, 0, 0x0800, 0x0FFF, &bus->CHR_ROM, th->io.CHR_bank[3], READONLY);
            map_CHR2K(th, 0, 0x1000, 0x17FF, &bus->CHR_ROM, th->io.CHR_bank[5], READONLY);
            map_CHR2K(th, 0, 0x1800, 0x1FFF, &bus->CHR_ROM, th->io.CHR_bank[7], READONLY);

            map_CHR2K(th, 1, 0x0000, 0x07FF, &bus->CHR_ROM, th->io.CHR_bank[9], READONLY);
            map_CHR2K(th, 1, 0x0800, 0x0FFF, &bus->CHR_ROM, th->io.CHR_bank[11], READONLY);
            map_CHR2K(th, 1, 0x1000, 0x17FF, &bus->CHR_ROM, th->io.CHR_bank[9], READONLY);
            map_CHR2K(th, 1, 0x1800, 0x1FFF, &bus->CHR_ROM, th->io.CHR_bank[11], READONLY);
            break;
        case 3: // 1KB
            map_CHR1K(th, 0, 0x0000, 0x03FF, &bus->CHR_ROM, th->io.CHR_bank[0], READONLY);
            map_CHR1K(th, 0, 0x0400, 0x07FF, &bus->CHR_ROM, th->io.CHR_bank[1], READONLY);
            map_CHR1K(th, 0, 0x0800, 0x0BFF, &bus->CHR_ROM, th->io.CHR_bank[2], READONLY);
            map_CHR1K(th, 0, 0x0C00, 0x0FFF, &bus->CHR_ROM, th->io.CHR_bank[3], READONLY);
            map_CHR1K(th, 0, 0x1000, 0x13FF, &bus->CHR_ROM, th->io.CHR_bank[4], READONLY);
            map_CHR1K(th, 0, 0x1400, 0x17FF, &bus->CHR_ROM, th->io.CHR_bank[5], READONLY);
            map_CHR1K(th, 0, 0x1800, 0x1BFF, &bus->CHR_ROM, th->io.CHR_bank[6], READONLY);
            map_CHR1K(th, 0, 0x1C00, 0x1FFF, &bus->CHR_ROM, th->io.CHR_bank[7], READONLY);

            map_CHR1K(th, 1, 0x0000, 0x03FF, &bus->CHR_ROM, th->io.CHR_bank[8], READONLY);
            map_CHR1K(th, 1, 0x1000, 0x13FF, &bus->CHR_ROM, th->io.CHR_bank[8], READONLY);

            map_CHR1K(th, 1, 0x0400, 0x07FF, &bus->CHR_ROM, th->io.CHR_bank[9], READONLY);
            map_CHR1K(th, 1, 0x1400, 0x17FF, &bus->CHR_ROM, th->io.CHR_bank[9], READONLY);

            map_CHR1K(th, 1, 0x0800, 0x0BFF, &bus->CHR_ROM, th->io.CHR_bank[10], READONLY);
            map_CHR1K(th, 1, 0x1800, 0x1BFF, &bus->CHR_ROM, th->io.CHR_bank[10], READONLY);

            map_CHR1K(th, 1, 0x0C00, 0x0FFF, &bus->CHR_ROM, th->io.CHR_bank[11], READONLY);
            map_CHR1K(th, 1, 0x1C00, 0x1FFF, &bus->CHR_ROM, th->io.CHR_bank[11], READONLY);
            break;
        default:
            assert(1==2);
    }
}

static void map_PRG(NES_mapper *bus, u32 start_addr, u32 end_addr, u32 val)
{
    THISM;
    struct simplebuf8 *bptr = val & 0x80 ? &bus->PRG_ROM : &bus->fake_PRG_RAM;
    u32 readwrite = val & 0x80 ? READONLY : READWRITE;

    NES_bus_map_PRG8K(bus, start_addr, end_addr, bptr, val & 0x7F, readwrite);
}

static void remap_PRG(NES_mapper *bus)
{
    THISM;
    switch(th->io.PRG_bank_mode) {
        case 0: {
            // 32KB
            map_PRG(bus, 0x6000, 0x7FFF, th->io.PRG_bank[0] & 0x7C);
            u32 b = (th->io.PRG_bank[4] & 0x7C) | 0x80; // ROM-only in mode 0
            map_PRG(bus, 0x8000, 0x9FFF, b);
            map_PRG(bus, 0xA000, 0xBFFF, b | 1);
            map_PRG(bus, 0xC000, 0xDFFF, b | 2);
            map_PRG(bus, 0xE000, 0xFFFF, b | 3);
            break; }
        case 1: // 16KB
            map_PRG(bus, 0x6000, 0x7FFF, th->io.PRG_bank[0] & 0x7E);

            // first two 5115 (2)
            // next two 5117 (4)
            map_PRG(bus, 0x8000, 0x9FFF, th->io.PRG_bank[2] & 0xFE);
            map_PRG(bus, 0xA000, 0xBFFF, th->io.PRG_bank[2] | 1);
            map_PRG(bus, 0xC000, 0xDFFF, th->io.PRG_bank[4] & 0xFE);
            map_PRG(bus, 0xE000, 0xFFFF, th->io.PRG_bank[4] | 1);
            break;
        case 2: // semi-16kb
            map_PRG(bus, 0x6000, 0x7FFF, th->io.PRG_bank[0] & 0x7E);

            map_PRG(bus, 0x8000, 0x9FFF, th->io.PRG_bank[2] & 0xFE);
            map_PRG(bus, 0xA000, 0xBFFF, th->io.PRG_bank[2] | 1);
            map_PRG(bus, 0xC000, 0xDFFF, th->io.PRG_bank[3]);
            map_PRG(bus, 0xE000, 0xFFFF, th->io.PRG_bank[4]);

            break;

        case 3: // 8kb all around!
            map_PRG(bus, 0x6000, 0x7FFF, th->io.PRG_bank[0] & 0x7E);

            map_PRG(bus, 0x8000, 0x9FFF, th->io.PRG_bank[1]);
            map_PRG(bus, 0xA000, 0xBFFF, th->io.PRG_bank[2]);
            map_PRG(bus, 0xC000, 0xDFFF, th->io.PRG_bank[3]);
            map_PRG(bus, 0xE000, 0xFFFF, th->io.PRG_bank[4]);
            break;
        default:
            assert(1==2);
    }
}

static void remap_exram(NES_mapper *bus)
{

}

static void remap(NES_mapper *bus, u32 boot)
{
    THISM;
    if (boot) {
        //assert(1 == 2);
    }

    remap_CHR(bus);
    remap_PRG(bus);
    remap_exram(bus);
}

static void serialize(NES_mapper *bus, serialized_state &state)
{
    THISM;
#define S(x) Sadd(state, &th-> x, sizeof(th-> x))
#undef S
}

static void deserialize(NES_mapper *bus, serialized_state &state)
{
    THISM;
#define L(x) Sload(state, &th-> x, sizeof(th-> x))
#undef L
}

static void MMC5_destruct(NES_mapper *bus)
{
    THISM;
    NES_memmap_init_empty(th->PPU_map[0], 0x0000, 0x3FFF, 10);
    NES_memmap_init_empty(th->PPU_map[1], 0x0000, 0x3FFF, 10);
    th->exram.~simplebuf8();
}

static void MMC5_reset(NES_mapper *bus)
{
    printf("\nMMC5 Resetting, so remapping bus...");
    THISM;
    th->io.PRG_bank_mode = 3;
    th->io.upper_CHR_bits = 0;
    th->io.PRG_bank[4] = 0xFF;
    th->io.multiplier = th->io.multplicand = 0xFF;
    remap(bus, 1);
}


static void MMC5_writecart(NES_mapper *bus, u32 addr, u32 val, u32 *do_write)
{
    *do_write = 1;
    THISM;
    // EXRAM
    if ((addr >= 0x5C00) && (addr < 0x6000)) {
        switch(th->io.exram_mode) {
            case 0:
            case 1: // only during blanking
                if (th->blanking) {
                    th->exram.ptr[addr - 0x5C00] = val;
                    *do_write = 0;
                }
                return;
            case 2: // any time
                th->exram.ptr[addr - 0x5C00] = val;
                *do_write = 0;
                return;
            case 3: // read-only
                return;
            default: break;
        }
    }
    switch(addr) {
        case 0x2000:
            th->io.ppu.sprites_8x16 = (val >> 5) & 1;
            return;
        case 0x2001:
            th->io.ppu.substitutions_enabled = ((val >> 3) & 3) != 0;
            return;
        case 0x5100: // PRG bank mode
            th->io.PRG_bank_mode = val & 3;
            remap_PRG(bus);
            break;
        case 0x5101: // CHR page size
            th->io.CHR_page_size = val & 3;
            remap_CHR(bus);
            return;
        case 0x5102: // PRG RAM protect 1
            th->io.PRG_RAM_protect[0] = (val & 3) == 2;
            th->io.PRG_RAM_protect_mux = th->io.PRG_RAM_protect[0] && th->io.PRG_RAM_protect[1];
            remap_PRG(bus);
            return;
        case 0x5103:
            th->io.PRG_RAM_protect[1] = (val & 3) == 1;
            th->io.PRG_RAM_protect_mux = th->io.PRG_RAM_protect[0] && th->io.PRG_RAM_protect[1];
            remap_PRG(bus);
            return;
        case 0x5104: // Internal extended RAM mode
            th->io.exram_mode = val & 3;
            remap_exram(bus);
            return;
        case 0x5105: // PPU mirroring mode
            th->nametables[0] = val & 3;
            th->nametables[1] = (val >> 2) & 3;
            th->nametables[2] = (val >> 4) & 3;
            th->nametables[3] = (val >> 6) & 3;
            return;
        case 0x5106:
            th->io.fill_tile = val;
            return;
        case 0x5107: {
            u32 v = val & 3;
            th->io.fill_color = v | (v << 2) | (v << 4) | (v << 6);
            return; }
        case 0x5113: case 0x5114: case 0x5115: case 0x5116: case 0x5117: // PRG RAM bank
            th->io.PRG_bank[addr - 0x5113] = val;
            remap_PRG(bus);
            return;
        case 0x5120: case 0x5121: case 0x5122: case 0x5123: case 0x5124: case 0x5125: // CHR RAM bank
        case 0x5126: case 0x5127: case 0x5128: case 0x5129: case 0x512A: case 0x512B:
            th->io.CHR_bank[addr - 0x5120] = th->io.upper_CHR_bits | val;
            remap_CHR(bus);
            return;
        case 0x5130:
            th->io.upper_CHR_bits = (val & 3) << 8;
            return;
        case 0x5203:
            th->io.irq_scanline = val;
            return;
        case 0x5204:
            th->io.irq_enabled = (val >> 7) & 1;
            return;
        case 0x5205: //
            th->io.multplicand = val;
            return;
        case 0x5206:
            th->io.multiplier = val;
            return;
        default:
            return;
    }
}

static u32 MMC5_readcart(NES_mapper *bus, u32 addr, u32 old_val, u32 has_effect, u32 *do_read)
{
    *do_read = 1;
    THISM;
    if ((addr >= 0x5C00) && (addr < 0x6000)) {
        *do_read = 0;
        switch(th->io.exram_mode) {
            case 0:
            case 1: // only during blanking
                return old_val;
            case 2: // any time
            case 3:
                return th->exram.ptr[addr - 0x5C00];
            default:
        }
    }
    switch(addr) {
        case 0x5204:
            *do_read = 0;
            return (th->irq.in_frame << 7) | (th->irq.pending << 6);
        case 0x5205:
            *do_read = 0;
            return (th->io.multiplier * th->io.multplicand) & 0xFF;
        case 0x5206:
            *do_read = 0;
            return ((th->io.multiplier * th->io.multplicand) >> 8) & 0xFF;
        default:
    }
    return old_val;
}

static void MMC5_setcart(NES_mapper *bus, NES_cart *cart)
{
    bus->ppu_mirror_mode = cart->header.mirroring;
    //1simplebuf8_allocate(&bus->PRG_RAM, 64 * 1024); // Re-allocate to 64KB RAM
    // TODO: was this important?
}

static u32 MMC5_PPU_read(NES_mapper *bus, u32 addr, u32 has_effect)
{
    addr &= 0x3FFF;
    THISM;
    if (addr < 0x2000) // Do
        return th->PPU_map[th->rendering_sprites][addr >> 10].read(addr, 0);
    addr = addr & 0xFFF;
    u32 nt_base = addr & 0xC00;
    u32 nt = nt_base >> 10; // 3
    switch(th->nametables[nt]) {
        case 0: // CIRAM page 0
            return bus->CIRAM.ptr[addr & 0x3FF];
        case 1:
            return bus->CIRAM.ptr[(addr & 0x3FF) + 0x400];
        case 2:
            return th->exram.ptr[addr & 0x3FF];
        case 3:
            addr &= 0x3FF;
            if (th->io.exram_mode != 1) {
                // All attribute fetches get fill mode
                if (addr >= 0x3C0) return th->io.fill_tile;
            }
            if (addr < 0x3C0) {
                return th->exram.ptr[addr];
            }
            else {
                if (th->io.exram_mode == 0) {
                    return th->exram.ptr[addr];
                }
                return 0xFF; // ??? TODO: fix this
            }
        default:
    }
    return 0xFF;
}

static void MMC5_PPU_write(NES_mapper *bus, u32 addr, u32 val)
{

    addr &= 0x3FFF;
    THISM;
    if (addr < 0x2000) // Do
        return th->PPU_map[th->rendering_sprites][addr >> 10].write(addr, val);
    addr = addr & 0xFFF;
    u32 nt_base = addr & 0xC00;
    u32 nt = nt_base >> 10; // 3
    switch(th->nametables[nt]) {
        case 0: // CIRAM page 0
            bus->CIRAM.ptr[addr & 0x3FF] = val;
            return;
        case 1:
            bus->CIRAM.ptr[(addr & 0x3FF) + 0x400] = val;
            return;
        case 2:
            th->exram.ptr[addr & 0x3FF] = val;
            return;
        case 3:
            return;
        default:
    }
}


void MMC5_a12_watch(NES_mapper *bus, u32 addr) {
    THISM;
    if ((addr >= 0x2000) && (addr <= 0x2FFF) && (addr == th->irq.addr))
        th->irq.reads++;
    if (th->irq.reads == 2) {
        if (th->irq.in_frame == 0) {
            th->irq.in_frame = 1;
            th->irq.scanline = 0;
        } else {
            th->irq.scanline++;
            printf("\nNEW SCANLINE! PPUY: %d MYY:%d", bus->nes->clock.ppu_y, th->irq.scanline); //bus->nes->ppu.line_cycle)
            if (th->irq.scanline == th->io.irq_scanline) {
                th->irq.pending = 1;
            }
        }
    }
    else
        th->irq.reads = 0;
    th->irq.addr = addr;
    //ppuIsReading := true
}

void MMC5_init(NES_mapper *bus, NES *nes)
{
    if (bus->ptr != nullptr) free(bus->ptr);
    bus->ptr = malloc(sizeof(MMC5));
    THISM;

    th->exram.allocate(1024);

    th->nes = nes;
    NES_memmap_init_empty(th->PPU_map[0], 0x0000, 0x3FFF, 10);
    NES_memmap_init_empty(th->PPU_map[1], 0x0000, 0x3FFF, 10);

    bus->destruct = &MMC5_destruct;
    bus->reset = &MMC5_reset;
    bus->writecart = &MMC5_writecart;
    bus->readcart = &MMC5_readcart;
    bus->setcart = &MMC5_setcart;
    bus->cpu_cycle = nullptr;
    bus->a12_watch = &MMC5_a12_watch;
    bus->PPU_read_override = &MMC5_PPU_read;
    bus->PPU_write_override = &MMC5_PPU_write;
    bus->serialize = &serialize;
    bus->deserialize = &deserialize;
}

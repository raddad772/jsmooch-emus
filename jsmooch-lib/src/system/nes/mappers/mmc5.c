//
// Created by . on 10/3/24.
//

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include "helpers_c/debugger/debugger.h"
#include "system/nes/nes.h"

#include "mapper.h"
#include "mmc5.h"

#define THISM struct MMC5 *this = (struct MMC5 *)bus->ptr

struct MMC5 {
    struct NES *nes;
    struct simplebuf8 exram;

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

static void map_CHR1K(struct MMC5 *mapper, u32 sprites_status, u32 range_start, u32 range_end, struct simplebuf8 *buf, u32 bank, u32 is_readonly)
{
    NES_memmap_map(mapper->PPU_map[sprites_status], 10, range_start, range_end, buf, bank << 10, is_readonly, NULL, 1, mapper->nes->bus.SRAM);
}

static void map_CHR2K(struct MMC5 *mapper, u32 sprites_status, u32 range_start, u32 range_end, struct simplebuf8 *buf, u32 bank, u32 is_readonly)
{
    NES_memmap_map(mapper->PPU_map[sprites_status], 10, range_start, range_end, buf, bank << 11, is_readonly, NULL, 1, mapper->nes->bus.SRAM);
}

static void map_CHR4K(struct MMC5 *mapper, u32 sprites_status, u32 range_start, u32 range_end, struct simplebuf8 *buf, u32 bank, u32 is_readonly)
{
    NES_memmap_map(mapper->PPU_map[sprites_status], 10, range_start, range_end, buf, bank << 12, is_readonly, NULL, 1, mapper->nes->bus.SRAM);
}

static void map_CHR8K(struct MMC5 *mapper, u32 sprites_status, u32 range_start, u32 range_end, struct simplebuf8 *buf, u32 bank, u32 is_readonly)
{
    NES_memmap_map(mapper->PPU_map[sprites_status], 10, range_start, range_end, buf, bank << 13, is_readonly, NULL, 1, mapper->nes->bus.SRAM);
}

static void remap_CHR(struct NES_bus *bus)
{
    THISM;
    switch(this->io.CHR_page_size) {
        case 0: // 8KB
            map_CHR8K(this, 0, 0x0000, 0x1FFF, &bus->CHR_ROM, this->io.CHR_bank[7], READONLY);

            map_CHR8K(this, 1, 0x0000, 0x1FFF, &bus->CHR_ROM, this->io.CHR_bank[11], READONLY);
            break;
        case 1: // 4KB
            map_CHR4K(this, 0, 0x0000, 0x0FFF, &bus->CHR_ROM, this->io.CHR_bank[3], READONLY);
            map_CHR4K(this, 0, 0x1000, 0x1FFF, &bus->CHR_ROM, this->io.CHR_bank[7], READONLY);

            map_CHR4K(this, 1, 0x0000, 0x0FFF, &bus->CHR_ROM, this->io.CHR_bank[11], READONLY);
            map_CHR4K(this, 1, 0x1000, 0x1FFF, &bus->CHR_ROM, this->io.CHR_bank[11], READONLY);
            break;
        case 2: // 2KB
            map_CHR2K(this, 0, 0x0000, 0x07FF, &bus->CHR_ROM, this->io.CHR_bank[1], READONLY);
            map_CHR2K(this, 0, 0x0800, 0x0FFF, &bus->CHR_ROM, this->io.CHR_bank[3], READONLY);
            map_CHR2K(this, 0, 0x1000, 0x17FF, &bus->CHR_ROM, this->io.CHR_bank[5], READONLY);
            map_CHR2K(this, 0, 0x1800, 0x1FFF, &bus->CHR_ROM, this->io.CHR_bank[7], READONLY);

            map_CHR2K(this, 1, 0x0000, 0x07FF, &bus->CHR_ROM, this->io.CHR_bank[9], READONLY);
            map_CHR2K(this, 1, 0x0800, 0x0FFF, &bus->CHR_ROM, this->io.CHR_bank[11], READONLY);
            map_CHR2K(this, 1, 0x1000, 0x17FF, &bus->CHR_ROM, this->io.CHR_bank[9], READONLY);
            map_CHR2K(this, 1, 0x1800, 0x1FFF, &bus->CHR_ROM, this->io.CHR_bank[11], READONLY);
            break;
        case 3: // 1KB
            map_CHR1K(this, 0, 0x0000, 0x03FF, &bus->CHR_ROM, this->io.CHR_bank[0], READONLY);
            map_CHR1K(this, 0, 0x0400, 0x07FF, &bus->CHR_ROM, this->io.CHR_bank[1], READONLY);
            map_CHR1K(this, 0, 0x0800, 0x0BFF, &bus->CHR_ROM, this->io.CHR_bank[2], READONLY);
            map_CHR1K(this, 0, 0x0C00, 0x0FFF, &bus->CHR_ROM, this->io.CHR_bank[3], READONLY);
            map_CHR1K(this, 0, 0x1000, 0x13FF, &bus->CHR_ROM, this->io.CHR_bank[4], READONLY);
            map_CHR1K(this, 0, 0x1400, 0x17FF, &bus->CHR_ROM, this->io.CHR_bank[5], READONLY);
            map_CHR1K(this, 0, 0x1800, 0x1BFF, &bus->CHR_ROM, this->io.CHR_bank[6], READONLY);
            map_CHR1K(this, 0, 0x1C00, 0x1FFF, &bus->CHR_ROM, this->io.CHR_bank[7], READONLY);

            map_CHR1K(this, 1, 0x0000, 0x03FF, &bus->CHR_ROM, this->io.CHR_bank[8], READONLY);
            map_CHR1K(this, 1, 0x1000, 0x13FF, &bus->CHR_ROM, this->io.CHR_bank[8], READONLY);

            map_CHR1K(this, 1, 0x0400, 0x07FF, &bus->CHR_ROM, this->io.CHR_bank[9], READONLY);
            map_CHR1K(this, 1, 0x1400, 0x17FF, &bus->CHR_ROM, this->io.CHR_bank[9], READONLY);

            map_CHR1K(this, 1, 0x0800, 0x0BFF, &bus->CHR_ROM, this->io.CHR_bank[10], READONLY);
            map_CHR1K(this, 1, 0x1800, 0x1BFF, &bus->CHR_ROM, this->io.CHR_bank[10], READONLY);

            map_CHR1K(this, 1, 0x0C00, 0x0FFF, &bus->CHR_ROM, this->io.CHR_bank[11], READONLY);
            map_CHR1K(this, 1, 0x1C00, 0x1FFF, &bus->CHR_ROM, this->io.CHR_bank[11], READONLY);
            break;
        default:
            assert(1==2);
    }
}

static void map_PRG(struct NES_bus *bus, u32 start_addr, u32 end_addr, u32 val)
{
    THISM;
    struct simplebuf8 *bptr = val & 0x80 ? &bus->PRG_ROM : &bus->fake_PRG_RAM;
    u32 readwrite = val & 0x80 ? READONLY : READWRITE;

    NES_bus_map_PRG8K(bus, start_addr, end_addr, bptr, val & 0x7F, readwrite);
}

static void remap_PRG(struct NES_bus *bus)
{
    THISM;
    switch(this->io.PRG_bank_mode) {
        case 0: // 32KB
            map_PRG(bus, 0x6000, 0x7FFF, this->io.PRG_bank[0] & 0x7C);
            u32 b = (this->io.PRG_bank[4] & 0x7C) | 0x80; // ROM-only in mode 0
            map_PRG(bus, 0x8000, 0x9FFF, b);
            map_PRG(bus, 0xA000, 0xBFFF, b | 1);
            map_PRG(bus, 0xC000, 0xDFFF, b | 2);
            map_PRG(bus, 0xE000, 0xFFFF, b | 3);
            break;
        case 1: // 16KB
            map_PRG(bus, 0x6000, 0x7FFF, this->io.PRG_bank[0] & 0x7E);

            // first two 5115 (2)
            // next two 5117 (4)
            map_PRG(bus, 0x8000, 0x9FFF, this->io.PRG_bank[2] & 0xFE);
            map_PRG(bus, 0xA000, 0xBFFF, this->io.PRG_bank[2] | 1);
            map_PRG(bus, 0xC000, 0xDFFF, this->io.PRG_bank[4] & 0xFE);
            map_PRG(bus, 0xE000, 0xFFFF, this->io.PRG_bank[4] | 1);
            break;
        case 2: // semi-16kb
            map_PRG(bus, 0x6000, 0x7FFF, this->io.PRG_bank[0] & 0x7E);

            map_PRG(bus, 0x8000, 0x9FFF, this->io.PRG_bank[2] & 0xFE);
            map_PRG(bus, 0xA000, 0xBFFF, this->io.PRG_bank[2] | 1);
            map_PRG(bus, 0xC000, 0xDFFF, this->io.PRG_bank[3]);
            map_PRG(bus, 0xE000, 0xFFFF, this->io.PRG_bank[4]);

            break;

        case 3: // 8kb all around!
            map_PRG(bus, 0x6000, 0x7FFF, this->io.PRG_bank[0] & 0x7E);

            map_PRG(bus, 0x8000, 0x9FFF, this->io.PRG_bank[1]);
            map_PRG(bus, 0xA000, 0xBFFF, this->io.PRG_bank[2]);
            map_PRG(bus, 0xC000, 0xDFFF, this->io.PRG_bank[3]);
            map_PRG(bus, 0xE000, 0xFFFF, this->io.PRG_bank[4]);
            break;
        default:
            assert(1==2);
    }
}

static void remap_exram(struct NES_bus *bus)
{

}

static void remap(struct NES_bus *bus, u32 boot)
{
    THISM;
    if (boot) {
        //assert(1 == 2);
    }

    remap_CHR(bus);
    remap_PRG(bus);
    remap_exram(bus);
}

static void serialize(struct NES_bus *bus, struct serialized_state *state)
{
    THISM;
#define S(x) Sadd(state, &this-> x, sizeof(this-> x))
#undef S
}

static void deserialize(struct NES_bus *bus, struct serialized_state *state)
{
    THISM;
#define L(x) Sload(state, &this-> x, sizeof(this-> x))
#undef L
}

static void MMC5_destruct(struct NES_bus *bus)
{
    THISM;
    NES_memmap_init_empty(this->PPU_map[0], 0x0000, 0x3FFF, 10);
    NES_memmap_init_empty(this->PPU_map[1], 0x0000, 0x3FFF, 10);
    simplebuf8_delete(&this->exram);
}

static void MMC5_reset(struct NES_bus *bus)
{
    printf("\nMMC5 Resetting, so remapping bus...");
    THISM;
    this->io.PRG_bank_mode = 3;
    this->io.upper_CHR_bits = 0;
    this->io.PRG_bank[4] = 0xFF;
    this->io.multiplier = this->io.multplicand = 0xFF;
    remap(bus, 1);
}


static void MMC5_writecart(struct NES_bus *bus, u32 addr, u32 val, u32 *do_write)
{
    *do_write = 1;
    THISM;
    // EXRAM
    if ((addr >= 0x5C00) && (addr < 0x6000)) {
        switch(this->io.exram_mode) {
            case 0:
            case 1: // only during blanking
                if (this->blanking) {
                    this->exram.ptr[addr - 0x5C00] = val;
                    *do_write = 0;
                }
                return;
            case 2: // any time
                this->exram.ptr[addr - 0x5C00] = val;
                *do_write = 0;
                return;
            case 3: // read-only
                return;
        }
    }
    switch(addr) {
        case 0x2000:
            this->io.ppu.sprites_8x16 = (val >> 5) & 1;
            return;
        case 0x2001:
            this->io.ppu.substitutions_enabled = ((val >> 3) & 3) != 0;
            return;
        case 0x5100: // PRG bank mode
            this->io.PRG_bank_mode = val & 3;
            remap_PRG(bus);
            break;
        case 0x5101: // CHR page size
            this->io.CHR_page_size = val & 3;
            remap_CHR(bus);
            return;
        case 0x5102: // PRG RAM protect 1
            this->io.PRG_RAM_protect[0] = (val & 3) == 2;
            this->io.PRG_RAM_protect_mux = this->io.PRG_RAM_protect[0] && this->io.PRG_RAM_protect[1];
            remap_PRG(bus);
            return;
        case 0x5103:
            this->io.PRG_RAM_protect[1] = (val & 3) == 1;
            this->io.PRG_RAM_protect_mux = this->io.PRG_RAM_protect[0] && this->io.PRG_RAM_protect[1];
            remap_PRG(bus);
            return;
        case 0x5104: // Internal extended RAM mode
            this->io.exram_mode = val & 3;
            remap_exram(bus);
            return;
        case 0x5105: // PPU mirroring mode
            this->nametables[0] = val & 3;
            this->nametables[1] = (val >> 2) & 3;
            this->nametables[2] = (val >> 4) & 3;
            this->nametables[3] = (val >> 6) & 3;
            return;
        case 0x5106:
            this->io.fill_tile = val;
            return;
        case 0x5107: {
            u32 v = val & 3;
            this->io.fill_color = v | (v << 2) | (v << 4) | (v << 6);
            return; }
        case 0x5113: case 0x5114: case 0x5115: case 0x5116: case 0x5117: // PRG RAM bank
            this->io.PRG_bank[addr - 0x5113] = val;
            remap_PRG(bus);
            return;
        case 0x5120: case 0x5121: case 0x5122: case 0x5123: case 0x5124: case 0x5125: // CHR RAM bank
        case 0x5126: case 0x5127: case 0x5128: case 0x5129: case 0x512A: case 0x512B:
            this->io.CHR_bank[addr - 0x5120] = this->io.upper_CHR_bits | val;
            remap_CHR(bus);
            return;
        case 0x5130:
            this->io.upper_CHR_bits = (val & 3) << 8;
            return;
        case 0x5203:
            this->io.irq_scanline = val;
            return;
        case 0x5204:
            this->io.irq_enabled = (val >> 7) & 1;
            return;
        case 0x5205: //
            this->io.multplicand = val;
            return;
        case 0x5206:
            this->io.multiplier = val;
            return;
    }
}

static void PPU_mirror_set(struct NES_bus *bus)
{
    THISM;
    /*switch(bus->ppu_mirror_mode) {
        case PPUM_Vertical:
            bus->ppu_mirror = &NES_mirror_ppu_vertical;
            break;
        case PPUM_Horizontal:
            bus->ppu_mirror = &NES_mirror_ppu_horizontal;
            break;
        case PPUM_FourWay:
            bus->ppu_mirror = &NES_mirror_ppu_four;
            break;
        case PPUM_ScreenAOnly:
            bus->ppu_mirror = &NES_mirror_ppu_Aonly;
            break;
        case PPUM_ScreenBOnly:
            bus->ppu_mirror = &NES_mirror_ppu_Bonly;
            break;
        default:
            assert(1==0);
    }*/
    for (u32 addr = 0x2000; addr < 0x3FFF; addr += 0x400) {
        struct NES_memmap *m = &bus->PPU_map[addr >> 10];
        m->offset = bus->ppu_mirror(addr);
        m->mask = 0x3FF;
        //printf("\nPPUmap %04x to offset %04x", addr, m->offset);
    }
}

static u32 MMC5_readcart(struct NES_bus *bus, u32 addr, u32 old_val, u32 has_effect, u32 *do_read)
{
    *do_read = 1;
    THISM;
    if ((addr >= 0x5C00) && (addr < 0x6000)) {
        *do_read = 0;
        switch(this->io.exram_mode) {
            case 0:
            case 1: // only during blanking
                return old_val;
            case 2: // any time
            case 3:
                return this->exram.ptr[addr - 0x5C00];
        }
    }
    switch(addr) {
        case 0x5204:
            *do_read = 0;
            return (this->irq.in_frame << 7) | (this->irq.pending << 6);
        case 0x5205:
            *do_read = 0;
            return (this->io.multiplier * this->io.multplicand) & 0xFF;
        case 0x5206:
            *do_read = 0;
            return ((this->io.multiplier * this->io.multplicand) >> 8) & 0xFF;
    }
    return old_val;
}

static void MMC5_setcart(struct NES_bus *bus, struct NES_cart *cart)
{
    bus->ppu_mirror_mode = cart->header.mirroring;
    //1simplebuf8_allocate(&bus->PRG_RAM, 64 * 1024); // Re-allocate to 64KB RAM
    // TODO: was this important?
}

static u32 MMC5_PPU_read(struct NES_bus *bus, u32 addr, u32 has_effect)
{
    addr &= 0x3FFF;
    THISM;
    if (addr < 0x2000) // Do
        return NES_mmap_read(&this->PPU_map[this->rendering_sprites][addr >> 10], addr, 0);
    addr = addr & 0xFFF;
    u32 nt_base = addr & 0xC00;
    u32 nt = nt_base >> 10; // 3
    switch(this->nametables[nt]) {
        case 0: // CIRAM page 0
            return bus->CIRAM.ptr[addr & 0x3FF];
        case 1:
            return bus->CIRAM.ptr[(addr & 0x3FF) + 0x400];
        case 2:
            return this->exram.ptr[addr & 0x3FF];
        case 3:
            addr &= 0x3FF;
            if (this->io.exram_mode != 1) {
                // All attribute fetches get fill mode
                if (addr >= 0x3C0) return this->io.fill_tile;
            }
            if (addr < 0x3C0) {
                return this->exram.ptr[addr];
            }
            else {
                if (this->io.exram_mode == 0) {
                    return this->exram.ptr[addr];
                }
                return 0xFF; // ??? TODO: fix this
            }
    }
    return 0xFF;
}

static void MMC5_PPU_write(struct NES_bus *bus, u32 addr, u32 val)
{

    addr &= 0x3FFF;
    THISM;
    if (addr < 0x2000) // Do
        return NES_mmap_write(&this->PPU_map[this->rendering_sprites][addr >> 10], addr, val);
    addr = addr & 0xFFF;
    u32 nt_base = addr & 0xC00;
    u32 nt = nt_base >> 10; // 3
    switch(this->nametables[nt]) {
        case 0: // CIRAM page 0
            bus->CIRAM.ptr[addr & 0x3FF] = val;
            return;
        case 1:
            bus->CIRAM.ptr[(addr & 0x3FF) + 0x400] = val;
            return;
        case 2:
            this->exram.ptr[addr & 0x3FF] = val;
            return;
        case 3:
            return;
    }
}


void MMC5_a12_watch(struct NES_bus *bus, u32 addr) {
    THISM;
    if ((addr >= 0x2000) && (addr <= 0x2FFF) && (addr == this->irq.addr))
        this->irq.reads++;
    if (this->irq.reads == 2) {
        if (this->irq.in_frame == 0) {
            this->irq.in_frame = 1;
            this->irq.scanline = 0;
        } else {
            this->irq.scanline++;
            printf("\nNEW SCANLINE! PPUY: %d MYY:%d", bus->nes->clock.ppu_y, this->irq.scanline); //bus->nes->ppu.line_cycle)
            if (this->irq.scanline == this->io.irq_scanline) {
                this->irq.pending = 1;
            }
        }
    }
    else
        this->irq.reads = 0;
    this->irq.addr = addr;
    //ppuIsReading := true
}

void MMC5_init(struct NES_bus *bus, struct NES *nes)
{
    if (bus->ptr != NULL) free(bus->ptr);
    bus->ptr = malloc(sizeof(struct MMC5));
    struct MMC5 *this = (struct MMC5*)bus->ptr;

    simplebuf8_init(&this->exram);
    simplebuf8_allocate(&this->exram, 1024);

    this->nes = nes;
    NES_memmap_init_empty(this->PPU_map[0], 0x0000, 0x3FFF, 10);
    NES_memmap_init_empty(this->PPU_map[1], 0x0000, 0x3FFF, 10);

    bus->destruct = &MMC5_destruct;
    bus->reset = &MMC5_reset;
    bus->writecart = &MMC5_writecart;
    bus->readcart = &MMC5_readcart;
    bus->setcart = &MMC5_setcart;
    bus->cpu_cycle = NULL;
    bus->a12_watch = &MMC5_a12_watch;
    bus->PPU_read_override = &MMC5_PPU_read;
    bus->PPU_write_override = &MMC5_PPU_write;
    bus->serialize = &serialize;
    bus->deserialize = &deserialize;
}

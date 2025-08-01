//
// Created by . on 4/21/25.
//

#include "snes_bus.h"
#include "snes_mem.h"
#include "snesdefs.h"
#include "snes_ppu.h"
#include "r5a22.h"

static void write_bad(struct SNES *this, u32 addr, u32 val, struct SNES_memmap_block *bl)
{
    printf("\nWARN BAD WRITE %06x:%02x", addr, val);
}

static u32 read_bad(struct SNES *this, u32 addr, u32 old, u32 has_effect, struct SNES_memmap_block *bl)
{
    if (has_effect) {
        printf("\nWARN BAD READ %06x", addr);
        static int num = 0;
        num++;
        if (num > 0) {
//        dbg_break("BECAUSE", this->clock.master_cycle_count);
        }
    }
    return old;
}

static void clear_map(struct SNES *this)
{
    printf("\nCLEAR MAP!");
    for (u32 i = 0; i < 0x1000; i++) {
        this->mem.read[i] = &read_bad;
        this->mem.write[i] = &write_bad;
        this->mem.blockmap[i] = (struct SNES_memmap_block){.kind=SMB_open};
    }
}

void SNES_mem_init(struct SNES *this)
{
    clear_map(this);
}

// typedef void (*SNES_memmap_write)(struct SNES *, u32 addr, u32 val, struct SNES_memmap_block *bl);
//typedef u32 (*SNES_memmap_read)(struct SNES *, u32 addr, u32 old, u32 has_effect, struct SNES_memmap_block *bl);
static void write_WRAM(struct SNES *this, u32 addr, u32 val, struct SNES_memmap_block *bl)
{
    this->mem.WRAM[((addr & 0xFFF) + bl->offset) & 0x1FFFF] = val;
}

static u32 read_WRAM(struct SNES *this, u32 addr, u32 old, u32 has_effect, struct SNES_memmap_block *bl)
{
    return this->mem.WRAM[((addr & 0xFFF) + bl->offset) & 0x1FFFF];
}

static void write_loROM(struct SNES *this, u32 addr, u32 val, struct SNES_memmap_block *bl)
{
    static int a = 1;
    printf("\nWARNING writes to ROM area! %06x", addr);
    if (a) {
        a = 0;
        printf("\nWARNING writes to ROM area!");
    }
}

static u32 read_loROM(struct SNES *this, u32 addr, u32 old, u32 has_effect, struct SNES_memmap_block *bl)
{
    u32 maddr = ((addr & 0xFFF) + bl->offset) % this->cart.ROM.size;
    return ((u8 *)this->cart.ROM.ptr)[maddr];
}

static void write_SRAM(struct SNES *this, u32 addr, u32 val, struct SNES_memmap_block *bl)
{
    ((u8 *)this->cart.SRAM->data)[((addr & 0xFFF) + bl->offset) & this->cart.header.sram_mask] = val;
    this->cart.SRAM->dirty = 1;
}

static u32 read_SRAM(struct SNES *this, u32 addr, u32 old, u32 has_effect, struct SNES_memmap_block *bl)
{
    return ((u8 *)this->cart.SRAM->data)[((addr & 0xFFF) + bl->offset) & this->cart.header.sram_mask];
}

static void map_generic(struct SNES *this, u32 bank_start, u32 bank_end, u32 addr_start, u32 addr_end, u32 offset, enum SMB_kind kind, SNES_memmap_read rfunc, SNES_memmap_write wfunc) {
    for (u32 c = bank_start; c <= bank_end; c++) {
        for (u32 i = addr_start; i <= addr_end; i += 0x1000) {
            u32 b = (c << 4) | (i >> 12);
            this->mem.blockmap[b].kind = kind;
            this->mem.blockmap[b].offset = (i - addr_start) + offset;
            this->mem.read[b] = rfunc;
            this->mem.write[b] = wfunc;
        }
    }
}

static void map_loram(struct SNES *this, u32 bank_start, u32 bank_end, u32 addr_start, u32 addr_end, u32 offset)
{
    map_generic(this, bank_start, bank_end, addr_start, addr_end, offset, SMB_WRAM, &read_WRAM, &write_WRAM);
}

static void map_hirom(struct SNES *this, u32 bank_start, u32 bank_end, u32 addr_start, u32 addr_end, u32 offset, u32 bank_mask)
{
    for (u32 c = bank_start; c <= bank_end; c++) {
        for (u32 i = addr_start; i <= addr_end; i += 0x1000) {
            u32 b = (c << 4) | (i >> 12);
            u32 mapaddr = ((c & bank_mask) << 16) | i;
            //printf("\nMAP %06X to %06X", (c << 16) | i, mapaddr);
            mapaddr %= this->cart.header.rom_size;
            this->mem.blockmap[b].kind = SMB_ROM;
            this->mem.blockmap[b].offset = mapaddr;
            this->mem.read[b] = &read_loROM;
            this->mem.write[b] = &write_loROM;
            //printf("\nMap %06x with offset %04x", (c << 16) | i, offset);
        }
    }
}


static void map_lorom(struct SNES *this, u32 bank_start, u32 bank_end, u32 addr_start, u32 addr_end)
{
    u32 offset = 0;
    for (u32 c = bank_start; c <= bank_end; c++) {
        for (u32 i = addr_start; i <= addr_end; i += 0x1000) {
            u32 b = (c << 4) | (i >> 12);
            this->mem.blockmap[b].kind = SMB_ROM;
            this->mem.blockmap[b].offset = offset;
            this->mem.read[b] = &read_loROM;
            this->mem.write[b] = &write_loROM;
            //printf("\nMap %06x with offset %04x", (c << 16) | i, offset);
            offset += 0x1000;
            if (offset >= this->mem.ROMSize) offset = 0;
        }
    }
}

static void sys_map_lo(struct SNES *this)
{
    for (u32 bank = 0; bank < 0xFF; bank += 0x80) {
        map_loram(this, bank+0x00, bank+0x3F, 0x0000, 0x1FFF, 0);
        map_generic(this, bank+0x00, bank+0x3F, 0x2000, 0x3FFF, 0x2000, SMB_PPU, &SNES_PPU_read, &SNES_PPU_write);
        map_generic(this, bank+0x00, bank+0x3F, 0x4000, 0x5FFF, 0x4000, SMB_CPU, &R5A22_reg_read, &R5A22_reg_write);
    }
}

static void sys_map_hi(struct SNES *this)
{
    for (u32 bank = 0; bank < 0xFF; bank += 0x80) {
        map_loram(this, bank + 0x00, bank + 0x3F, 0x0000, 0x1FFF, 0);
        map_generic(this, bank + 0x00, bank + 0x3F, 0x2000, 0x3FFF, 0x2000, SMB_PPU, &SNES_PPU_read, &SNES_PPU_write);
        map_generic(this, bank + 0x00, bank + 0x3F, 0x4000, 0x5FFF, 0x4000, SMB_CPU, &R5A22_reg_read, &R5A22_reg_write);
    }
}

static void map_sram_hi(struct SNES *this, u32 bank_start, u32 bank_end)
{
    u32 offset = 0;
    for (u32 c = bank_start; c <= bank_end; c++) {
        for (u32 i = 0x6000; i < 0x7FFF; i += 0x1000) {
            u32 b = (c << 4) | (i >> 12);
            this->mem.blockmap[b].kind = SMB_SRAM;
            this->mem.blockmap[b].offset = offset;
            this->mem.read[b] = &read_SRAM;
            this->mem.write[b] = &write_SRAM;
            offset += 0x1000;
            if (offset >= this->mem.SRAMSize) offset = 0;
        }
    }
}


static void map_sram(struct SNES *this, u32 bank_start, u32 bank_end, u32 addr_start, u32 addr_end)
{
    u32 offset = 0;
    for (u32 c = bank_start; c <= bank_end; c++) {
        for (u32 i = addr_start; i <= addr_end; i += 0x1000) {
            u32 b = (c << 4) | (i >> 12);
            this->mem.blockmap[b].kind = SMB_SRAM;
            this->mem.blockmap[b].offset = offset;
            this->mem.read[b] = &read_SRAM;
            this->mem.write[b] = &write_SRAM;
            offset += 0x1000;
            if (offset >= this->mem.SRAMSize) offset = 0;
        }
    }
}

static void map_wram(struct SNES *this, u32 bank_start, u32 bank_end, u32 addr_start, u32 addr_end)
{
    u32 offset = 0;
    for (u32 c = bank_start; c <= bank_end; c++) {
        for (u32 i = addr_start; i <= addr_end; i += 0x1000) {
            u32 b = (c << 4) | (i >> 12);
            this->mem.blockmap[b].kind = SMB_WRAM;
            this->mem.blockmap[b].offset = offset;
            this->mem.read[b] = &read_WRAM;
            this->mem.write[b] = &write_WRAM;
            offset += 0x1000;
        }
    }
}

static void map_hirom_sram(struct SNES *this)
{
    u32 mask = this->cart.header.sram_mask;
    map_sram_hi(this, 0x20, 0x3F);
    map_sram_hi(this, 0xA0, 0xBF);
}

static void map_lorom_sram(struct SNES *this)
{
    // TODO: fix this
    u32 hi;
    if (this->mem.ROMSizebit > 11 || this->mem.SRAMSizebit > 5)
        hi = 0x7FFF;
    else
        hi = 0xFFFF;

    // HMMM...
    hi = 0x7FFF;
    map_sram(this, 0x70, 0x7D, 0x0000, hi);
    map_sram(this, 0xF0, 0xFF, 0x0000, hi);
}

static void setup_mem_map_lorom(struct SNES *this)
{
    clear_map(this);

    sys_map_lo(this);
    if (this->mem.ROMSize > (2 * 1024 * 1024)) {
        printf("\nBLROM MAPPING!");
        map_lorom(this, 0x00, 0x7F, 0x8000, 0xFFFF);
        map_lorom(this, 0x80, 0xFF, 0x8000, 0xFFFF);
    }
    else {
        map_lorom(this, 0x00, 0x3F, 0x8000, 0xFFFF);
        map_lorom(this, 0x40, 0x7F, 0x8000, 0xFFFF);
        map_lorom(this, 0x80, 0xBF, 0x8000, 0xFFFF);
        map_lorom(this, 0xC0, 0xFF, 0x8000, 0xFFFF);
    }

    map_lorom_sram(this);
    map_loram(this, 0x00, 0x3F, 0x0000, 0x1FFF, 0);
    map_loram(this, 0x80, 0xBF, 0x0000, 0x1FFF, 0);
    map_wram(this, 0x7E, 0x7F, 0x0000, 0xFFFF);
}

static void setup_mem_map_hirom(struct SNES *this)
{
    clear_map(this);
    printf("\nMEM MAPPING!");


    sys_map_hi(this);

    map_hirom(this, 0x00, 0x3F, 0x8000, 0xFFFF, 0, 0x3F);
    map_hirom(this, 0x40, 0x7D, 0x0000, 0xFFFF, 0, 0x3F);
    map_hirom(this, 0x80, 0xBF, 0x0000, 0xFFFF, 0, 0x3F);
    map_hirom(this, 0xC0, 0xFF, 0x0000, 0xFFFF, 0, 0x3F);

    map_loram(this, 0x00, 0x3F, 0x0000, 0x1FFF, 0);
    map_loram(this, 0x80, 0xBF, 0x0000, 0x1FFF, 0);
    map_hirom_sram(this);

    map_wram(this, 0x7E, 0x7F, 0x0000, 0xFFFF);
}

void SNES_mem_cart_inserted(struct SNES *this)
{
    this->mem.ROMSizebit = this->cart.header.rom_sizebit;
    this->mem.SRAMSizebit = this->cart.header.sram_sizebit;
    this->mem.ROMSize = this->cart.ROM.size;
    this->mem.SRAMSize = this->cart.header.sram_size;

    /// TODO: SRAM
    if (this->cart.header.lorom)
        setup_mem_map_lorom(this);
    else
        setup_mem_map_hirom(this);
}


u32 SNES_wdc65816_read(struct SNES *this, u32 addr, u32 old, u32 has_effect)
{
    return this->mem.read[addr >> 12](this, addr, old, has_effect, &this->mem.blockmap[addr >> 12]);
}

void SNES_wdc65816_write(struct SNES *this, u32 addr, u32 val)
{
    this->mem.write[addr >> 12](this, addr, val, &this->mem.blockmap[addr >> 12]);
}

u32 SNES_spc700_read(struct SNES *this, u32 addr, u32 old, u32 has_effect)
{
    return SPC700_read8(&this->apu.cpu, addr);
}

void SNES_spc700_write(struct SNES *this, u32 addr, u32 val)
{
    SPC700_write8(&this->apu.cpu, addr, val);
}


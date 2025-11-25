//
// Created by . on 4/21/25.
//

#pragma once

#include "helpers/buf.h"
#include "helpers/int.h"
namespace SNES {

struct memmap_block {
    enum kinds {
        open,
        CPU,
        APU,
        PPU,
        WRAM,
        ROM,
        SRAM
    } kind;
    //u8 speed;
    u32 offset, mask;

    void clear(kinds in_kind) {offset = mask = 0; kind = in_kind;}
};
struct core;


struct mem {
    explicit mem(core *parent) : snes(parent) {}
    memmap_block blockmap[0x1000];
    typedef void (mem::*memmap_write)(u32 addr, u32 val, memmap_block *bl);
    typedef u32 (mem::*memmap_read)(u32 addr, u32 old, u32 has_effect, memmap_block *bl);
    memmap_read read[0x1000];
    memmap_write write[0x1000];
    core *snes;

    u8 WRAM[0x20000]{};

    u32 ROMSizebit{};
    u32 SRAMSizebit{};
    u32 ROMSize{};
    u32 SRAMSize{};
    buf ROM{};
    u32 read_bus_A(u32 addr, u32 old, u32 has_effect);
    void write_bus_A(u32 addr, u32 val);
    void cart_inserted();

private:
    void write_bad(u32 addr, u32 val, memmap_block *bl);
    u32 read_bad(u32 addr, u32 old, u32 has_effect, memmap_block *bl);
    void write_WRAM(u32 addr, u32 val, memmap_block *bl);
    u32 read_WRAM(u32 addr, u32 old, u32 has_effect, memmap_block *bl);
    void write_loROM(u32 addr, u32 val, memmap_block *bl);
    u32 read_loROM(u32 addr, u32 old, u32 has_effect, memmap_block *bl);
    void write_SRAM(u32 addr, u32 val, memmap_block *bl);
    u32 read_SRAM(u32 addr, u32 old, u32 has_effect, memmap_block *bl);
    void map_generic(u32 bank_start, u32 bank_end, u32 addr_start, u32 addr_end, u32 offset, memmap_block::kinds kind, memmap_read rfunc, memmap_write wfunc);
    void map_loram(u32 bank_start, u32 bank_end, u32 addr_start, u32 addr_end, u32 offset);
    void map_hirom(u32 bank_start, u32 bank_end, u32 addr_start, u32 addr_end, u32 offset, u32 bank_mask);
    void map_lorom(u32 bank_start, u32 bank_end, u32 addr_start, u32 addr_end);
    void sys_map_lo();
    void sys_map_hi();
    void map_sram_hi(u32 bank_start, u32 bank_end);
    void map_sram(u32 bank_start, u32 bank_end, u32 addr_start, u32 addr_end);
    void map_wram(u32 bank_start, u32 bank_end, u32 addr_start, u32 addr_end);
    void map_hirom_sram();
    void map_lorom_sram();
    void setup_mem_map_lorom();
    void setup_mem_map_hirom();

    u32 read_R5A22(u32 addr, u32 old, u32 has_effect, memmap_block *bl);
    void write_R5A22(u32 addr, u32 val, memmap_block *bl);
    u32 read_PPU(u32 addr, u32 old, u32 has_effect, memmap_block *bl);
    void write_PPU(u32 addr, u32 val, memmap_block *bl);

    void clear_map();
};
}
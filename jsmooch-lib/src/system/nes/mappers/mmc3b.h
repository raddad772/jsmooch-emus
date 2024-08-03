//
// Created by Dave on 2/6/2024.
//

#ifndef JSMOOCH_EMUS_MMC3B_H
#define JSMOOCH_EMUS_MMC3B_H

#include "helpers/int.h"
#include "helpers/buf.h"
#include "a12_watcher.h"
#include "mapper.h"

struct NES;

struct MMC3b_map {
    u32 addr;
    u32 offset;
    u32 ROM;
    u32 RAM;
    u8* data;
};

struct NES_mapper_MMC3b {
    struct NES_a12_watcher a12_watcher;
    struct buf CHR_ROM;
    struct buf PRG_ROM;
    struct buf CHR_RAM;
    struct buf PRG_RAM;

    struct MMC3b_map PRG_map[5];
    struct MMC3b_map CHR_map[8];

    u32 prg_ram_size;
    u32 has_chr_ram;
    struct {
        u32 rC000;
        u32 bank_select;
        u32 bank[8];
    } regs;

    struct {
        u32 ROM_bank_mode;
        u32 CHR_mode;
        u32 PRG_mode;
    } status;

    u32 irq_enable;
    i32 irq_counter;
    u32 irq_reload;

    u8 CIRAM[0x2000]; // PPU RAM
    u8 CPU_RAM[0x800];

    mirror_ppu_t ppu_mirror;
    u32 ppu_mirror_mode;
    u32 num_PRG_banks;
    u32 num_CHR_banks;
};

void NES_mapper_MMC3b_init(struct NES_mapper* mapper, struct NES* nes);
void NES_mapper_MMC3b_delete(struct NES_mapper* mapper);
void MMC3b_map_init(struct MMC3b_map*);
void MMC3b_map_set(struct MMC3b_map*, u32 addr, u32 offset, u32 ROM, u32 RAM);
void MMC3b_map_write(struct MMC3b_map*, u32 addr, u32 val);
u32 MMC3b_map_read(struct MMC3b_map*, u32 addr, u32 val);

#endif //JSMOOCH_EMUS_MMC3B_H

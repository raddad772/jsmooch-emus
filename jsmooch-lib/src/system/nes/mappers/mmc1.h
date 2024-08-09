//
// Created by Dave on 2/7/2024.
//

#ifndef JSMOOCH_EMUS_MMC1_H
#define JSMOOCH_EMUS_MMC1_H
#include "helpers/int.h"
#include "helpers/buf.h"
#include "a12_watcher.h"
#include "mmc3b.h"
#include "mapper.h"

struct NES;

struct NES_mapper_MMC1 {
    struct NES_a12_watcher a12_watcher;
    struct buf CHR_ROM;
    struct buf PRG_ROM;
    struct buf CHR_RAM;
    struct buf PRG_RAM;

    struct NES* bus;

    /*
     *
     */
    u8 CIRAM[0x2000]; // PPU RAM
    u8 CPU_RAM[0x800];

    mirror_ppu_t ppu_mirror;
    u32 ppu_mirror_mode;

    u32 prg_ram_size;
    u32 chr_ram_size;
    u32 has_chr_ram;
    u32 has_prg_ram;

    u64 last_cycle_write;

    struct MMC3b_map PRG_map[2];
    struct MMC3b_map CHR_map[2];

    u32 num_CHR_banks;
    u32 num_PRG_banks;

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

void NES_mapper_MMC1_init(struct NES_mapper* mapper, struct NES* nes);
void NES_mapper_MMC1_delete(struct NES_mapper* mapper);

#endif //JSMOOCH_EMUS_MMC1_H

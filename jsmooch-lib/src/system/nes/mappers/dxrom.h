//
// Created by Dave on 2/7/2024.
//

#ifndef JSMOOCH_EMUS_DXROM_H
#define JSMOOCH_EMUS_DXROM_H
#include "helpers/int.h"
#include "helpers/buf.h"
#include "a12_watcher.h"
#include "mapper.h"

struct NES;

struct NES_mapper_DXROM {
    struct NES_a12_watcher a12_watcher;
    struct buf CHR_ROM;
    struct buf PRG_ROM;

    u8 CIRAM[0x2000]; // PPU RAM
    u8 CPU_RAM[0x800];

    u32 num_PRG_banks;
    u32 num_CHR_banks;
    struct {
        u32 select;
        u32 prg_banks[4];
        u32 chr_banks[8];
    } regs;

    mirror_ppu_t ppu_mirror;
    u32 ppu_mirror_mode;
};

void NES_mapper_DXROM_init(struct NES_mapper* mapper, struct NES* nes);
void NES_mapper_DXROM_delete(struct NES_mapper* mapper);

#endif //JSMOOCH_EMUS_DXROM_H

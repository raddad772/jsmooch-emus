//
// Created by Dave on 2/7/2024.
//

#ifndef JSMOOCH_EMUS_CNROM_H
#define JSMOOCH_EMUS_CNROM_H

#include "helpers/int.h"
#include "helpers/buf.h"
#include "a12_watcher.h"
#include "mapper.h"

struct NES;

struct NES_mapper_CNROM {
    struct NES_a12_watcher a12_watcher;
    struct buf CHR_ROM;
    struct buf PRG_ROM;

    struct NES* bus;
    /*
     *
     */
    u8 CIRAM[0x2000]; // PPU RAM
    u8 CPU_RAM[0x800];

    mirror_ppu_t ppu_mirror;
    u32 ppu_mirror_mode;

    u32 num_CHR_banks;
    u32 PRG_ROM_mask;
    u32 chr_bank_offset;
};

void NES_mapper_CNROM_init(struct NES_mapper* mapper, struct NES* nes);
void NES_mapper_CNROM_delete(struct NES_mapper* mapper);


#endif //JSMOOCH_EMUS_CNROM_H

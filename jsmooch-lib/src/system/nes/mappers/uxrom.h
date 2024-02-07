//
// Created by Dave on 2/7/2024.
//

#ifndef JSMOOCH_EMUS_UXROM_H
#define JSMOOCH_EMUS_UXROM_H

#include "helpers/int.h"
#include "helpers/buf.h"
#include "a12_watcher.h"
#include "mapper.h"

struct NES;

struct NES_mapper_UXROM {
    struct NES_a12_watcher a12_watcher;
    struct buf PRG_ROM;
    u8 CHR_RAM[0x2000];

    u8 CIRAM[0x2000]; // PPU RAM
    u8 CPU_RAM[0x800];

    mirror_ppu_t ppu_mirror;
    u32 ppu_mirror_mode;

    u32 num_PRG_banks;
    u32 PRG_ROM_size;
    u32 prg_bank_offset;
    u32 last_bank_offset;

    u32 is_unrom;
    u32 is_uorom;
};

void NES_mapper_UXROM_init(struct NES_mapper* mapper, struct NES* nes);
void NES_mapper_UXROM_delete(struct NES_mapper* mapper);

#endif //JSMOOCH_EMUS_UXROM_H

//
// Created by . on 9/17/24.
//

#ifndef JSMOOCH_EMUS_SUNSOFT_57_H
#define JSMOOCH_EMUS_SUNSOFT_57_H

#include "helpers/int.h"
#include "helpers/buf.h"
#include "a12_watcher.h"
#include "mapper.h"

#include "mmc3b.h"

struct NES;

enum NES_mapper_sunsoft_57_variants {
    NES_mapper_sunsoft_5a,
    NES_mapper_sunsoft_5b,
    NES_mapper_sunsoft_fme_7
};

struct NES_mapper_sunsoft_57 {
    struct NES_a12_watcher a12_watcher;
    struct buf CHR_ROM;
    struct buf PRG_ROM;
    struct NES *bus;

    enum NES_mapper_sunsoft_57_variants variant;
    u32 bank_mask;
    u32 has_sound;

    u8 CIRAM[0x2000];
    u8 CPU_RAM[0x800];

    struct buf PRG_RAM;

    struct MMC3b_map PRG_map[8];
    struct MMC3b_map CHR_map[8];

    u32 prg_ram_size;
    u32 num_CHR_banks;
    u32 num_PRG_banks;

    mirror_ppu_t ppu_mirror;
    u32 ppu_mirror_mode;

    struct {
        u32 counter;
        u32 output;
        u32 enabled;
        u32 counter_enabled;
    } irq;
    struct {
        struct {
            u32 banks[4];
        } cpu;
        struct {
            u32 banks[8];
        } ppu;
        u32 reg;
        u32 wram_enabled, wram_mapped;

        struct {
            u32 reg;
            u32 write_enabled;
        } audio;
    } io;
};

void NES_mapper_sunsoft_57_init(struct NES_mapper* mapper, struct NES* nes, enum NES_mapper_sunsoft_57_variants variant);
void NES_mapper_sunsoft_57_delete(struct NES_mapper* mapper);

#endif //JSMOOCH_EMUS_SUNSOFT_57_H

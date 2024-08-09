//
// Created by Dave on 2/7/2024.
//

#ifndef JSMOOCH_EMUS_VRC_2B_4E_4F_H
#define JSMOOCH_EMUS_VRC_2B_4E_4F_H

#include "helpers/int.h"
#include "helpers/buf.h"
#include "a12_watcher.h"
#include "mapper.h"

#include "mmc3b.h"

struct NES;

struct NES_mapper_VRC2B_4E_4F {
    struct NES_a12_watcher a12_watcher;
    struct buf CHR_ROM;
    struct buf PRG_ROM;
    struct NES* bus;

    /*
     *
     */
    u8 CIRAM[0x2000]; // PPU RAM
    u8 CPU_RAM[0x800];

    struct buf PRG_RAM;

    struct MMC3b_map PRG_map[8];
    struct MMC3b_map CHR_map[8];

    u32 prg_ram_size;
    u32 num_CHR_banks;
    u32 num_PRG_banks;

    u32 is_vrc4;
    u32 is_vrc2a;

    mirror_ppu_t ppu_mirror;
    u32 ppu_mirror_mode;

    struct {
        u32 cycle_mode;
        u32 enable;
        u32 enable_after_ack;
        u32 reload;
        u32 prescaler;
        u32 counter;
    } irq;
    struct {
        u32 wram_enabled;
        u32 latch60;
        struct {
            u32 banks_swapped;
        } vrc4;
        struct {
            u32 banks[8];
        } ppu;
        struct {
            u32 bank80;
            u32 banka0;
        } cpu;
    } io;

};

void NES_mapper_VRC2B_4E_4F_init(struct NES_mapper* mapper, struct NES* nes);
void NES_mapper_VRC2B_4E_4F_delete(struct NES_mapper* mapper);

#endif //JSMOOCH_EMUS_VRC_2B_4E_4F_H

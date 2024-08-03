//
// Created by Dave on 2/5/2024.
//

#ifndef JSMOOCH_EMUS_NES_CART_H
#define JSMOOCH_EMUS_NES_CART_H

#include "helpers/int.h"
#include "helpers/buf.h"
#include "mappers/mapper.h"

#ifndef NES_PPU_mirror_modes_def
#define NES_PPU_mirror_modes_def

enum NES_PPU_mirror_modes {
    PPUM_Horizontal,
    PPUM_Vertical,
    PPUM_FourWay,
    PPUM_ScreenAOnly,
    PPUM_ScreenBOnly
};
#endif

struct NES_cart {
    u32 valid;
    struct buf CHR_ROM;
    struct buf PRG_ROM;
    struct NES* nes;

    struct {
        u32 mapper_number;
        u32 nes_timing;
        u32 submapper;
        enum NES_PPU_mirror_modes mirroring;
        u32 battery_present;
        u32 trainer_present;
        u32 four_screen_mode;
        u32 chr_ram_present;
        u32 chr_rom_size;
        u32 prg_rom_size;
        u32 prg_ram_size;
        u32 prg_nvram_size;
        u32 chr_ram_size;
        u32 chr_nvram_size;
    } header;
};

void NES_cart_init(struct NES_cart*, struct NES* nes);
void NES_cart_delete(struct NES_cart*);
u32 NES_cart_load_ROM_from_RAM(struct NES_cart*, char* fil, u64 fil_sz);

#endif //JSMOOCH_EMUS_NES_CART_H

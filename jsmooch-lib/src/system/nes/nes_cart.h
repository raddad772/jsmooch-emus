//
// Created by Dave on 2/5/2024.
//

#pragma once

#include "helpers/int.h"
#include "helpers/buf.h"
#include "mappers/mapper.h"

enum NES_PPU_mirror_modes {
    PPUM_Horizontal=0,
    PPUM_Vertical,
    PPUM_FourWay,
    PPUM_ScreenAOnly,
    PPUM_ScreenBOnly
};

struct NES_cart {
    explicit NES_cart(struct NES *nes) : nes(nes) {};
    u32 read_ines1_header(const char *fil, size_t fil_sz);
    u32 read_ines2_header(const char *fil, size_t fil_sz);
    void read_ROM_RAM(const char* inp, size_t inp_size);
    u32 load_ROM_from_RAM(char* fil, u64 fil_sz);

    u32 valid=0;
    buf CHR_ROM{};
    buf PRG_ROM{};
    NES* nes{};

    struct {
        u32 mapper_number{};
        u32 nes_timing{};
        u32 submapper{};
        NES_PPU_mirror_modes mirroring{};
        u32 battery_present{};
        u32 trainer_present{};
        u32 four_screen_mode{};
        u32 chr_ram_present{};
        u32 chr_rom_size{};
        u32 prg_rom_size{};
        u32 prg_ram_size{};
        u32 prg_nvram_size{};
        u32 chr_ram_size{};
        u32 chr_nvram_size{};
    } header{};

    void serialize(struct serialized_state &state);
    void deserialize(struct serialized_state &state);
};

//
// Created by . on 4/21/25.
//

#pragma once

#include "helpers/int.h"
#include "helpers/buf.h"
#include "helpers/physical_io.h"

struct SNES;

struct SNES_cart {
    explicit SNES_cart(SNES *parent) : snes(parent) {}
    u32 load_ROM_from_RAM(char* fil, u64 fil_sz, physical_io_device &pio);
    SNES *snes;
    buf ROM{};
    struct SNES_cart_header {
        u32 mapping_mode{};
        u32 version{};
        u32 hi_speed{};
        u32 rom_size{}, sram_size{};
        u32 sram_mask{};
        u32 rom_sizebit{}, sram_sizebit{};
        char internal_name[21]{};
        u32 lorom{}, hirom{};
        u32 offset{};
    } header{};
    persistent_store *SRAM{};

private:
    void read_ver1_header();
    void read_ver2_header();
    void read_ver3_header();
    i32 score_header(u32 addr) const;
    u32 find_cart_header();
};

//
// Created by . on 11/13/25.
//

#pragma once

#include "mappers/nes_memmap.h"

struct NES_bus {
    explicit NES_bus(NES *nes);
    ~NES_bus();

    NES_mapper *mapper{};

    void set_cart(physical_io_device &pio);
    void set_which_mapper(u32 wh);
    u32 CPU_read(u32 addr, u32 old_val, u32 has_effect);
    void CPU_write(u32 addr, u32 val);

    NES* nes{};
    void PPU_write(u32 addr, u32 val);
    u32 PPU_read_effect(u32 addr);
    u32 PPU_read_noeffect(u32 addr);

    NES_mappers which{};
    void do_reset();

    void *mapper_ptr{};

    mirror_ppu_t ppu_mirror{};
    u32 ppu_mirror_mode{};
    struct {
        u32 has_sound{};
    } flags{};

    simplebuf8 CIRAM{0x800}; // 0x800 PPU RAM
    simplebuf8 CPU_RAM{0x800}; // 0x800 CPU RAM

    NES_memmap CPU_map[65536 / 0x2000]{};
    NES_memmap PPU_map[0x4000 / 0x400]{};

    persistent_store *SRAM{};
    simplebuf8 fake_PRG_RAM{};
    simplebuf8 PRG_ROM{};
    simplebuf8 CHR_ROM{};
    simplebuf8 CHR_RAM{};

    float NES_audio_bias=1.0f;
    float mapper_audio_bias=0.0f;

    u32 num_PRG_ROM_banks8K{};
    u32 num_PRG_ROM_banks16K{};
    u32 num_PRG_ROM_banks32K{};
    u32 num_CHR_ROM_banks1K{};
    u32 num_CHR_ROM_banks2K{};
    u32 num_CHR_ROM_banks4K{};
    u32 num_CHR_ROM_banks8K{};
    u32 num_CHR_RAM_banks{};
    u32 num_PRG_RAM_banks{};
    u32 has_PPU_RAM{};

    void map_PRG8K(u32 range_start, u32 range_end, simplebuf8 *buf, u32 bank, u32 is_readonly);
    void map_PRG16K(u32 range_start, u32 range_end, simplebuf8 *buf, u32 bank, u32 is_readonly);
    void map_PRG32K(u32 range_start, u32 range_end, simplebuf8 *buf, u32 bank, u32 is_readonly);
    void map_CHR1K(u32 range_start, u32 range_end, simplebuf8 *buf, u32 bank, u32 is_readonly);
    void map_CHR2K(u32 range_start, u32 range_end, simplebuf8 *buf, u32 bank, u32 is_readonly);
    void map_CHR4K(u32 range_start, u32 range_end, simplebuf8 *buf, u32 bank, u32 is_readonly);
    void map_CHR8K(u32 range_start, u32 range_end, simplebuf8 *buf, u32 bank, u32 is_readonly);
    void PPU_mirror_set();

};


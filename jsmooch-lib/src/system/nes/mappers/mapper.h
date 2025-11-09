//
// Created by . on 9/27/24.
//

#ifndef JSMOOCH_EMUS_MAPPER_H
#define JSMOOCH_EMUS_MAPPER_H

#include "helpers/int.h"
#include "helpers/sys_interface.h"
#include "helpers/simplebuf.h"
#include "../nes_cart.h"
#include "helpers/sram.h"
#include "nes_memmap.h"

// NES_bus takes care of mappers!

typedef u32 (*mirror_ppu_t)(u32);

enum NES_mappers {
    NESM_UNKNOWN,
    NESM_NONE,
    NESM_MMC1,
    NESM_UXROM,
    NESM_CNROM,
    NESM_MMC3b,
    NESM_AXROM,
    NESM_VRC4E_4F,
    NESM_DXROM,
    NESM_SUNSOFT_5b,
    NESM_SUNSOFT_7,
    NESM_JF11_JF14,
    NESM_GNROM,
    NESM_COLOR_DREAMS,
    NESM_MMC5
};

struct NES;

struct NES_mapper {
    NES_mapper(NES *nes);
    ~NES_mapper();

    void set_cart(physical_io_device &pio);
    void CPU_cycle();
    void set_which_mapper(u32 wh);
    u32 CPU_read(u32 addr, u32 old_val, u32 has_effect);
    void CPU_write(u32 addr, u32 val);

    void *ptr{};
    NES* nes{};
    void do_a12_watch(u32 addr);
    u32 PPU_read_effect(u32 addr);
    u32 PPU_read_noeffect(u32 addr);

    NES_mappers which{};

    void *mapper_ptr{};

    void (*destruct)(NES_mapper*){};
    void (*writecart)(NES_mapper*, u32 addr, u32 val, u32 *do_write){};
    u32 (*readcart)(NES_mapper*, u32 addr, u32 old_val, u32 has_effect, u32 *do_read){};
    void (*setcart)(NES_mapper*, NES_cart *cart){};
    void (*reset)(NES_mapper*){};
    void (*a12_watch)(NES_mapper*, u32 addr){};
    void (*cpu_cycle)(NES_mapper*){};

    void (*serialize)(NES_mapper*, serialized_state &state){};
    void (*deserialize)(NES_mapper*, serialized_state &state){};

    float (*sample_audio)(NES_mapper*){};

    u32 (*PPU_read_override)(NES_mapper*, u32 addr, u32 has_effect){};
    void (*PPU_write_override)(NES_mapper*, u32 addr, u32 val){};

    mirror_ppu_t ppu_mirror{};
    u32 ppu_mirror_mode{};
    struct {
        u32 has_sound{};
    } flags{};

    simplebuf8 CIRAM{}; // 0x800 PPU RAM
    simplebuf8 CPU_RAM{}; // 0x800 CPU RAM

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
};


void NES_bus_map_PRG8K(NES_mapper*, u32 range_start, u32 range_end, simplebuf8 *buf, u32 bank, u32 is_readonly);
void NES_bus_map_PRG16K(NES_mapper*, u32 range_start, u32 range_end, simplebuf8 *buf, u32 bank, u32 is_readonly);
void NES_bus_map_PRG32K(NES_mapper*, u32 range_start, u32 range_end, simplebuf8 *buf, u32 bank, u32 is_readonly);
void NES_bus_map_CHR1K(NES_mapper*, u32 range_start, u32 range_end, simplebuf8 *buf, u32 bank, u32 is_readonly);
void NES_bus_map_CHR2K(NES_mapper*, u32 range_start, u32 range_end, simplebuf8 *buf, u32 bank, u32 is_readonly);
void NES_bus_map_CHR4K(NES_mapper*, u32 range_start, u32 range_end, simplebuf8 *buf, u32 bank, u32 is_readonly);
void NES_bus_map_CHR8K(NES_mapper*, u32 range_start, u32 range_end, simplebuf8 *buf, u32 bank, u32 is_readonly);
void NES_bus_PPU_mirror_set(NES_mapper*);


#endif //JSMOOCH_EMUS_MAPPER_H

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

struct NES;

struct NES_bus {
    void *ptr{};
    NES* nes{};

    NES_mappers which;

    void *mapper_ptr{};

    void (*destruct)(struct NES_bus*);
    void (*writecart)(struct NES_bus*, u32 addr, u32 val, u32 *do_write);
    u32 (*readcart)(struct NES_bus*, u32 addr, u32 old_val, u32 has_effect, u32 *do_read);
    void (*setcart)(struct NES_bus*, struct NES_cart *cart);
    void (*reset)(struct NES_bus*);
    void (*a12_watch)(struct NES_bus*, u32 addr);
    void (*cpu_cycle)(struct NES_bus*);

    void (*serialize)(struct NES_bus*, struct serialized_state *state);
    void (*deserialize)(struct NES_bus*, struct serialized_state *state);

    float (*sample_audio)(struct NES_bus*);

    u32 (*PPU_read_override)(struct NES_bus*, u32 addr, u32 has_effect);
    void (*PPU_write_override)(struct NES_bus*, u32 addr, u32 val);

    mirror_ppu_t ppu_mirror;
    u32 ppu_mirror_mode;
    struct {
        u32 has_sound;
    } flags;

    simplebuf8 CIRAM{}; // 0x800 PPU RAM
    simplebuf8 CPU_RAM{}; // 0x800 CPU RAM

    NES_memmap CPU_map[65536 / 0x2000];
    NES_memmap PPU_map[0x4000 / 0x400];

    persistent_store *SRAM{};
    simplebuf8 fake_PRG_RAM{};
    simplebuf8 PRG_ROM{};
    simplebuf8 CHR_ROM{};
    simplebuf8 CHR_RAM{};

    float NES_audio_bias;
    float mapper_audio_bias;

    u32 num_PRG_ROM_banks8K;
    u32 num_PRG_ROM_banks16K;
    u32 num_PRG_ROM_banks32K;
    u32 num_CHR_ROM_banks1K;
    u32 num_CHR_ROM_banks2K;
    u32 num_CHR_ROM_banks4K;
    u32 num_CHR_ROM_banks8K;
    u32 num_CHR_RAM_banks;
    u32 num_PRG_RAM_banks;
    u32 has_PPU_RAM;
};

void NES_bus_init(struct NES_bus*, struct NES* nes);
void NES_bus_delete(struct NES_bus*);
void NES_bus_set_which_mapper(struct NES_bus*, u32 wh);
void NES_bus_CPU_cycle(struct NES*);
u32 NES_bus_CPU_read(struct NES*, u32 addr, u32 old_val, u32 has_effect);
void NES_bus_CPU_write(struct NES*, u32 addr, u32 val);
u32 NES_PPU_read_effect(struct NES*, u32 addr);
u32 NES_PPU_read_noeffect(struct NES*, u32 addr);
void NES_PPU_write(struct NES*, u32 addr, u32 val);
void NES_bus_reset(struct NES*);
struct physical_io_device;
void NES_bus_set_cart(struct NES*, struct NES_cart* cart, struct physical_io_device *pio);
void NES_bus_a12_watch(struct NES*, u32 addr);


void NES_bus_map_PRG8K(struct NES_bus*, u32 range_start, u32 range_end, struct simplebuf8 *buf, u32 bank, u32 is_readonly);
void NES_bus_map_PRG16K(struct NES_bus*, u32 range_start, u32 range_end, struct simplebuf8 *buf, u32 bank, u32 is_readonly);
void NES_bus_map_PRG32K(struct NES_bus*, u32 range_start, u32 range_end, struct simplebuf8 *buf, u32 bank, u32 is_readonly);
void NES_bus_map_CHR1K(struct NES_bus*, u32 range_start, u32 range_end, struct simplebuf8 *buf, u32 bank, u32 is_readonly);
void NES_bus_map_CHR2K(struct NES_bus*, u32 range_start, u32 range_end, struct simplebuf8 *buf, u32 bank, u32 is_readonly);
void NES_bus_map_CHR4K(struct NES_bus*, u32 range_start, u32 range_end, struct simplebuf8 *buf, u32 bank, u32 is_readonly);
void NES_bus_map_CHR8K(struct NES_bus*, u32 range_start, u32 range_end, struct simplebuf8 *buf, u32 bank, u32 is_readonly);
void NES_bus_PPU_mirror_set(struct NES_bus*);


#endif //JSMOOCH_EMUS_MAPPER_H

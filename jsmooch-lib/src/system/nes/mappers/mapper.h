//
// Created by Dave on 2/6/2024.
//

#ifndef JSMOOCH_EMUS_MAPPER_H
#define JSMOOCH_EMUS_MAPPER_H

#include "helpers/int.h"
#include "helpers/sys_interface.h"
#include "../nes_cart.h"

enum NES_mappers {
        NESM_NONE,
        NESM_MMC1,
        NESM_UXROM,
        NESM_CNROM,
        NESM_MMC3b,
        NESM_AXROM,
        NESM_VRC4E_4F,
        NESM_DXROM
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

struct NES_mapper {
    void *ptr;
    struct NES* nes;

    enum NES_mappers which;

    u32 (*CPU_read)(struct NES*, u32, u32, u32);
    void (*CPU_write)(struct NES*, u32, u32);
    u32 (*PPU_read_effect)(struct NES*, u32);
    u32 (*PPU_read_noeffect)(struct NES*, u32);
    void (*PPU_write)(struct NES*, u32, u32);
    void (*reset)(struct NES*);
    void (*set_cart)(struct NES*, struct NES_cart* cart);
    void (*a12_watch)(struct NES*, u32);
    void (*cycle)(struct NES*);
};

void NES_mapper_init(struct NES_mapper* this, struct NES* nes);
void NES_mapper_delete(struct NES_mapper* this);
void NES_mapper_set_which(struct NES_mapper* this, u32 wh);
//struct NES_mapper* new_NES_mapper(struct NES* nes, enum NES_mappers which);
//void delete_NES_mapper(struct NES_mapper* whom);

typedef u32 (*mirror_ppu_t)(u32);
u32 NES_mirror_ppu_Bonly(u32 addr);
u32 NES_mirror_ppu_Aonly(u32 addr);
u32 NES_mirror_ppu_horizontal(u32 addr);
u32 NES_mirror_ppu_vertical(u32 addr);
u32 NES_mirror_ppu_four(u32 addr);

#endif //JSMOOCH_EMUS_MAPPER_H

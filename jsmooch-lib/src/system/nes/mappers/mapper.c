//
// Created by Dave on 2/6/2024.
//

#include "malloc.h"
#include "stdio.h"
#include "assert.h"

#include "mapper.h"

#include "no_mapper.h"
#include "mmc3b.h"

void NES_mapper_init(struct NES_mapper* this, struct NES* nes)
{
    this->ptr = NULL;
    this->which = -1;
    this->nes = nes;
}

void NES_mapper_set_which(struct NES_mapper* this, u32 wh) {
    if (this->ptr != NULL) {
        NES_mapper_delete(this);
    }
    enum NES_mappers which;
    switch(wh) {
        case 0: // no mapper
            which = NESM_NONE;
            break;
        case 1: // MMC1
            which = NESM_MMC1;
            break;
        case 2: // UxROM
            which = NESM_UXROM;
            break;
        case 3: // CxROM
            which = NESM_CXROM;
            break;
        case 4: // MMC3
            which = NESM_MMC3b;
            break;
        case 7: // AXROM
            which = NESM_AXROM;
            break;
        case 23: // VRC4
            which = NESM_VRC4E_4F;
            break;
        case 206: // DXROM
            which = NESM_DXROM;
            break;
        default:
            printf("Unknown mapper number dawg! %d", which);
            return;
    }

    switch (which) {
        case NESM_NONE: // No mapper!
            NES_mapper_none_init(this, this->nes);
            printf("\nNO MAPPER!");
            break;
        case NESM_MMC3b:
            NES_mapper_MMC3b_init(this, this->nes);
            printf("\nMBC1");
            break;
            /*
        case MBC2:
            NES_mapper_MBC2_new(mapper, nes);
            printf("\nMBC2");
            break;
        case MBC3:
            NES_mapper_MBC3_new(mapper, nes);
            printf("\nMBC3");
            break;
        case MBC5:
            NES_mapper_MBC5_new(mapper, nes);
            printf("\nMBC5");
            break;*/
        default:
            printf("\nNO SUPPORTED MAPPER! %d", which);
            break;
    }
    fflush(stdout);
}

void NES_mapper_delete(struct NES_mapper* this)
{
    if (this->ptr == NULL) return;
    switch (this->which) {
        case NESM_NONE: // No-mapper!
            NES_mapper_none_delete(this);
            break;
        case NESM_MMC3b: // No-mapper!
            NES_mapper_MMC3b_delete(this);
            break;
        default:
            assert(1==0);
            break;
    }
}

u32 NES_mirror_ppu_four(u32 addr) {
    return addr & 0xFFF;
}

u32 NES_mirror_ppu_vertical(u32 addr) {
    return (addr & 0x0400) | (addr & 0x03FF);
}

u32 NES_mirror_ppu_horizontal(u32 addr) {
    return ((addr >> 1) & 0x0400) | (addr & 0x03FF);
}

u32 NES_mirror_ppu_Aonly(u32 addr) {
    return addr & 0x3FF;
}

u32  NES_mirror_ppu_Bonly(u32 addr) {
    return 0x400 | (addr & 0x3FF);
}

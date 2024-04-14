#ifndef _GB_ENUMS_H
#define _GB_ENUMS_H

enum GB_variants {
    DMG,
    GBC,
    SGB
};


enum GB_PPU_modes {
    HBLANK,
    VBLANK,
    OAM_search,
    pixel_transfer
};

enum GB_mappers {
    NONE,
    MBC1,
    MBC2,
    MMM01,
    MBC3,
    MBC5,
    MBC6,
    MBC7,
    POCKET_CAMERA,
    BANDAI_TAMA5,
    HUC3,
    HUC1
};


#endif
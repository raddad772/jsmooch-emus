#pragma once

namespace GB {

enum variants {
    DMG,
    GBC,
    SGB
};

namespace PPU {
    enum modes {
        HBLANK,
        VBLANK,
        OAM_search,
        pixel_transfer
    };
}

enum mappers {
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


}
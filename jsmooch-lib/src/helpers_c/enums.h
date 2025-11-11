#ifndef _JSMOOCH_ENUMS_H
#define _JSMOOCH_ENUMS_H

enum jsm::systems {
	jsm::systems::DMG,
	jsm::systems::GBC,
	jsm::systems::NES,
	jsm::systems::SNES,
    jsm::systems::SG1000,
	jsm::systems::SMS1,
    jsm::systems::SMS2,
	jsm::systems::GG,
    jsm::systems::DREAMCAST,
	jsm::systems::ZX_SPECTRUM_48K,
    jsm::systems::ZX_SPECTRUM_128K,
	jsm::systems::BBC_MICRO,
	jsm::systems::GENESIS_USA,
    jsm::systems::MEGADRIVE_PAL,
    jsm::systems::GENESIS_JAP,
	jsm::systems::PS1,
    jsm::systems::ATARI2600,
    jsm::systems::MAC128K,
    jsm::systems::MAC512K,
    jsm::systems::MACPLUS_1MB,
    jsm::systems::APPLEIIe,
    jsm::systems::GBA,
    jsm::systems::NDS,
    jsm::systems::GALAKSIJA,
    jsm::systems::TURBOGRAFX16
};

enum jsm_regions {
    REGION_USA,
    REGION_JAPAN,
    REGION_EUROPE
};

enum jsm_display_standards {
    NTSC,
    PAL,
    NSTCJ
};
#endif
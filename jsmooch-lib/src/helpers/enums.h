#ifndef _JSMOOCH_ENUMS_H
#define _JSMOOCH_ENUMS_H

enum jsm_systems {
	SYS_DMG,
	SYS_GBC,
	SYS_NES,
	SYS_SNES,
	SYS_SMS1,
    SYS_SMS2,
	SYS_GG,
    SYS_DREAMCAST,
	SYS_ZX_SPECTRUM,
	SYS_BBC_MICRO,
	SYS_GENESIS,
	SYS_PSX,
    SYS_ATARI2600
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
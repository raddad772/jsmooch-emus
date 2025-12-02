#include <cstdio>
#include <cstring>
#include <cstdlib>

#include "sys_interface.h"
//#include "system/gb/gb.h"
#include "system/nes/nes.h"
//#include "system/nds/nds.h"
#include "system/cosmac_vip/cosmac_vip.h"
#include "system/snes/snes.h"
#include "system/sms_gg/sms_gg.h"
#include "system/commodore64/commodore64.h"
//#include "system/dreamcast/dreamcast.h"
//#include "system/ps1/ps1.h"
//#include "system/atari2600/atari2600.h"
//#include "system/zxspectrum/zxspectrum.h"
#include "system/genesis/genesis.h"
//#include "system/apple2/apple2.h"
//#include "system/mac/mac.h"
#include "helpers/debug.h"
#include "system/gba/gba.h"
#include "system/galaksija/galaksija.h"
#include "system/tg16/tg16.h"

jsm_system* new_system(jsm::systems which)
{
    dbg_init();
    jsm_system* out = nullptr;
    switch (which) {
        /*case jsm::systems::GBA:
            GBA_new(out);
            break;
        case jsm::systems::NDS:
            NDS_new(out);
            break;*/
        case jsm::systems::COMMODORE64:
            out = Commodore64_new(which);
            break;
        case jsm::systems::SNES:
            out = SNES_new();
            break;
        /*case jsm::systems::TURBOGRAFX16:
            TG16_new(out, which);
            break;*/
        case jsm::systems::GENESIS_JAP:
        case jsm::systems::GENESIS_USA:
        case jsm::systems::MEGADRIVE_PAL:
            out = genesis_new(which);
            break;
        /*case jsm::systems::ATARI2600:
            atari2600_new(out);
            break;
		case jsm::systems::DMG:
			GB_new(out, DMG);
			break;
        case jsm::systems::GBC:
            GB_new(out, GBC);
            break;
        case jsm::systems::MAC128K:
            mac_new(out, mac128k);
            break;
        case jsm::systems::MAC512K:
            mac_new(out, mac512k);
            break;
        case jsm::systems::MACPLUS_1MB:
            mac_new(out, macplus_1mb);
            break;*/
        case jsm::systems::COSMAC_VIP_2k:
        case jsm::systems::COSMAC_VIP_4k:
            out = VIP_new(which);
            break;
        case jsm::systems::NES:
            out = NES_new();
            break;
        /*case jsm::systems::PS1:
            PS1_new(out);
            break;*/
        case jsm::systems::SG1000:
        case jsm::systems::SMS1:
        case jsm::systems::SMS2:
        case jsm::systems::GG:
            out = SMSGG_new(which, jsm::regions::USA);
            break;
        /*case jsm::systems::DREAMCAST:
            DC_new(out);
            break;
        case jsm::systems::APPLEIIe:
            apple2_new(out);
            break;
        case jsm::systems::ZX_SPECTRUM_48K:
            ZXSpectrum_new(out, ZXS_spectrum48);
            break;
        case jsm::systems::GALAKSIJA:
            galaksija_new(out);
            break;
        case jsm::systems::ZX_SPECTRUM_128K:
            ZXSpectrum_new(out, ZXS_spectrum128);
            break;*/
        default:
            printf("CREATE UNKNOWN SYSTEM!");
            break;
	}
    out->kind = which;
    //out->IOs->reserve(20);
    //out->opts->reserve(5);
    out->describe_io();
    return out;
}

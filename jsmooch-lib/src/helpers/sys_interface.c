#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "sys_interface.h"
#include "system/gb/gb.h"
#include "system/nes/nes.h"
#include "system/nds/nds.h"
#include "system/snes/snes.h"
#include "system/sms_gg/sms_gg.h"
#include "system/dreamcast/dreamcast.h"
#include "system/ps1/ps1.h"
#include "system/atari2600/atari2600.h"
#include "system/zxspectrum/zxspectrum.h"
#include "system/genesis/genesis.h"
#include "system/apple2/apple2.h"
#include "system/mac/mac.h"
#include "helpers/debug.h"
#include "helpers/debugger/debugger.h"
#include "system/gba/gba.h"
#include "system/galaksija/galaksija.h"

struct jsm_system* new_system(enum jsm_systems which)
{
    dbg_init();
    struct jsm_system* out = malloc(sizeof(struct jsm_system));
    memset(out, 0, sizeof(struct jsm_system));
    cvec_init(&out->IOs, sizeof(struct physical_io_device), 20);
    cvec_init(&out->opts, sizeof(struct debugger_widget), 5);
    cvec_lock_reallocs(&out->IOs);
    out->kind = which;
	switch (which) {
        case SYS_GBA:
            GBA_new(out);
            break;
        case SYS_NDS:
            NDS_new(out);
            break;
        case SYS_SNES:
            SNES_new(out);
            break;
        case SYS_GENESIS_JAP:
        case SYS_GENESIS_USA:
        case SYS_MEGADRIVE_PAL:
            genesis_new(out, which);
            break;
        case SYS_ATARI2600:
            atari2600_new(out);
            break;
		case SYS_DMG:
			GB_new(out, DMG);
			break;
        case SYS_GBC:
            GB_new(out, GBC);
            break;
        case SYS_MAC128K:
            mac_new(out, mac128k);
            break;
        case SYS_MAC512K:
            mac_new(out, mac512k);
            break;
        case SYS_MACPLUS_1MB:
            mac_new(out, macplus_1mb);
            break;
        case SYS_NES:
            NES_new(out);
            break;
        case SYS_PS1:
            PS1_new(out);
            break;
        case SYS_SG1000:
        case SYS_SMS1:
        case SYS_SMS2:
        case SYS_GG:
            SMSGG_new(out, which, REGION_USA);
            break;
        case SYS_DREAMCAST:
            DC_new(out);
            break;
        case SYS_APPLEIIe:
            apple2_new(out);
            break;
        case SYS_ZX_SPECTRUM_48K:
            ZXSpectrum_new(out, ZXS_spectrum48);
            break;
        case SYS_GALAKSIJA:
            galaksija_new(out);
            break;
        case SYS_ZX_SPECTRUM_128K:
            ZXSpectrum_new(out, ZXS_spectrum128);
            break;
        default:
            printf("CREATE UNKNOWN SYSTEM!");
            break;
	}
    out->describe_io(out, &out->IOs);
    return out;
}

void jsm_clearfuncs(struct jsm_system *jsm)
{
    jsm->finish_frame = NULL;
    jsm->finish_scanline = NULL;
    jsm->step_master = NULL;
    jsm->reset = NULL;
    jsm->load_BIOS = NULL;
    jsm->get_framevars = NULL;
    jsm->play = NULL;
    jsm->pause = NULL;
    jsm->stop = NULL;
    jsm->describe_io = NULL;
    jsm->setup_debugger_interface = NULL;
    jsm->set_audiobuf = NULL;
}

void jsm_delete(struct jsm_system* jsm)
{
    struct cvec* o = &jsm->IOs;
    switch(jsm->kind) {
        case SYS_GBA:
            GBA_delete(jsm);
            break;
        case SYS_NDS:
            NDS_delete(jsm);
            break;
        case SYS_SNES:
            SNES_delete(jsm);
            break;
        case SYS_GENESIS_USA:
        case SYS_GENESIS_JAP:
        case SYS_MEGADRIVE_PAL:
            genesis_delete(jsm);
            break;
        case SYS_ATARI2600:
            atari2600_delete(jsm);
            break;
        case SYS_DMG:
        case SYS_GBC:
            GB_delete(jsm);
            break;
        case SYS_MAC512K:
        case SYS_MAC128K:
        case SYS_MACPLUS_1MB:
            mac_delete(jsm);
            break;
        case SYS_NES:
            NES_delete(jsm);
            break;
        case SYS_PS1:
            PS1_delete(jsm);
            break;
        case SYS_SG1000:
        case SYS_SMS1:
        case SYS_SMS2:
        case SYS_GG:
            SMSGG_delete(jsm);
            break;
        case SYS_DREAMCAST:
            DC_delete(jsm);
            break;
        case SYS_APPLEIIe:
            apple2_delete(jsm);
            break;
        case SYS_ZX_SPECTRUM_48K:
        case SYS_ZX_SPECTRUM_128K:
            ZXSpectrum_delete(jsm);
            break;
        case SYS_GALAKSIJA:
            galaksija_delete(jsm);
            break;
        default:
            printf("DELETE UNKNOWN SYSTEM!");
            break;
    }
    cvec_delete(o);
    cvec_delete(&jsm->opts);
    dbg_delete();
    free(jsm);
}


#include "stdlib.h"

#include "sys_interface.h"
#include "system/gb/gb.h"
//#include "system/gb/gb_enums.h"
#include "system/nes/nes.h"
#include "system/sms_gg/sms_gg.h"
#include "system/dreamcast/dreamcast.h"
#include "system/atari2600/atari2600.h"
#include "helpers/debug.h"
#include "stdio.h"

struct jsm_system* new_system(enum jsm_systems which)
{
    dbg_init();
    struct jsm_system* out = malloc(sizeof(struct jsm_system));
    cvec_init(&out->IOs, sizeof(struct physical_io_device), 20);
    cvec_lock_reallocs(&out->IOs);
    out->kind = which;
	switch (which) {
        case SYS_ATARI2600:
            atari2600_new(out);
            break;
		case SYS_DMG:
			GB_new(out, DMG);
			break;
        case SYS_GBC:
            GB_new(out, GBC);
            break;
        case SYS_NES:
            NES_new(out);
            break;
        case SYS_SMS1:
        case SYS_SMS2:
        case SYS_GG:
            SMSGG_new(out, which, REGION_USA);
            break;
        case SYS_DREAMCAST:
            DC_new(out);
            break;
        default:
            printf("CREATE UNKNOWN SYSTEM!");
            break;
	}
    out->describe_io(out, &out->IOs);
    return out;
}

void jsm_delete(struct jsm_system* jsm)
{
    struct cvec* o = &jsm->IOs;
    switch(jsm->kind) {
        case SYS_ATARI2600:
            atari2600_delete(jsm);
            break;
        case SYS_DMG:
        case SYS_GBC:
            GB_delete(jsm);
            break;
        case SYS_NES:
            NES_delete(jsm);
            break;
        case SYS_SMS1:
        case SYS_SMS2:
        case SYS_GG:
            SMSGG_delete(jsm);
            break;
        case SYS_DREAMCAST:
            DC_delete(jsm);
            break;
        default:
            printf("DELETE UNKNOWN SYSTEM!");
            break;
    }
    cvec_delete(o);
    dbg_delete();
    free(jsm);
}


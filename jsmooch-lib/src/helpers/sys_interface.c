#include <malloc.h>

#include "sys_interface.h"
#include "system/gb/gb.h"
#include "system/gb/gb_enums.h"
#include "helpers/debug.h"
#include "stdio.h"

struct jsm_system* new_system(enum jsm_systems which, struct JSM_IOmap *iomap)
{
    dbg_init();
    struct jsm_system* out = malloc(sizeof(struct jsm_system));
    out->which = which;
	switch (which) {
		case SYS_DMG:
			GB_new(out, DMG, iomap);
			break;
        default:
            printf("DELETE UNKNOWN SYSTEM!");
            break;
	}
    return out;
}

void jsm_delete(struct jsm_system* jsm)
{
    switch(jsm->which) {
        case SYS_DMG:
            GB_delete(jsm);
            break;
        default:
            printf("DELETE UNKNOWN SYSTEM!");
            break;
    }
    dbg_delete();
    free(jsm);
}


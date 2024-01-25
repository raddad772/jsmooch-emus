#include <malloc.h>

#include "sys_interface.h"
#include "system/gb/gb.h"
#include "system/gb/gb_enums.h"


struct jsm_system* new_system(enum jsm_systems which, struct JSM_IOmap *iomap)
{
	struct jsm_system* out = malloc(sizeof(struct jsm_system));
    out->which = which;
	switch (which) {
		case SYS_DMG:
			GB_new(out, DMG, iomap);
			break;

	}
}

void delete_system(struct jsm_system* which)
{

}


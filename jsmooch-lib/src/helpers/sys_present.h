#ifndef JSMOOCH_EMUS_SYS_PRESENT_H
#define JSMOOCH_EMUS_SYS_PRESENT_H

#include "helpers/int.h"
#include "helpers/sys_interface.h"
#include "helpers/enums.h"

void jsm_present(enum jsm_systems which, u32 last_used_buffer, struct JSM_IOmap *iom, void *out_buf, u32 out_width, u32 out_height);
//jsm_present(sys->which, fv.last_used_buffer, &iom, window_surface->pixels, 640, 480);

#endif //JSMOOCH_EMUS_SYS_PRESENT_H

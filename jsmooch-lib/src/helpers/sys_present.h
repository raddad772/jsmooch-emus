#ifndef JSMOOCH_EMUS_SYS_PRESENT_H
#define JSMOOCH_EMUS_SYS_PRESENT_H

#ifdef __cplusplus
extern "C" {
#endif

#include "physical_io.h"
#include "helpers/int.h"
#include "helpers/sys_interface.h"
#include "helpers/enums.h"

void jsm_present(enum jsm_systems which, struct physical_io_device *display, void *out_buf, u32 out_width, u32 out_height);
//jsm_present(sys->kind, fv.last_used_buffer, &iom, window_surface->pixels, 640, 480);

#ifdef __cplusplus
}
#endif

#endif //JSMOOCH_EMUS_SYS_PRESENT_H

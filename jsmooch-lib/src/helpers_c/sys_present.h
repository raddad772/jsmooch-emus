#ifndef JSMOOCH_EMUS_jsm::systems::PRESENT_H
#define JSMOOCH_EMUS_jsm::systems::PRESENT_H

#ifdef __cplusplus
extern "C" {
#endif

#include "physical_io.h"
#include "helpers/int.h"
#include "helpers/sys_interface.h"
#include "helpers/enums.h"

struct events_view;
void jsm_present(enum jsm::systems which, struct physical_io_device *display, void *out_buf, u32 x_offset, u32 y_offset, u32 out_width, u32 out_height, struct events_view *ev);
void mac512k_present(struct physical_io_device *device, void *out_buf, u32 out_width, u32 out_height);
void apple2_present(struct physical_io_device *device, void *out_buf, u32 x_offset, u32 y_offset, u32 out_width, u32 out_height);
void zx_spectrum_present(struct physical_io_device *device, void *out_buf, u32 out_width, u32 out_height);
void atari2600_present(struct physical_io_device *device, void *out_buf, u32 out_width, u32 out_height);
void DMG_present(struct physical_io_device *device, void *out_buf, u32 x_offset, u32 y_offset, u32 out_width, u32 out_height, bool is_event_view_present);
void GBC_present(struct physical_io_device *device, void *out_buf, u32 x_offset, u32 y_offset, u32 out_width, u32 out_height, bool is_event_view_present);
void NES_present(struct physical_io_device *device, void *out_buf, u32 x_offset, u32 y_offset, u32 out_width, u32 out_height);
void genesis_present(struct physical_io_device *device, void *out_buf, u32 out_width, u32 out_height, u32 is_event_view_present);
void SMS_present(struct physical_io_device *device, void *out_buf, u32 x_offset, u32 y_offset, u32 out_width, u32 out_height);
void GG_present(struct physical_io_device *device, void *out_buf, u32 x_offset, u32 y_offset, u32 out_width, u32 out_height);

#ifdef __cplusplus
}
#endif

#endif //JSMOOCH_EMUS_jsm::systems::PRESENT_H

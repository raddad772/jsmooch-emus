#pragma once

#include "physical_io.h"
#include "helpers/int.h"
#include "helpers/sys_interface.h"
#include "helpers/enums.h"

struct events_view;
void jsm_present(jsm::systems which, physical_io_device &display, void *out_buf, u32 x_offset, u32 y_offset, u32 out_width, u32 out_height, events_view *ev);
void mac512k_present(physical_io_device *device, void *out_buf, u32 out_width, u32 out_height);
void apple2_present(physical_io_device *device, void *out_buf, u32 x_offset, u32 y_offset, u32 out_width, u32 out_height);
void zx_spectrum_present(physical_io_device *device, void *out_buf, u32 out_width, u32 out_height);
void atari2600_present(physical_io_device *device, void *out_buf, u32 out_width, u32 out_height);
void DMG_present(physical_io_device *device, void *out_buf, u32 x_offset, u32 y_offset, u32 out_width, u32 out_height, bool is_event_view_present);
void GBC_present(physical_io_device *device, void *out_buf, u32 x_offset, u32 y_offset, u32 out_width, u32 out_height, bool is_event_view_present);
void NES_present(physical_io_device *device, void *out_buf, u32 x_offset, u32 y_offset, u32 out_width, u32 out_height);
void genesis_present(physical_io_device *device, void *out_buf, u32 out_width, u32 out_height, u32 is_event_view_present);
void SMS_present(physical_io_device *device, void *out_buf, u32 x_offset, u32 y_offset, u32 out_width, u32 out_height);
void GG_present(physical_io_device *device, void *out_buf, u32 x_offset, u32 y_offset, u32 out_width, u32 out_height);

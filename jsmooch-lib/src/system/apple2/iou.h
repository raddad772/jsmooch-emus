//
// Created by . on 8/30/24.
//

#ifndef JSMOOCH_EMUS_IOU_H
#define JSMOOCH_EMUS_IOU_H

#include "apple2_internal.h"

void apple2_IOU_cycle(struct apple2* this);
void apple2_IOU_reset(struct apple2* this);
void apple2_IOU_update_switches(struct apple2* this);
void apple2_IOU_access_c0xx(struct apple2* this, u32 addr, u32 has_effect, u32 is_write, u32 *r, u32 *MSB);
void apple2_setup_keyboard(struct apple2* this);
u8 apple2_read_keyboard(struct apple2* this);

#endif //JSMOOCH_EMUS_IOU_H

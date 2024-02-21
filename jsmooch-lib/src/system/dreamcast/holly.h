//
// Created by Dave on 2/16/2024.
//

#ifndef JSMOOCH_EMUS_HOLLY_H
#define JSMOOCH_EMUS_HOLLY_H

#include "dreamcast.h"

void holly_write(struct DC* this, u32 addr, u32 val);
u32 holly_read(struct DC* this, u32 addr);
void holly_reset(struct DC* this);
void DC_recalc_frame_timing(struct DC* this);
void holly_vblank_in(struct DC* this);
void holly_vblank_out(struct DC* this);


#endif //JSMOOCH_EMUS_HOLLY_H

//
// Created by David Schneider on 3/2/24.
//

#ifndef JSMOOCH_EMUS_GDROM_H
#define JSMOOCH_EMUS_GDROM_H

#include "helpers/int.h"
#include "dreamcast.h"
#include "dc_mem.h"

void GDROM_write(struct DC* this, u32 reg, u32 val, u32 bits);
u32 GDROM_read(struct DC* this, u32 reg, u32 bits);


#endif //JSMOOCH_EMUS_GDROM_H

//
// Created by . on 8/30/24.
//

#ifndef JSMOOCH_EMUS_MMU_H
#define JSMOOCH_EMUS_MMU_H

#include "apple2_internal.h"

void apple2_MMU_reset(struct apple2* this);
void apple2_cpu_bus_write(struct apple2* this, u32 addr, u8 val);
u8 apple2_cpu_bus_read(struct apple2* this, u32 addr, u8 old_val, u32 has_effect);


#endif //JSMOOCH_EMUS_MMU_H

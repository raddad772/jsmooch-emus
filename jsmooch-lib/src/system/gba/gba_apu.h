//
// Created by . on 12/28/24.
//

#ifndef JSMOOCH_EMUS_GBA_APU_H
#define JSMOOCH_EMUS_GBA_APU_H

#include "helpers/int.h"

struct GBA;
u32 GBA_APU_read_IO(struct GBA*, u32 addr, u32 sz, u32 access, u32 has_effect);
void GBA_APU_write_IO(struct GBA*, u32 addr, u32 sz, u32 access, u32 val);


#endif //JSMOOCH_EMUS_GBA_APU_H

//
// Created by . on 2/20/25.
//

#ifndef JSMOOCH_EMUS_PS1_SPU_H
#define JSMOOCH_EMUS_PS1_SPU_H

#include "helpers/int.h"

struct PS1_SPU {
    u32 val[0x400];
};

struct PS1;
void PS1_SPU_init(struct PS1 *);

void PS1_SPU_write(struct PS1_SPU *, u32 addr, u32 sz, u32 val);
u32 PS1_SPU_read(struct PS1_SPU *, u32 addr, u32 sz, u32 has_effect);

#endif //JSMOOCH_EMUS_PS1_SPU_H

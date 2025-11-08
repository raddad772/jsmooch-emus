//
// Created by . on 2/15/25.
//

#ifndef JSMOOCH_EMUS_PS1_DMA_H
#define JSMOOCH_EMUS_PS1_DMA_H

#include "helpers/int.h"

struct PS1;

u32 PS1_DMA_read(struct PS1 *, u32 addr, u32 sz);
void PS1_DMA_write(struct PS1 *, u32 addr, u32 sz, u32 val);


#endif //JSMOOCH_EMUS_PS1_DMA_H

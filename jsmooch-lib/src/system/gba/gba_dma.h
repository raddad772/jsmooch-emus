//
// Created by . on 5/5/25.
//

#ifndef JSMOOCH_EMUS_GBA_DMA_H
#define JSMOOCH_EMUS_GBA_DMA_H

#include "helpers/int.h"

struct GBA_DMA_ch;
struct GBA;

void GBA_dma_start(struct GBA_DMA_ch *ch, u32 i, u32 is_sound);
u32 GBA_dma_go(struct GBA *this);

#endif //JSMOOCH_EMUS_GBA_DMA_H

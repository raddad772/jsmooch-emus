//
// Created by . on 5/5/25.
//

#ifndef JSMOOCH_EMUS_GBA_DMA_H
#define JSMOOCH_EMUS_GBA_DMA_H

#include "helpers/int.h"

struct GBA_DMA_ch;
struct GBA;


void GBA_DMA_start(struct GBA *, struct GBA_DMA_ch *ch);
void GBA_DMA_cnt_written(struct GBA *, struct GBA_DMA_ch *ch, u32 old_enable);
void GBA_DMA_init(struct GBA *this);
u32 GBA_DMA_go(struct GBA *);
void GBA_DMA_on_modify_write(struct GBA_DMA_ch *ch);

#endif //JSMOOCH_EMUS_GBA_DMA_H

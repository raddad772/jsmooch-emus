//
// Created by . on 1/20/25.
//

#ifndef JSMOOCH_EMUS_NDS_DMA_H
#define JSMOOCH_EMUS_NDS_DMA_H

#include "helpers/int.h"

struct NDS_DMA_ch;
struct NDS;
void NDS_dma7_start(struct NDS *, struct NDS_DMA_ch *ch, u32 i);
u32 NDS_dma7_go(struct NDS *this);

void NDS_dma9_start(struct NDS *, struct NDS_DMA_ch *ch, u32 i);
u32 NDS_dma9_go(struct NDS *this);

void NDS_check_dma9_at_hblank(struct NDS *);
void NDS_check_dma9_at_vblank(struct NDS *);
void NDS_check_dma7_at_vblank(struct NDS *);
void NDS_trigger_dma7_if(struct NDS *, u32 start_timing);
void NDS_trigger_dma9_if(struct NDS *, u32 start_timing);

#endif //JSMOOCH_EMUS_NDS_DMA_H

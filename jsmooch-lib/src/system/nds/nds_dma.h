//
// Created by . on 1/20/25.
//

#ifndef JSMOOCH_EMUS_NDS_DMA_H
#define JSMOOCH_EMUS_NDS_DMA_H

#include "helpers/int.h"

/*
0  Start Immediately
  1  Start at V-Blank
  2  Start at H-Blank (paused during V-Blank)
  3  Synchronize to start of display
  4  Main memory display
  5  DS Cartridge Slot
  6  GBA Cartridge Slot
  7  Geometry Command FIFO */

enum NDS_DMA_start_timings {
    NDS_DMA_IMMEDIATE=0,
    NDS_DMA_VBLANK,
    NDS_DMA_HBLANK,
    NDS_DMA_START_OF_DISPLAY,
    NDS_DMA_MAIN_MEMORY_DISPLAY,
    NDS_DMA_DS_CART_SLOT,
    NDS_DMA_GBA_CART_SLOT,
    NDS_DMA_GE_FIFO
};

struct NDS_DMA_ch;
struct NDS;
void NDS_dma7_start(struct NDS *, struct NDS_DMA_ch *ch, u32 i);
u32 NDS_dma7_go(struct NDS *this);

void NDS_dma9_start(struct NDS *, struct NDS_DMA_ch *ch, u32 i);
u32 NDS_dma9_go(struct NDS *this);

void NDS_DMA_init(struct NDS *);
void NDS_check_dma9_at_hblank(struct NDS *);
void NDS_check_dma9_at_vblank(struct NDS *);
void NDS_check_dma7_at_vblank(struct NDS *);
void NDS_trigger_dma7_if(struct NDS *, u32 start_timing);
void NDS_trigger_dma9_if(struct NDS *, u32 start_timing);

#endif //JSMOOCH_EMUS_NDS_DMA_H

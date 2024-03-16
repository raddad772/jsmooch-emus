//
// Created by Dave on 2/16/2024.
//

#ifndef JSMOOCH_EMUS_HOLLY_H
#define JSMOOCH_EMUS_HOLLY_H

#include "dreamcast.h"

void holly_write(struct DC* this, u32 addr, u32 val, u32* success);
u64 holly_read(struct DC* this, u32 addr, u32* success);
void holly_reset(struct DC* this);
void DC_recalc_frame_timing(struct DC* this);
void holly_vblank_in(struct DC* this);
void holly_vblank_out(struct DC* this);

void holly_raise_interrupt(struct DC* this, u32 irq_num);
void holly_lower_interrupt(struct DC* this, enum holly_interrupt_masks irq_num);
void holly_eval_interrupt(struct DC* this, enum holly_interrupt_masks irq_num, u32 is_true);
void holly_recalc_interrupts(struct DC* this);


void holly_TA_FIFO_DMA(struct DC* this, u32 src_addr, u32 tx_len, void *src, u32 src_len);

#endif //JSMOOCH_EMUS_HOLLY_H

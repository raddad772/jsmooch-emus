//
// Created by Dave on 2/16/2024.
//

#ifndef JSMOOCH_EMUS_HOLLY_H
#define JSMOOCH_EMUS_HOLLY_H

#include "dreamcast.h"


union HOLLY_PCW {
    struct {
        u32 uv_is_16bit: 1;
        u32 gouraud: 1;
        u32 offset: 1;
        u32 texture: 1;
        u32 col_type: 2;
        u32 volume: 1;
        u32 shadow: 1;
        u32 : 8;
        u32 user_clip: 2;
        u32 strip_len: 2;
        u32 : 3;
        u32 group_en: 1;
        u32 list_type: 3;
        u32 : 1;
        u32 end_of_strip: 1;
        u32 para_type: 3;
    };
    u32 u;
};

union HOLLY_ISP_TSP_IWORD {
    struct {
        u32 : 20;
        u32 dcalc_ctrl : 1;
        u32 cache_bypass : 1;
        u32 uv_16bit : 1;
        u32 gouraud : 1;
        u32 offset : 1;
        u32 texture : 1;
        u32 z_write_disable : 1;
        u32 culling_mode : 2;
        u32 depth_compare_mode : 3;
    };
    u32 u;
};

void holly_init(DC*);
void holly_delete(DC*);

void holly_write(DC*, u32 addr, u32 val, u32* success);
u64 holly_read(DC*, u32 addr, u32* success);
void holly_reset(DC*);
void DC_recalc_frame_timing(DC*);
void holly_vblank_in(DC*);
void holly_vblank_out(DC*);

enum holly_interrupt_masks;
void holly_raise_interrupt(DC*, enum holly_interrupt_masks irq_num, i64 delay);
void holly_lower_interrupt(DC*, enum holly_interrupt_masks irq_num);
void holly_eval_interrupt(DC*, enum holly_interrupt_masks irq_num, u32 is_true);
void holly_recalc_interrupts(DC*);

void holly_TA_FIFO_DMA(DC*, u32 src_addr, u32 tx_len, void *src, u32 src_len);

#endif //JSMOOCH_EMUS_HOLLY_H

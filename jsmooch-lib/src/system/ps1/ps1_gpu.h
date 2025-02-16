//
// Created by . on 2/15/25.
//

#ifndef JSMOOCH_EMUS_PS1_GPU_H
#define JSMOOCH_EMUS_PS1_GPU_H

#include "helpers/int.h"

struct PS1_GPU {
    struct {
        u32 GPUSTAT;
    } io;
    u8 VRAM[1024 * 1024];

    void (*current_ins)(struct PS1_GPU *);
    u8 mmio_buffer[96];
    u32 gp0_buffer[256];
    u32 gp1_buffer[256];
    u32 IRQ_bit;

    u32 cmd[16];
    u32 cmd_arg_num, cmd_arg_index;
    u32 ins_special;

    u32 recv_gp0_len;
    u32 recv_gp1_len;

    u16 *cur_output;
    struct cvec_ptr display_ptr;
    struct JSM_DISPLAY *display;
};

struct PS1;

void PS1_GPU_write_gp0(struct PS1 *, u32 val);
void PS1_GPU_write_gp1(struct PS1 *, u32 val);
u32 PS1_GPU_get_gpuread(struct PS1 *);
u32 PS1_GPU_get_gpustat(struct PS1 *);
#endif //JSMOOCH_EMUS_PS1_GPU_H

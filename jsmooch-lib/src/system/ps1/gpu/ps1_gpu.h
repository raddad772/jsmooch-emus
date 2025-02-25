//
// Created by . on 2/15/25.
//

#ifndef JSMOOCH_EMUS_PS1_GPU_H
#define JSMOOCH_EMUS_PS1_GPU_H

#include "helpers/cvec.h"
#include "helpers/int.h"
#include "rasterize_tri.h"

struct PS1_GPU;
struct PS1_GPU_TEXTURE_SAMPLER;
typedef u16 (*PS1_GPU_texture_sample_func)(struct PS1_GPU_TEXTURE_SAMPLER *ts, i32 u, i32 v);
struct PS1_GPU_TEXTURE_SAMPLER {
    u32 page_x, page_y, base_addr, clut_addr;
    u8 *VRAM;
    u32 semi_mode;
    PS1_GPU_texture_sample_func sample;
};


struct PS1_GPU_VERTEX2 {
    i32 x, y;
    i32 u, v;
    u32 r, g, b;
};

struct PS1;
struct PS1_GPU {
    struct {
        u32 GPUSTAT, GPUREAD;
    } io;
    u8 VRAM[1024 * 1024];

    void (*current_ins)(struct PS1_GPU *);
    u8 mmio_buffer[96];
    u32 gp0_buffer[256];
    u32 gp1_buffer[256];
    u32 IRQ_bit;

    u32 cmd[16];
    u32 cmd_arg_num, cmd_arg_index;
    struct PS1 *bus;
    u32 ins_special;

    u8 recv_gp0[1024 * 1024];
    u8 recv_gp1[1024 * 1024];
    u32 recv_gp0_len;
    u32 recv_gp1_len;

    struct {
        i32 texture_x_flip;
        i32 texture_y_flip;
    } rect;

    u32 tx_win_x_mask, tx_win_y_mask;
    u32 tx_win_x_offset, tx_win_y_offset;
    u32 draw_area_top, draw_area_bottom, draw_area_left, draw_area_right;
    i32 draw_x_offset, draw_y_offset; // applied to all vertices
    u32 display_vram_x_start, display_vram_y_start;
    u32 display_horiz_start, display_horiz_end;
    u32 display_line_start, display_line_end;

    u32 page_base_x, page_base_y;
    i32 semi_transparency;
    enum PS1GPUTEX_DEPTH {
        PS1e_t4bit, PS1e_t8bit, PS1e_t15bit
    } texture_depth;
    u32 dithering;
    u32 draw_to_display;
    u32 force_set_mask_bit;
    u32 preserve_masked_pixels;
    enum PS1GPUFIELD {
        PS1e_top,
        PS1e_bottom
    } field;
    u32 texture_disable;
    u32 hres;
    enum PS1GPUVRES {
        PS1e_y240lines, PS1e_y480lines
    } vres;
    enum PS1GPUVMODE {
        PS1e_ntsc, PS1e_pal
    } vmode;
    enum PS1GPUDDEPTH {
        PS1e_d15bits, PS1e_d24bits
    } display_depth;
    u32 interlaced;
    u32 display_disabled;
    u32 interrupt;
    enum PS1GPUDMADIR {
        PS1e_dma_off,
        PS1e_dma_fifo,
        PS1e_dma_cpu_to_gp0,
        PS1e_dma_vram_to_cpu
    } dma_direction;

    struct PS1_GPU_COLOR_SAMPLER {
        i32 r_start, g_start, b_start;
        i32 r, g, b;
        i32 r_end, g_end, b_end;
    } color1, color2, color3;

    struct RT_POINT2D v0, v1, v2, v3, v4, v5, t0, t1, t2, t3, t4;

    struct PS1_GPU_VERTEX3u {
        u32 x, y, z;
        i32 r, g, b;
        float u, v;
    } s0, s1, s2, s3;

    struct PS1_GPU_VERTEX2 vert[4];

    struct {
        u32 x, y, width, height, img_x, img_y, line_ptr;
    } load_buffer;

    struct PS1_GPU_POLYGON {
        u32 shading, vertices, textured, transparent;
        u32 raw_texture, rgb, num_cmds, clut;
        u32 tx_page;
        struct PS1_GPU_VERTEX2 vert[4];
    } polygon;

    u32 gp0_transfer_remaining;

    void (*handle_gp0)(struct PS1_GPU *, u32 val);

    u16 *cur_output;
    struct cvec_ptr display_ptr;
    struct JSM_DISPLAY *display;
};

struct PS1;

void PS1_GPU_init(struct PS1 *this);
void PS1_GPU_write_gp0(struct PS1_GPU *, u32 val);
void PS1_GPU_write_gp1(struct PS1_GPU *, u32 val);
u32 PS1_GPU_get_gpuread(struct PS1_GPU *);
u32 PS1_GPU_get_gpustat(struct PS1_GPU *);
void PS1_GPU_texture_sampler_new(struct PS1_GPU_TEXTURE_SAMPLER *, u32 page_x, u32 page_y, u32 clut, struct PS1_GPU *ctrl);
#endif //JSMOOCH_EMUS_PS1_GPU_H

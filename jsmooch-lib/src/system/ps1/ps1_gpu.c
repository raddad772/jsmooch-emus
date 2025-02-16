//
// Created by . on 2/15/25.
//
#include <string.h>

#include "ps1_bus.h"
#include "ps1_gpu.h"

#include "helpers/multisize_memaccess.c"

#define R_GPUSTAT 0
#define R_GPUPLAYING 1
#define R_GPUQUIT 2
#define R_GPUGP1 3
#define R_GPUREAD 4
#define R_LASTUSED 23

void PS1_GPU_init(struct PS1 *this)
{
    memset(this->gpu.VRAM, 0x77, 1024*1024);
}

static void ins_null(struct PS1_GPU *this)
{

}

static void unready_cmd(struct PS1_GPU *this)
{
    this->io.GPUSTAT &= 0xFBFFFFFF;
}

static void unready_recv_dma(struct PS1_GPU *this)
{
    this->io.GPUSTAT &= 0xEFFFFFFF;
}

static void unready_vram_to_CPU(struct PS1_GPU *this)
{
    this->io.GPUSTAT &= 0xF7FFFFFF;
}

static void unready_all(struct PS1_GPU *this)
{
    unready_cmd(this);
    unready_recv_dma(this);
    unready_vram_to_CPU(this);
}

static void ready_cmd(struct PS1_GPU *this)
{
    this->io.GPUSTAT |= 0x4000000;
}

static void ready_recv_dma(struct PS1_GPU *this)
{
    this->io.GPUSTAT |= 0x10000000;
}

static void ready_vram_to_CPU(struct PS1_GPU *this)
{
    this->io.GPUSTAT |= 0x8000000;
}

static void ready_all(struct PS1_GPU *this)
{
    ready_cmd(this);
    ready_recv_dma(this);
    ready_vram_to_CPU(this);
}


static inline u32 BGR24to15(u32 c)
{
    return (((c >> 19) & 0x1F) << 10) |
           (((c >> 11) & 0x1F) << 5) |
           ((c >> 3) & 0x1F);
}

static void cmd02_quick_rect(struct PS1_GPU *this)
{
    unready_all(this);
    u32 ysize = (this->cmd[2] >> 16) & 0xFFFF;
    u32 xsize = (this->cmd[2]) & 0xFFFF;
    u32 BGR = BGR24to15(this->cmd[0] & 0xFFFFFF);
    u32 start_y = (this->cmd[1] >> 16) & 0xFFFF;
    u32 start_x = (this->cmd[1]) & 0xFFFF;
    //if (LOG_DRAW_QUADS) console.log('QUICKRECT! COLOR', hex4(BGR), 'X Y', start_x, start_y, 'SZ X SZ Y', xsize, ysize);
    for (u32 y = start_y; y < (start_y+ysize); y++) {
        for (u32 x = start_x; x < (start_x + xsize); x++) {
            //this->setpix(y, x, BGR);
            u32 addr = (2048*y)+(x*2);
            cW[2](this->VRAM, addr, BGR);
        }
    }

    ready_all(this);
}

static inline i32 mksigned16(u32 v)
{
    return SIGNe16to32(v);
}

static void draw_flat_triangle(struct PS1_GPU *this, i32 x0, i32 y0, i32 x1, i32 y1, i32 x2, i32 y2, u32 color)
{
    // sort points vertically
    i32 a, b;
    if (y1 > y2) {
        a = x1;
        b = y1;
        x1 = x2;
        y1 = y2;
        x2 = a;
        y2 = b;
    }

    if (y0 > y1) {
        a = x0;
        b = y0;
        x0 = x1;
        y0 = y1;
        x1 = a;
        y1 = b;
    }

    if (y1 > y2) {
        a = x1;
        b = y1;
        x1 = x2;
        y1 = y2;
        x2 = a;
        y2 = b;
    }

    float dx_far = (x2 - x0) / (y2 - y0 + 1);
    float dx_upper = (x1 - x0) / (y1 - y0 + 1);
    float dx_low = (x2 - x1) / (y2 - y1 + 1);
    float xf = x0;
    float xt = x0 + dx_upper;
    for (i32 y = y0; y <= y2; y++) {
        if (y >= 0) {
            for (let x = (xf > 0 ? xf : 0); x <= xt; x++)
                this->setpix(y, x, color);
            for (let x = xf; x > (xt > 0 ? xt : 0); x--)
                this->setpix(y, x, color);
        }
        xf += dx_far;
        if (y < y1)
            xt += dx_upper;
        else
            xt += dx_low;
    }
}


static void cmd28_draw_flat4untex(struct PS1_GPU *this)
{
    // Flat 4-vertex untextured poly
    i32 x0 = mksigned16(this->cmd[1] & 0xFFFF);
    i32 y0 = mksigned16(this->cmd[1] >> 16);
    i32 x1 = mksigned16(this->cmd[2] & 0xFFFF);
    i32 y1 = mksigned16(this->cmd[2] >> 16);
    i32 x2 = mksigned16(this->cmd[3] & 0xFFFF);
    i32 y2 = mksigned16(this->cmd[3] >> 16);
    i32 x3 = mksigned16(this->cmd[4] & 0xFFFF);
    i32 y3 = mksigned16(this->cmd[4] >> 16);

    /*
    For quads, I do v1, v2, v3 for one triangle and then v2, v3, v4 for the other
     */
    u32 color = BGR24to15(this->cmd[0] & 0xFFFFFF);
#ifdef LOG_DRAW_QUADS
    printf("\nflat4untex %d %d %d %d %d %d %06x", x0, y0, x1, y1, x2, y2, color);
#endif
    draw_flat_triangle(this, x0, y0, x1, y1, x2, y2, color);
    draw_flat_triangle(this, x1, y1, x2, y2, x3, y3, color);
}

static void cmd2c_quad_opaque_flat_textured(struct PS1_GPU *this)
{

}

static void cmd30_tri_shaded_opaque(struct PS1_GPU *this)
{

}

static void cmd38_quad_shaded_opaque(struct PS1_GPU *this)
{

}

static void cmd60_rect_opaque_flat(struct PS1_GPU *this)
{

}

static void cmd64_rect_opaque_flat_textured(struct PS1_GPU *this)
{

}

static void cmd68_rect_1x1(struct PS1_GPU *this)
{

}

static void cmd75_rect_opaque_flat_textured(struct PS1_GPU *this)
{

}

static void gp0_image_load_start(struct PS1_GPU *this)
{

}

static void gp0_cmd_unhandled(struct PS1_GPU *this)
{

}


void PS1_GPU_write_gp0(struct PS1 *this, u32 cmd)
{
    this->gpu.gp0_buffer[this->gpu.recv_gp0_len] = cmd;
    this->gpu.recv_gp0_len++;

    // Check if we have an instruction..
    if (this->gpu.current_ins) {
        this->gpu.cmd[this->gpu.cmd_arg_index++] = cmd;
        if (this->gpu.cmd_arg_index == this->gpu.cmd_arg_num) {
            this->gpu.current_ins(&this->gpu);
            this->gpu.current_ins = NULL;
        }
    } else {
        this->gpu.cmd[0] = cmd;
        this->gpu.cmd_arg_index = 1;
        this->gpu.ins_special = 0;
        this->gpu.cmd_arg_num = 1;
        u32 cmdr = cmd >> 24;
        switch(cmdr) {
            case 0: // NOP
                if (cmd != 0) printf("\nINTERPRETED AS NOP:%08x", cmd);
                break;
            case 0x01: // Clear cache (not implemented)
                break;
            case 0x02: // Quick Rectangle
                //console.log('Quick rectangle!');
                this->gpu.current_ins = &cmd02_quick_rect;
                this->gpu.cmd_arg_num = 3;
                break;
                //case 0x21: // ??
                //    console.log('NOT IMPLEMENT 0x21');
                //    break;
            case 0x28: // flat-shaded rectangle
                this->gpu.current_ins = &cmd28_draw_flat4untex;
                this->gpu.cmd_arg_num = 5;
                break;
            case 0x2C: // polygon, 4 points, textured, flat
                this->gpu.current_ins = &cmd2c_quad_opaque_flat_textured;
                this->gpu.cmd_arg_num = 9;
                break;
            case 0x30: // opaque shaded trinalge
                this->gpu.current_ins = &cmd30_tri_shaded_opaque;
                this->gpu.cmd_arg_num = 6;
                break;
            case 0x38: // polygon, 4 points, gouraud-shaded
                this->gpu.current_ins = &cmd38_quad_shaded_opaque;
                this->gpu.cmd_arg_num = 8;
                break;
            case 0x60: // Rectangle, variable size, opaque
                this->gpu.current_ins = &cmd60_rect_opaque_flat;
                this->gpu.cmd_arg_num = 3;
                break;
            case 0x64: // Rectangle, variable size, textured, flat, opaque
                this->gpu.current_ins = &cmd64_rect_opaque_flat_textured;
                this->gpu.cmd_arg_num = 4;
                break;
            case 0x6A: // Solid 1x1 rect
            case 0x68:
                this->gpu.current_ins = &cmd68_rect_1x1;
                this->gpu.cmd_arg_num = 2;
                break;
            case 0x75: // Rectangle, 8x8, opaque, textured
                this->gpu.current_ins = &cmd75_rect_opaque_flat_textured;
                this->gpu.cmd_arg_num = 3;
                break;
            case 0xBC:
            case 0xB8:
            case 0xA0: // Image stream to GPU
                this->gpu.current_ins = &gp0_image_load_start;
                this->gpu.cmd_arg_num = 3;
                break;
            case 0xC0:
                console.log('WARNING unhandled GP0 command 0xC0');
                this->gpu.cmd_arg_num = 2;
                this->gpu.current_ins = &gp0_cmd_unhandled;
                break;
            case 0xE1: // GP0 Draw Mode
                if (DBG_GP0) console.log('GP0 E1 set draw mode');
                console.log('SET DRAW MODE', hex8(cmd));
                this->page_base_x = cmd & 15;
                this->page_base_y = (cmd >>> 4) & 1;
                switch ((cmd >>> 7) & 3) {
            case 0:
                this->texture_depth = PS1e.T4bit;
            break;
            case 1:
                this->texture_depth = PS1e.T8bit;
            break;
            case 2:
                this->texture_depth = PS1e.T15bit;
            break;
            case 3:
                console.log('UNHANDLED TEXTAR DEPTH!!!');
            break;
        }
                this->dithering = (cmd >>> 9) & 1;
                this->draw_to_display = (cmd >>> 10) & 1;
                this->texture_disable = (cmd >>> 1) & 1;
                this->GPUSTAT_update();
                this->rect.texture_x_flip = (cmd >>> 12) & 1;
                this->rect.texture_y_flip = (cmd >>> 12) & 1;
                break;
            case 0xE2: // Texture window
                if (DBG_GP0) console.log('GP0 E2 set draw mode');
                this->tx_win_x_mask = cmd & 0x1F;
                this->tx_win_y_mask = (cmd >>> 5) & 0x1F;
                this->tx_win_x_offset = (cmd >>> 10) & 0x1F;
                this->tx_win_y_offset = (cmd >>> 15) & 0x1F;
                break;
            case 0xE3: // Set draw area upper-left corner
                if (DBG_GP0) console.log('GP0 E3 set draw area UL corner');
                this->draw_area_top = (cmd >>> 10) & 0x3FF;
                this->draw_area_left = cmd & 0x3FF;
                break;
            case 0xE4: // Draw area lower-right corner
                if (DBG_GP0) console.log('GP0 E4 set draw area LR corner');
                this->draw_area_bottom = (cmd >>> 10) & 0x3FF;
                this->draw_area_right = cmd & 0x3FF;
                break;
            case 0xE5: // Drawing offset
                if (DBG_GP0) console.log('GP0 E5 set drawing offset');
                this->draw_x_offset = mksigned11(cmd & 0x7FF);
                this->draw_y_offset = mksigned11((cmd >>> 11) & 0x7FF);
                break;
            case 0xE6: // Set Mask Bit setting
                if (DBG_GP0) console.log('GP0 E6 set bit mask');
                this->force_set_mask_bit = cmd & 1;
                this->preserve_masked_pixels = (cmd >>> 1) & 1;
                break;
            default:
                console.log('Unknown GP0 command', hex8(cmd >>> 0));
                break;
        }
    }

}

void PS1_GPU_write_gp1(struct PS1 *this, u32 val)
{

}

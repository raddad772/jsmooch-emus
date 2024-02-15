//
// Created by Dave on 2/11/2024.
//

#ifndef JSMOOCH_EMUS_DREAMCAST_H
#define JSMOOCH_EMUS_DREAMCAST_H

#include "helpers/buf.h"
#include "helpers/sys_interface.h"

#include "component/cpu/sh4/sh4_interpreter_opcodes.h"
#include "component/cpu/sh4/sh4_interpreter.h"

void DC_new(struct jsm_system* system, struct JSM_IOmap *iomap);
void DC_delete(struct jsm_system* system);

struct DC {
    struct SH4 sh4;

    u8 RAM[16 * 1024 * 1024];
    u8 VRAM[8 * 1024 * 1024];

    struct buf BIOS;
    struct buf ROM;

    struct {
        struct DC_FB_R_CTRL {
            u32 vclk_div;         // 23 - 0 = NTSC/PAL, 1 = VGA
            u32 fb_strip_buf_en;  // 22 - default 0. 1 for strip buffer
            u32 fb_stripsize;      // 16-21 - size of strip buffer in units of 32 lines. 2 = 32, 4 = 64.
            u32 fb_chroma_threshold; // 8-15 - when framebuffer is ARGB8888, when pixel_alpha < this, 0 is output on CHROMA pin
            u32 fb_concat;         // 6-4 - value added to lower end of 5- or 6-bit frame buffer data to get to 8bits. default 0
            u32 fb_depth;          // 2-3 - 0- 0555RGB 15bit, 0x1 = 565 RGB 16bit,2 = 888 RGB24 packed, 3 = 0888 RGB 32bit
            u32 fb_line_double;    // 1 - 0 = no line-doubling, 1 = line-doubling
            u32 fb_enable;         // 0 - enable or disable framebuffer reads
        } FB_R_CTRL;

        struct DC_FB_W_CTRL {
            u32 fb_alpha_threshold; // 16-23 - when ARGB1555 is written, if pixel_alpha > this, 1 is written for A
            u32 fb_kval; // 8-15 - K value for writing to framebuffer (?)
            u32 fb_dither; // 3 - 0 = discard lower bits. 1 = perform dithering for 16BPP
            u32 fb_packmode; // 0-2 - bit config for writing pixel data to buffer. 0 = 0555 KRGB, 1 = 565 RGB, 2 = 4444 ARGB, 3 = 1555ARGB, 4 = 888RGB24 packed, 5= 0888KRGB 32b, 6= 8888ARGB 32bit, 7=reserved
        } FB_W_CTRL;

        struct DC_FB_R_SIZE {
            u32 fb_modulus; // 20-29 - in 32-bit units, data between last pixel and next pixel. set to 1 in order to put them right after each other
            u32 fb_y_size; // 10-19 - display line #
            u32 fb_x_size; // 0-9 - x pixel size, in 32-bit units
        } FB_R_SIZE;

        u32 FB_W_SOF1;
        u32 FB_W_SOF2;
        u32 FB_R_SOF1;
        u32 FB_R_SOF2;

        u32 last_used_buffer;
        u32 cur_output_num;
        u32 *cur_output;
        u32 *out_buffer[2];

        u64 master_frame;

    } holly;
};


#endif //JSMOOCH_EMUS_DREAMCAST_H

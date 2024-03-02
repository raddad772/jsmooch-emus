//
// Created by Dave on 2/11/2024.
//

#ifndef JSMOOCH_EMUS_DREAMCAST_H
#define JSMOOCH_EMUS_DREAMCAST_H

#include "helpers/buf.h"
#include "helpers/sys_interface.h"
#include "helpers/scheduler.h"

#include "component/cpu/sh4/sh4_interpreter_opcodes.h"
#include "component/cpu/sh4/sh4_interpreter.h"
#include "dc_mem.h"

#define DC_CYCLES_PER_SEC 200000000

#define DC_INT_VBLANK_IN 0x08
#define DC_INT_VBLANK_OUT 0x10

enum DC_MEM_SIZE {
    DC8,
    DC16,
    DC32,
    DC64
};

void DC_new(struct jsm_system* system, struct JSM_IOmap *iomap);
void DC_delete(struct jsm_system* system);

struct DC {
    struct SH4 sh4;

    u8 RAM[16 * 1024 * 1024];
    u8 VRAM[8 * 1024 * 1024];
    u8 OC[8 * 1024]; // Operand Cache[

    struct buf BIOS;
    struct buf flash;
    struct buf ROM;

    struct scheduler_t scheduler;

    struct {
        u32 TCOR0;
        u32 TCNT0;
        u32 PCTRA;
        u32 PDTRA;
        u32 TOCR;
        u32 TSTR;
        u32 SDMR;
            /* interrupt bits for SB_ISTNRM...
bit 21 = End of Transferring interrupt : Punch Through List (*only for HOLLY2)
bit 20 = End of DMA interrupt : Sort-DMA (Transferring for alpha sorting)
bit 19 = End of DMA interrupt : ch2-DMA
bit 18 = End of DMA interrupt : Dev-DMA(Development tool DMA)
bit 17 = End of DMA interrupt : Ext-DMA2(External 2)
bit 16 = End of DMA interrupt : Ext-DMA1(External 1)
bit 15 = End of DMA interrupt : AICA-DMA
bit 14 = End of DMA interrupt : GD-DMA
bit 13 = Maple V blank over interrupt
bit 12 = End of DMA interrupt : Maple-DMA
bit 11 = End of DMA interrupt : PVR-DMA
bit 10 = End of Transferring interrupt : Translucent Modifier Volume List
bit 9 = End of Transferring interrupt : Translucent List
bit 8 = End of Transferring interrupt : Opaque Modifier Volume List
bit 7 = End of Transferring interrupt : Opaque List
bit 6 = End of Transferring interrupt : YUV
bit 5 = H Blank-in interrupt
bit 4 = V Blank-out interrupt
bit 3 = V Blank-in interrupt
bit 2 = End of Render interrupt : TSP
bit 1 = End of Render interrupt : ISP
bit 0 = End of Render interrupt : Video
             */
        u32 SB_ISTNRM;
        u32 SB_LMMODE0;
        u32 SB_LMMODE1;
        u32 SB_IML2NRM;
        u32 SB_IML4NRM;
        u32 SB_IML6NRM;

        u32 BSCR;
        u32 BSCR2;
        u32 RTCOR;
        u32 RFCR;
        u32 RTCSR;
        u32 WCR1;
        u32 WCR2;
        u32 MCR; // memory control register
        u32 MMUCR;
    } io;

    struct {
        u32 ARMRST;
    } aica;

    struct {
        u32 frame_cycle;
        u32 cycles_per_frame;
        u32 cycles_per_line;
        u32 in_vblank;

        struct {
            u32 vblank_in_start;
            u32 vblank_in_end;
            u32 vblank_out_start;
            u32 vblank_out_end;

            u32 vblank_in_yet;
            u32 vblank_out_yet;

        } interrupt;
    } clock;

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
        u32 FB_W_LINESTRIDE;
        u32 FB_R_SOF1;
        u32 FB_R_SOF2;
        u32 FB_X_CLIP;
        u32 FB_Y_CLIP;
        u32 FB_BURSTCTRL;

        struct {
            u32 vcount; // 16-25
            u32 hcount; // 0-9
        } SPG_LOAD;

        u32 SPAN_SORT_CFG;
        u32 FOG_COL_RAM;
        u32 FOG_COL_VERT;
        u32 FOG_DENSITY;
        u32 FOG_TABLE[128];
        u32 FOG_CLAMP_MAX;
        u32 FOG_CLAMP_MIN;

        struct {
            u32 cb_endian_reg; // 17
            u32 index_endian_reg; // 16
            u32 bank_bit; // 8-12
            u32 stride; // 0-4
        } TEXT_CONTROL;
        u32 SCALER_CTL;

        u32 VO_CONTROL;
        u32 VO_BORDER_COL; // 005F8040

        u32 TA_OL_BASE; // starting address for object lists
        u32 TA_OL_LIMIT;
        u32 TA_ISP_BASE;
        u32 TA_ISP_LIMIT;
        u32 TA_GLOB_TILE_CLIP;
        u32 TA_ALLOC_CTRL;
        u32 TA_NEXT_OPB_INIT;
        u32 TA_LIST_INIT;

        struct {
            u32 vblank_out_line; // bits 16-25, default 0x150
            u32 vblank_in_line; // bits 0-9, default 0x104
        } SPG_VBLANK_INT;
        u32 SPG_HBLANK;
        u32 SPG_CONTROL;
        u32 SPG_VBLANK;
        u32 SPG_WIDTH;
        u32 SPG_HBLANK_INT;

        u32 FPU_SHAD_SCALE;
        u32 FPU_PARAM_CFG;
        u32 HALF_OFFSET;
        u32 Y_COEFF;
        float FPU_CULL_VAL;
        float FPU_PERP_VAL;
        float ISP_BACKGND_D;
        struct {
            u32 cache_bypass; // 28
            u32 shadow; // 27
            u32 skip;   // 24-26
            u32 tag_address; // 3-23
            u32 tag_offset; // 0-2
        } ISP_BACKGND_T;

        u32 last_used_buffer;
        u32 cur_output_num;
        u32 *cur_output;
        u32 *out_buffer[2];

        u64 master_frame;

    } holly;

    struct {
        u32 (*read_map[0x100])(struct DC*, u32, enum DC_MEM_SIZE);
        void (*write_map[0x100])(struct DC*, u32, u32, enum DC_MEM_SIZE);
    } mem;

};


void DC_mem_init(struct DC* this);
void DC_raise_interrupt(struct DC* this, u32 imask);
void DC_recalc_interrupts(struct DC* this);

#endif //JSMOOCH_EMUS_DREAMCAST_H

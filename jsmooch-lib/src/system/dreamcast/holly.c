//
// Created by Dave on 2/16/2024.
//

#include "stdio.h"
#include "helpers/debug.h"
#include "holly.h"

#define HOLLY_FB_R_CTRL 0x005F8044
#define HOLLY_FB_R_SIZE 0x005F805C
#define HOLLY_FB_R_SOF1 0x005F8050
#define HOLLY_FB_R_SOF2 0x005F8054

#define HOLLY_FB_W_CTRL 0x005F8048
#define HOLLY_FB_W_LINESTRIDE 0x005F804C
#define HOLLY_FB_W_SOF1 0x005F8060
#define HOLLY_FB_W_SOF2 0x005F8064

#define HOLLY_FB_X_CLIP 0x005F8068
#define HOLLY_FB_Y_CLIP 0x005F806C

static void holly_soft_reset(struct DC* this)
{
    printf("\nHOLLY soft reset!");
    fflush(stdout);
}

void DC_recalc_frame_timing(struct DC* this)
{
    // We need to know:
    // which line to vblank in IRQ. cycle # in frame
    // which line to vblank out IRQ. cycle # in frame
    // how many cycles per line
    // how many lines in frame
    //this->clock.cycles_per_frame;
    this->clock.cycles_per_line = this->clock.cycles_per_frame / this->holly.SPG_LOAD.f.vcount;
    this->clock.interrupt.vblank_in_start = this->holly.SPG_VBLANK_INT.f.vblank_in_line * this->clock.cycles_per_line;
    this->clock.interrupt.vblank_out_start = this->holly.SPG_VBLANK_INT.f.vblank_out_line * this->clock.cycles_per_line;
}

#define B32(b31_b28, b27_24,b23_20,b19_16,b15_12,b11_8,b7_4,b3_0) ( \
  ((0b##b31_b28) << 28) | \
  ((0b##b27_24) << 24) | \
  ((0b##b23_20) << 20) | \
  ((0b##b19_16) << 16) | \
  ((0b##b15_12) << 12) | \
  ((0b##b11_8) << 8) | \
  ((0b##b7_4) << 4) | \
  (0b##b3_0))

#define B10_6_10 B32(0000,0011,1111,1111,0000,0011,1111,1111)

static void holly_TA_list_init(struct DC* this)
{

}

u32 holly_read(struct DC* this, u32 addr) {
    switch ((addr & 0x0000FFFF) | 0x005F0000) {
        case 0x005F8000: // Device ID
            return 0x17FD11DB;
        case 0x005F80004: // Revision
            return 0x0011;
        case 0x005f80d0: // SPG_CONTROL
            return this->holly.SPG_CONTROL.u;
        case 0x005f8144: // TA_LIST_INIT
            return 0;
    }

    printf("\nUNKNOWN HOLLY READ: %08x", addr);
    fflush(stdout);
    return 0;
}

u32 holly_write(struct DC* this, u32 addr, u32 val)
{
    addr = (addr & 0x0000FFFF) | 0x005F0000;
    if ((addr >= 0x005F8200) && (addr <= 0x005F83FC)) {
        this->holly.FOG_TABLE[(addr - 0x005F8200) >> 2] = val;
        return 1;
    }
    switch(addr) {
        case 0x005F80e8: // VO_CONTROL
            this->holly.VO_CONTROL.u = val & B32(0000,0000,0011,1111,0000,0001,1111,1111);
            // 640 pixels 0x00160000
            // 320 pixels 0x00160100
            printf("\nVO_CONTROL SET TO %d PIXELS", (val & 0x100) ? 320 : 640);
            fflush(stdout);
            return 1;
        case 0x005f8124: // TA_OL_BASE
            this->holly.TA_OL_BASE = val & 0x00FFFFE0;
            return 1;
        case 0x005f812c: // TA_OL_LIMIT
            this->holly.TA_OL_LIMIT = val & 0x00FFFFE0;
            return 1;
        case 0x005f8128: // TA_ISP_BASE
            this->holly.TA_ISP_BASE = val & 0x00FFFFFC;
            return 1;
        case 0x005f8130: // TA_ISP_LIMIT
            this->holly.TA_ISP_LIMIT = val & 0x00FFFFFC;
            return 1;
        case 0x005f813C: // TA_GLOB_TILE_CLIP
            this->holly.TA_GLOB_TILE_CLIP = val;
            return 1;
        case 0x005f8140: // TA_ALLOC_CTRL
            this->holly.TA_ALLOC_CTRL = val;
            return 1;
        case 0x005f8164: // TA_NEXT_OPB_INIT
            this->holly.TA_NEXT_OPB_INIT = val;
            return 1;
        case 0x005f8144: // TA_LIST_INIT
            if (val & 0x80000000) holly_TA_list_init(this);
            return 1;
        case 0x005f8068: // FB_X_CLIP
            this->holly.FB_X_CLIP.u = val & B32(0000,0111,1111,1111,0000,0111,1111,1111);
            return 1;
        case 0x005f806c: // FB_Y_CLIP
            this->holly.FB_Y_CLIP.u = val & B32(0000,0111,1111,1111,0000,0111,1111,1111);
            return 1;
        case 0x005f8110: // FB_BURSTCTRL
            this->holly.FB_BURSTCTRL.u = val & B32(0000,0000,0000,1111,1111,1111,0011,1111);
            return 1;
        case 0x005f80d4: // SPG_HBLANK
            this->holly.SPG_HBLANK.u = val & B32(0000,0011,1111,1111,0000,0011,1111,1111);
            return 1;
        case 0x005f80d0: // SPG_CONTROL
            this->holly.SPG_CONTROL.u = val & B32(0000,0000,0000,0000,0000,0011,1111,1111);
            return 1;
        case 0x005f80dc: // SPG_VBLANK
            this->holly.SPG_VBLANK.u = val & B32(0000,0011,1111,1111,0000,0011,1111,1111);
            return 1;
        case 0x005f80d8: // SPG_LOAD
            this->holly.SPG_LOAD.u = val & B32(0000,0011,1111,1111,0000,0011,1111,1111);
            printf("\nSPG HCOUNT:%d VCOUNT:%d", this->holly.SPG_LOAD.f.hcount, this->holly.SPG_LOAD.f.vcount);
            fflush(stdout);
            DC_recalc_frame_timing(this);
            return 1;
        case 0x005f80e0: // SPG_WIDTH
            this->holly.SPG_WIDTH.u = val & B32(1111,1111,1111,1111,1111,1111,0111,1111);
            return 1;
        case 0x005f808c: // ISP_BACKGND_T
            this->holly.ISP_BACKGND_T.u = val & B32(0001,1111,1111,1111,1111,1111,1111,1111);
            return 1;
        case 0x005f80bc: // FOG_CLAMP_MAX
            this->holly.FOG_CLAMP_MAX = val;
            return 1;
        case 0x005f80c0:
            this->holly.FOG_CLAMP_MIN = val;
            return 1;
        case 0x005f80e4: // // TEXT_CONTROL
            this->holly.TEXT_CONTROL.u = val & B32(0000,0000,0000,0011,0001,1111,0001,1111);
            return 1;
        case 0x005f80f4: // SCALER_CTL
            this->holly.SCALER_CTL.u = val & B32(0000,0000,0000,0111,1111,1111,1111,1111);
            return 1;
        case 0x005f804c: // FB_W_LINESTRIDE
            this->holly.FB_W_LINESTRIDE.u = val & B32(0000,0000,0000,0000,0000,0001,1111,1111);
            return 1;
        case 0x005f80c8: // SPG_HBLANK_INT
            this->holly.SPG_HBLANK_INT.u = val & B32(0000,0011,1111,1111,0011,0011,1111,1111);
            return 1;
        case 0x005f8074: // FPU_SHAD_SCALE
            this->holly.FPU_SHAD_SCALE.u = val & B32(0000,0000,0000,0000,0000,0001,1111,1111);
            return 1;
        case 0x005f807c: // FPU_PARAM_CFG
            this->holly.FPU_PARAM_CFG.u = val & B32(0000,0000,0010,1111,1111,1111,1111,1111);
            return 1;
        case 0x005f8080: // HALF_OFFSET
            this->holly.HALF_OFFSET.u = val & B32(0000,0000,0000,0000,0000,0000,0000,0111);
            return 1;
        case 0x005f8118: // Y_COEFF
            this->holly.Y_COEFF.u = val & B32(0000,0000,0000,0000,1111,1111,1111,1111);
            return 1;
        case 0x005f8078: // FPU_CULL_VAL (float)
            this->holly.FPU_CULL_VAL = *(float*)&val;
            return 1;
        case 0x005f8084: // FPU_PERP_VAL (float)
            this->holly.FPU_PERP_VAL = *(float*)&val;
            return 1;
        case 0x005f8088: // ISP_BACKGND_D (float)
            this->holly.ISP_BACKGND_D = *(float*)&val;
            return 1;
        case 0x005F8008: // SOFTRESET
            holly_soft_reset(this);
            return 1;
        case 0x005F8030: // SPAN_SORT_CFG, mostly ignore this
            this->holly.SPAN_SORT_CFG = val;
            return 1;
        case 0x005F80B0: // FOG_COL_RAM
            this->holly.FOG_COL_RAM = val & 0xFFFFFF;
            return 1;
        case 0x005F80CC:
            this->holly.SPG_VBLANK_INT.u = val & B10_6_10;
            printf("\nVBLANK IN:%d OUT:%d", this->holly.SPG_VBLANK_INT.f.vblank_in_line, this->holly.SPG_VBLANK_INT.f.vblank_out_line);
            fflush(stdout);
            DC_recalc_frame_timing(this);
            return 1;
        case 0x005F80B4: // FOG_COL_VERT
            this->holly.FOG_COL_VERT = val & 0xFFFFFF;
            return 1;
        case 0x005F80B8: // FOG_DENSITY
            this->holly.FOG_DENSITY = val & 0xFFFF;
            return 1;
        case 0x005F8040: // V0 border color VO_BORDER_COL. default  = 0x005F8040
            this->holly.VO_BORDER_COL.u = val & B32(0000,0001,1111,1111,1111,1111,1111,1111);
            return 1;
        case HOLLY_FB_R_CTRL:
            this->holly.FB_R_CTRL.u = val & B32(0000,0000,1111,1111,1111,1111,0111,1111);
            printf("\nHOLLY enable:%d depth:%d", this->holly.FB_R_CTRL.f.fb_enable, this->holly.FB_R_CTRL.f.fb_depth);
            return 1;
        case HOLLY_FB_W_CTRL:
            this->holly.FB_W_CTRL.u = val & B32(0000,0000,1111,1111,1111,1111,0000,1111);
            return 1;
        case HOLLY_FB_W_SOF1:
            this->holly.FB_W_SOF1.u = val & 0x01FFFFFC;
            return 1;
        case HOLLY_FB_W_SOF2:
            this->holly.FB_W_SOF2.u = val & 0x01FFFFFC;
            return 1;
        case HOLLY_FB_R_SOF1:
            this->holly.FB_R_SOF1.u = val & 0x00FFFFFC;
            printf("\nRSOF1 new val %08x cycle %llu", val & 0x00FFFFFC, this->sh4.trace_cycles);
            fflush(stdout);
            return 1;
        case HOLLY_FB_R_SOF2:
            this->holly.FB_R_SOF2.u = val & 0x00FFFFFC;
            return 1;
        case HOLLY_FB_R_SIZE:
            this->holly.FB_R_SIZE.u = val & B32(0011,1111,1111,1111,1111,1111,1111,1111);
            printf("\nHOLLY mod:%03x ysize:%d xsize:%d", this->holly.FB_R_SIZE.f.fb_modulus,
                   this->holly.FB_R_SIZE.f.fb_y_size, this->holly.FB_R_SIZE.f.fb_x_size);
            fflush(stdout);
            return 1;
    }
    printf("\nUNKNOWN HOLLY WRITE: %08x data:%08x cyc:%llu", addr, val, this->sh4.trace_cycles);
    fflush(stdout);
    return 0;
}

void holly_vblank_in(struct DC* this)
{
    this->clock.in_vblank = 1;
    this->io.SB_ISTNRM |= 8;
    DC_raise_interrupt(this, DC_INT_VBLANK_IN);
}

void holly_vblank_out(struct DC* this)
{
    this->clock.in_vblank = 0;
    this->io.SB_ISTNRM |= 16;
    DC_raise_interrupt(this, DC_INT_VBLANK_OUT);
}
/*
Mmh no, it's configurable, look at the SB_ISTNRM and SB_IML2NRM/SB_IML4NRM/SB_IML6NRM... registers
originaldave_ — Today at 9:22 PM
oh, so when vblank in triggers
I need to look at those registers to determine what's next?
I hoped I was close to getting interrupts going lol
just wrote a whole (very basic) frame scheduler_t
Senryoku — Today at 9:23 PM
I'm not too sure about my implementation, but interrupts from SB_ISTNRM/SB_ISTEXT/SB_ISTERR will generate SH4 IRL9/11/13 interrupts depending on the register config
 */

void holly_reset(struct DC* this)
{
    this->holly.VO_BORDER_COL.u = 0x005F8040;
    this->holly.SPG_VBLANK_INT.u = 0;
    this->holly.SPG_VBLANK_INT.f.vblank_out_line = 0x150;
    this->holly.SPG_VBLANK_INT.f.vblank_in_line = 0x104;
    this->holly.SPG_LOAD.f.vcount = 400; // TODO: not correct but necessary to avoid divide by 0 in simple scheduler_t
}
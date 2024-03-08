//
// Created by Dave on 2/16/2024.
//

#include "stdio.h"
#include "helpers/debug.h"
#include "holly.h"


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
  ((0b##b31_b28) << 28) | ((0b##b27_24) << 24) | \
  ((0b##b23_20) << 20) | ((0b##b19_16) << 16) | \
  ((0b##b15_12) << 12) | ((0b##b11_8) << 8) | \
  ((0b##b7_4) << 4) | (0b##b3_0))

#define B10_6_10 B32(0000,0011,1111,1111,0000,0011,1111,1111)

static void holly_TA_list_init(struct DC* this)
{

}

u64 holly_read(struct DC* this, u32 addr, u32* success) {
    *success = 1;
    switch ((addr & 0x0000FFFF) | 0x005F0000) {
#include "generated/holly_reads.c"
        case 0x005F8000: // Device ID
            return 0x17FD11DB;
        case 0x005F80004: // Revision
            return 0x0011;
        case 0x005f8144: // TA_LIST_INIT
            return 0;
    }

    printf("\nUNKNOWN HOLLY READ: %08x", addr);
    fflush(stdout);
    *success = 0;
    return 0;
}

void holly_write(struct DC* this, u32 addr, u32 val, u32* success)
{
    *success = 1;
    addr = (addr & 0x0000FFFF) | 0x005F0000;
    if ((addr >= 0x005F8200) && (addr <= 0x005F83FC)) {
        this->holly.FOG_TABLE[(addr - 0x005F8200) >> 2] = val;
    }
    switch(addr) {
#include "generated/holly_writes.c"
        case 0x005f8144: // TA_LIST_INIT
            if (val & 0x80000000) holly_TA_list_init(this);
            return;
        case 0x005f8078: // FPU_CULL_VAL (float)
            this->holly.FPU_CULL_VAL = *(float*)&val;
            return;
        case 0x005f8084: // FPU_PERP_VAL (float)
            this->holly.FPU_PERP_VAL = *(float*)&val;
            return;
        case 0x005f8088: // ISP_BACKGND_D (float)
            this->holly.ISP_BACKGND_D = *(float*)&val;
            return;
        case 0x005F8008: // SOFTRESET
            holly_soft_reset(this);
            return;
    }

    *success = 0;
    printf("\nUNKNOWN HOLLY WRITE: %08x data:%08x cyc:%llu", addr, val, this->sh4.trace_cycles);
    fflush(stdout);
}

void holly_vblank_in(struct DC* this)
{
    this->clock.in_vblank = 1;
    this->io.SB_ISTNRM |= 8;
    DC_raise_interrupt(this, DC_INT_VBLANK_IN);
}

void maple_dma_init(struct DC* this)
{
    printf("\nMAPLE DMA!?!?!?!?");
}


void holly_vblank_out(struct DC* this)
{
    this->clock.in_vblank = 0;
    this->io.SB_ISTNRM |= 16;
    DC_raise_interrupt(this, DC_INT_VBLANK_OUT);

    if ((this->maple.SB_MDTSEL == 1) && this->maple.SB_MDEN) {
        maple_dma_init(this);
    }
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
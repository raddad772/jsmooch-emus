//
// Created by Dave on 2/16/2024.
//

#include "assert.h"
#include "stdio.h"
#include "helpers/debug.h"
#include "holly.h"


static void holly_soft_reset(struct DC* this)
{
    printf("\nHOLLY soft reset!");
    fflush(stdout);
}


#define NI 0b1111
static u32 HOLLY_IRQ_outputs[7] = {
        NI, // no interrupt
        NI, // no interrupt
        0b1101, // level 2
        NI, // no intterupt
        0b1011, // level 4
        NI, // no interrupt
        0b1001  // level 6
};
#undef NI

void holly_recalc_interrupts(struct DC* this)
{
    u32 level2 = (this->io.SB_IML2NRM & this->io.SB_ISTNRM.u) & 0x3FFFFF;
    level2 |= (this->io.SB_IML2EXT.u & this->io.SB_ISTEXT.u);
    level2 |= (this->io.SB_IML2ERR.u & this->io.SB_ISTERR.u);

    u32 level4 = (this->io.SB_IML4NRM & this->io.SB_ISTNRM.u);
    level4 |= (this->io.SB_IML4EXT.u & this->io.SB_ISTEXT.u);
    level4 |= (this->io.SB_IML4ERR.u & this->io.SB_ISTERR.u);

    u32 level6 = (this->io.SB_IML6NRM & this->io.SB_ISTNRM.u);
    level6 |= (this->io.SB_IML6EXT.u & this->io.SB_ISTEXT.u);
    level6 |= (this->io.SB_IML6ERR.u & this->io.SB_ISTERR.u);

    u32 highest_level = 0;
    if (level2) highest_level = 2;
    if (level4) highest_level = 4;
    if (level6) highest_level = 6;
    //if (highest_level != this->sh4.IRL_irq_level) {
        //printf("\nINTERRUPT HIGHEST LEVEL CHANGE TO %d cyc:%llu", highest_level, this->sh4.trace_cycles);
        //printf("\nIML6NRN: %08x", this->io.SB_IML6NRM & this->io.SB_ISTNRM.u & 0x3FFFFF);
    //}
    //printf("\nHOLLY RAISING INTERRUPT ON CYCLE %llu", this->sh4.trace_cycles);
    u32 lv = HOLLY_IRQ_outputs[highest_level];
#ifdef DCDBG_HOLLY_IRQ
    printf("\nSET HOLLY IRQ OUT LEVEL TO %d", lv);
#endif
    SH4_set_IRL_irq_level(&this->sh4, lv);
}

void holly_eval_interrupt(struct DC* this, enum holly_interrupt_masks irq_num, u32 is_true)
{
    if (is_true) holly_raise_interrupt(this, irq_num);
    else holly_lower_interrupt(this, irq_num);
}

void holly_lower_interrupt(struct DC* this, enum holly_interrupt_masks irq_num)
{
    u32 imask = (1 << (irq_num & 0xFF)) ^ 0xFFFFFFFF;
    if (irq_num & 0x100)
        this->io.SB_ISTEXT.u &= imask;
    else if (irq_num & 0x200)
        this->io.SB_ISTERR.u &= imask;
    else
        this->io.SB_ISTNRM.u &= imask;
    holly_recalc_interrupts(this);
}
/*
    hirq_vblank_in = 4,
    hirq_vblank_out = 5,

    hirq_maple_dma = holly_nrm | 12,

    hirq_gdrom_cmd = holly_ext | 0
*/
static const char irq_strings[50][50] = {
        "gdrom_cmd",
        "",
        "",
        "",
        "hirq_vblank_in",
        "hirq_vblank_out",
        "",
        "",
        "",
        "",
        "",
        "",
        "holly_maple_dma"
};

void holly_raise_interrupt(struct DC* this, enum holly_interrupt_masks irq_num)
{
    u32 imask = 1 << (irq_num & 0xFF);
    if (irq_num & 0x100)
        this->io.SB_ISTEXT.u |= imask;
    else if (irq_num & 0x200)
        this->io.SB_ISTERR.u |= imask;
    else
        this->io.SB_ISTNRM.u |= imask;
    //printf("\nHOLLY RAISE INTTERUPT %s val:%08x SB_ISTNRM:%08x SB_ISTEXT:%08x cyc:%llu", irq_strings[irq_num & 0xFF], imask, this->io.SB_ISTNRM.u, this->io.SB_ISTEXT.u, this->sh4.trace_cycles);
    holly_recalc_interrupts(this);
}

void DC_recalc_frame_timing(struct DC* this)
{
    // We need to know:
    // which line to vblank in IRQ. cycle # in frame
    // which line to vblank out IRQ. cycle # in frame
    // how many cycles per line
    // how many lines in frame
    //this->clock.cycles_per_frame;
    this->clock.cycles_per_line = this->clock.cycles_per_frame / this->holly.SPG_LOAD.vcount;
    this->clock.interrupt.vblank_in_start = this->holly.SPG_VBLANK_INT.vblank_in_line * this->clock.cycles_per_line;
    this->clock.interrupt.vblank_out_start = this->holly.SPG_VBLANK_INT.vblank_out_line * this->clock.cycles_per_line;
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

static u32 holly_get_frame_cycle(struct DC* this) {
    return (u32)(this->clock.frame_start_cycle - this->sh4.trace_cycles);
}

static u32 holly_get_SPG_line(struct DC* this) {
    u32 cycle_num = holly_get_frame_cycle(this);
    return cycle_num / this->clock.cycles_per_line;
}

u64 holly_read(struct DC* this, u32 addr, u32* success) {
    *success = 1;
    u32 v;
    switch ((addr & 0x0000FFFF) | 0x005F0000) {
// NOLINTNEXTLINE(bugprone-suspicious-include)
#include "generated/holly_reads.c"
        case 0x005F8000: // Device ID
            return 0x17FD11DB;
        case 0x005F80004: // Revision
            return 0x0011;
        case 0x005f8144: // TA_LIST_INIT
            return 0;
        case 0x005F8004: // REVISION
            return 0x0011;
        case 0x005F810C: // SPG_STATUS read-only
            // determine scanline
            v = holly_get_SPG_line(this) & 0x3FF;
            //TODO: blank, hsync, fieldnum
            v |= (this->clock.in_vblank) << 13;
            return v;
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
        return;
    }
    switch(addr) {
// NOLINTNEXTLINE(bugprone-suspicious-include)
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
    this->io.SB_ISTNRM.vblank_in = 1;
    holly_raise_interrupt(this, hirq_vblank_in);
}

void holly_vblank_out(struct DC* this) {
    this->clock.in_vblank = 0;
    this->io.SB_ISTNRM.vblank_out = 1;
    holly_raise_interrupt(this, hirq_vblank_out);

    if ((this->maple.SB_MDTSEL == 1) && this->maple.SB_MDEN) {
        maple_dma_init(this);
    }
}

void holly_reset(struct DC* this)
{
    this->holly.VO_BORDER_COL.u = 0x005F8040;
    this->holly.SPG_VBLANK_INT.u = 0;
    this->holly.SPG_VBLANK_INT.vblank_out_line = 0x150;
    this->holly.SPG_VBLANK_INT.vblank_in_line = 0x104;
    this->holly.SPG_LOAD.vcount = 400; // TODO: not correct but necessary to avoid divide by 0 in simple scheduler_t
}
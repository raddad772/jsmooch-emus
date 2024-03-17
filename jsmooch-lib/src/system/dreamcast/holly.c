//
// Created by Dave on 2/16/2024.
//

#include "assert.h"
#include "stdio.h"
#include "helpers/debug.h"
#include "holly.h"
#include "helpers/multisize_memaccess.c"

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
        "hirq_vblank_in", // 4
        "hirq_vblank_out", // 5
        "",
        "",
        "",
        "",
        "",
        "",
        "holly_maple_dma", // 12
        "",
        "",
        "",
        "",
        "",
        "",
        "Ch2DMA END", // 19
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
#ifdef SH4_IRQ_DBG
    printf(DBGC_BLUE "\nHOLLY RAISE INTTERUPT %s val:%08x SB_ISTNRM:%08x SB_ISTEXT:%08x cyc:%llu" DBGC_RST, irq_strings[irq_num & 0xFF], imask, this->io.SB_ISTNRM.u, this->io.SB_ISTEXT.u, this->sh4.trace_cycles);
#endif
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
    printf("\nTA LIST INIT!");
    this->holly.TA_NEXT_OPB = this->holly.TA_NEXT_OPB_INIT;
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

enum HOLLY_PCW_paratype {
    ctrl_end_of_list = 0,
    ctrl_user_tile_clip = 1,
    ctrl_object_list_set = 2,
    ctrl_reserved = 3,
    global_polygon_or_modifier_volume = 4,
    global_sprite = 5,
    global_reserved = 6,
    vertex_parameter = 7
};

union HOLLY_PCW {
    struct {
        u32 uv_is_16bit: 1;
        u32 goraud: 1;
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

void holly_TA_FIFO_load(struct DC* this, u32 src_addr, u32 tx_len, void* src)
{
    u32* ptr = (u32*)(((u8*)src)+src_addr);
    u32* end_ptr = ptr + (tx_len >> 2);
    u32 end_of_strip = 0;
    while (ptr < end_ptr) {
        union HOLLY_PCW cmd;
        cmd.u = *ptr;
        ptr++;
        printf("\nCMD FOUND! para_type: %d %08x", cmd.para_type, cmd.u);
        enum HOLLY_PCW_paratype ptype = cmd.para_type;
        switch(ptype) {
            case ctrl_end_of_list:
                printf("\nEND OF LIST! %ld", end_ptr - ptr);
                //ptr = end_ptr;
                break;
            case ctrl_user_tile_clip:
                printf("\nUSER TILE CLIP!");
                break;
            case ctrl_object_list_set:
                printf("\nOBJECT LIST SET!");
                break;
            case ctrl_reserved:
                printf("\nRESERVED!");
                break;
            case global_polygon_or_modifier_volume:
                printf("\nPOLYGON OR MODIFIER VOLUME!");
                break;
            case global_sprite:
                printf("\nSPRITE!");
                break;
            case global_reserved:
                printf("\nGLOBAL RESERVED!");
                break;
            case vertex_parameter:
                printf("\nVERTEX PARAMETER!!!");
                break;
        }
    }

}

void holly_TA_FIFO_DMA(struct DC* this, u32 src_addr, u32 tx_len, void *src, u32 src_len)
{
    if (tx_len == 0) {
        printf("\nHOLLY TA DMA TRANSFER SIZE 0!?");
        return;
    }
    if ((src_addr+tx_len) >= src_len) {
        printf(DBGC_RED "\nTOO LONG DMA TRANSFER CH2" DBGC_RST);
        return;
    }
    holly_TA_FIFO_load(this, src_addr, tx_len, src);
    this->sh4.regs.DMAC_SAR2 = src_addr;
    this->sh4.regs.DMAC_CHCR2.u &= 0xFFFFFFFE;
    this->sh4.regs.DMAC_DMATCR2 = 0x00000000;

    this->io.SB_C2DST = 0x00000000;
    this->io.SB_C2DLEN = 0x00000000;

    holly_raise_interrupt(this, hirq_ch2_dma);
}

void holly_reset(struct DC* this)
{
    this->holly.VO_BORDER_COL.u = 0x005F8040;
    this->holly.SPG_VBLANK_INT.u = 0;
    this->holly.SPG_VBLANK_INT.vblank_out_line = 0x150;
    this->holly.SPG_VBLANK_INT.vblank_in_line = 0x104;
    this->holly.SPG_LOAD.vcount = 400; // TODO: not correct but necessary to avoid divide by 0 in simple scheduler_t
}
//
// Created by Dave on 2/16/2024.
//

/*
 * Thanks very much to Senrokou of the Deecey emulator, who graciously let
 *  me use Deecey as a base for my polygon TA FIFO parsing code
 */


#include <string.h>
#include "assert.h"
#include "stdio.h"
#include "stdlib.h"
#include "helpers/debug.h"
#include "dreamcast.h"
#include "holly.h"
#include "triangle.h"
#include "component/cpu/sh4/sh4_interrupts.h"
#include "helpers/multisize_memaccess.c"

static void holly_soft_reset(struct DC* this)
{
    printf("\nHOLLY soft reset!");
    this->holly.ta.cmd_buffer_index = 0;
    this->holly.ta.list_type = HPL_none;
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
    if ((highest_level != 0) && (dbg.trace_on)) {
        dbg_printf(DBGC_RED "\nHIGHEST LEVEL: %d l2:%04x l4:%04x l6:%04x cyc:%llu" DBGC_RST, highest_level, this->io.SB_IML2NRM, this->io.SB_IML4NRM, this->io.SB_IML6NRM, *this->sh4.trace.cycles);
    }
    //if (highest_level != this->sh4.IRL_irq_level) {
        //printf("\nINTERRUPT HIGHEST LEVEL CHANGE TO %d cyc:%llu", highest_level, this->sh4.clock.trace_cycles);
        //printf("\nIML6NRN: %08x", this->io.SB_IML6NRM & this->io.SB_ISTNRM.u & 0x3FFFFF);
    //}
    //printf("\nHOLLY RAISING INTERRUPT ON STEP %llu", this->sh4.clock.trace_cycles);
    u32 lv = HOLLY_IRQ_outputs[highest_level];
#ifdef DCDBG_HOLLY_IRQ
    printf("\nSET HOLLY IRQ OUT LEVEL TO %d", lv);
#endif
    SH4_set_IRL_irq_level(&this->sh4, lv);
}

void holly_eval_interrupt(struct DC* this, enum holly_interrupt_masks irq_num, u32 is_true)
{
    if (is_true) holly_raise_interrupt(this, irq_num, -1);
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

struct delay_func_struct {
    struct DC* DC;
    enum holly_interrupt_masks which;
};

void holly_delayed_raise_interrupt(void* ptr, u64 key, u64 clock, u32 jitter)
{
    struct DC* this = (struct DC*)ptr;
    holly_raise_interrupt(this, key, 0);
}


void holly_raise_interrupt(struct DC* this, enum holly_interrupt_masks irq_num, i64 delay)
{
    if (delay > 0) {
        i64 tcode = (i64)*this->sh4.trace.cycles + delay;
        u64 key = irq_num;

        struct scheduler_event *e = scheduler_add_abs(&this->scheduler, tcode, key);
        scheduler_bind_or_run(e, this, &holly_delayed_raise_interrupt, tcode, key);
        return;
    }
    u32 imask = 1 << (irq_num & 0xFF);
    if (irq_num & 0x100)
        this->io.SB_ISTEXT.u |= imask;
    else if (irq_num & 0x200)
        this->io.SB_ISTERR.u |= imask;
    else
        this->io.SB_ISTNRM.u |= imask;
#ifdef SH4_IRQ_DBG
    printf(DBGC_BLUE "\nHOLLY RAISE INTTERUPT %s val:%08x SB_ISTNRM:%08x SB_ISTEXT:%08x cyc:%llu" DBGC_RST, irq_strings[irq_num & 0xFF], imask, this->io.SB_ISTNRM.u, this->io.SB_ISTEXT.u, this->sh4.clock.trace_cycles);
#endif
    holly_recalc_interrupts(this);
}

void DC_recalc_frame_timing(struct DC* this)
{
    // We need to know:
    // kind line to vblank in IRQ. cycle # in frame
    // kind line to vblank out IRQ. cycle # in frame
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

    if (!this->holly.TA_LIST_CONT) {
        this->holly.TA_NEXT_OPB = this->holly.TA_NEXT_OPB_INIT;
        this->holly.TA_ITP_CURRENT = this->holly.TA_ISP_BASE;
    }
    this->holly.ta.cmd_buffer_index = 0;
    this->holly.ta.list_type = HPL_none;
}

static u32 holly_get_frame_cycle(struct DC* this) {
    return (u32)(this->clock.frame_start_cycle - *this->sh4.trace.cycles);
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
    printf("\nUNKNOWN HOLLY WRITE: %08x data:%08x cyc:%llu", addr, val, *this->sh4.trace.cycles);
    fflush(stdout);
}

void holly_vblank_in(struct DC* this)
{
    this->clock.in_vblank = 1;
    this->io.SB_ISTNRM.vblank_in = 1;
    holly_raise_interrupt(this, hirq_vblank_in, -1);
}

void holly_vblank_out(struct DC* this) {
    this->clock.in_vblank = 0;
    this->io.SB_ISTNRM.vblank_out = 1;
    holly_raise_interrupt(this, hirq_vblank_out, -1);
    this->holly.ta.list_type = HPL_none;

    if ((this->maple.SB_MDTSEL == 1) && this->maple.SB_MDEN) {
        maple_dma_init(this);
    }
}

enum VolumeInstruction {
    VI_Normal = 0,
    VI_InsideLastPolygon = 1,
    VI_OutsideLastPolygon = 2,
};

void holly_TA_cmd(struct DC* this) {

    if (this->holly.ta.cmd_buffer_index % 8 != 0) return; // All commands are 8 or 16 bytes long
    assert(this->holly.ta.cmd_buffer_index <= 64);
    union HOLLY_PCW cmd;
    //cmd.u = *(u32 *)(&this->holly.ta.cmd_buffer[0]);
    enum HOLLY_PCW_paratype ptype = cmd.para_type;
}

void holly_TA_FIFO_load(struct DC* this, u32 src_addr, u32 tx_len, void* src)
{
    u32 bytes_tx = 0;
    for (u32 i = 0; i < tx_len; i+= 8) {
        //memcpy(&this->holly.ta.cmd_buffer[this->holly.ta.cmd_buffer_index], (src+src_addr+i), tx_len);
        this->holly.ta.cmd_buffer_index += 8;
        printf("\nSZ: %d", this->holly.ta.cmd_buffer_index);
        bytes_tx += 8;
        holly_TA_cmd(this);
    }
    if (bytes_tx < tx_len) {
        printf("\nREVISIT THIS FOOL");
        printf("\nREVISIT THIS FOOL");
        printf("\nREVISIT THIS FOOL");
        printf("\nREVISIT THIS FOOL");
        printf("\nREVISIT THIS FOOL");
        printf("\nREVISIT THIS FOOL");
    }
}

void dump_RAM_to_console(u32 print_addr, void *src, u32 len)
{
    u8* ptr = src;
    // 16 bytes per line
    u32 bytes_printed = 0;
    for (u32 laddr = 0; laddr < len; laddr += 16) {
        printf("\n%08X   ", print_addr+bytes_printed);
        for (u32 i = 0; i < 16; i++) {
            printf("%02X ", (u32)*ptr);
            ptr++;
            bytes_printed++;
            if (bytes_printed >= len) return;
        }
    }
}

void holly_TA_FIFO_DMA(struct DC* this, u32 src_addr, u32 tx_len, void *src, u32 src_len)
{
    if (tx_len == 0) {
        printf("\nHOLLY TA DMA TRANSFER SIZE 0!?");
        return;
    }
    /*if ((src_addr+tx_len) >= src_len) {
        printf(DBGC_RED "\nTOO LONG DMA TRANSFER CH2 %08x" DBGC_RST, src_addr);
        return;
    }*/
    printf("\nSRCADDR:%08x LEN:%d", src_addr, tx_len);
    dump_RAM_to_console(src_addr, this->RAM + (src_addr & 0xFFFFFF), tx_len);
    holly_TA_FIFO_load(this, src_addr & 0xFFFFFF, tx_len, this->RAM);
    this->sh4.regs.DMAC_SAR2 = src_addr + tx_len;
    this->sh4.regs.DMAC_CHCR2.u &= 0xFFFFFFFE;
    this->sh4.regs.DMAC_DMATCR2 = 0x00000000;

    this->io.SB_C2DST = 0x00000000;
    this->io.SB_C2DLEN = 0x00000000;

    holly_raise_interrupt(this, hirq_ch2_dma, 200);
}

void holly_reset(struct DC* this)
{
    this->holly.VO_BORDER_COL.u = 0;
    this->holly.SPG_VBLANK_INT.u = 0;
    this->holly.SPG_VBLANK_INT.vblank_out_line = 0x150;
    this->holly.SPG_VBLANK_INT.vblank_in_line = 0x104;
    this->holly.SPG_LOAD.vcount = 400; // TODO: not correct but necessary to avoid divide by 0 in simple scheduler_t
}


void holly_init(struct DC* this)
{
    cvec_init(&this->holly.ta.cmd_buffer, 1, 2*1024*1024);
}

void holly_delete(struct DC* this)
{
    cvec_delete(&this->holly.ta.cmd_buffer);
}
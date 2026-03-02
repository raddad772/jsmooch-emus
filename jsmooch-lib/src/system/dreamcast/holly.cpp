//
// Created by Dave on 2/16/2024.
//

/*
 * Thanks very much to Senrokou of the Deecey emulator, who graciously let
 *  me use Deecey as a base for my polygon TA FIFO parsing code
 */


#include <cassert>
#include "helpers/debug.h"
#include "dc_bus.h"
#include "component/cpu/sh4/sh4_interrupts.h"

namespace DC::HOLLY {

void core::soft_reset()
{
    printf("\nHOLLY soft reset!");
    ta.cmd_buffer_index = 0;
    ta.list_type = HPL_none;
}

#define NI 0b1111
    static constexpr u32 IRQ_outputs[7] = {
    NI, // no interrupt
    NI, // no interrupt
    0b1101, // level 2
    NI, // no intterupt
    0b1011, // level 4
    NI, // no interrupt
    0b1001  // level 6
};
#undef NI

    void core::recalc_interrupts()
    {
        u32 level2 = (bus->io.SB_IML2NRM & bus->io.SB_ISTNRM.u) & 0x3FFFFF;
        level2 |= (bus->io.SB_IML2EXT.u & bus->io.SB_ISTEXT.u);
        level2 |= (bus->io.SB_IML2ERR.u & bus->io.SB_ISTERR.u);

        u32 level4 = (bus->io.SB_IML4NRM & bus->io.SB_ISTNRM.u);
        level4 |= (bus->io.SB_IML4EXT.u & bus->io.SB_ISTEXT.u);
        level4 |= (bus->io.SB_IML4ERR.u & bus->io.SB_ISTERR.u);

        u32 level6 = (bus->io.SB_IML6NRM & bus->io.SB_ISTNRM.u);
        level6 |= (bus->io.SB_IML6EXT.u & bus->io.SB_ISTEXT.u);
        level6 |= (bus->io.SB_IML6ERR.u & bus->io.SB_ISTERR.u);

        u32 highest_level = 0;
        if (level2) highest_level = 2;
        if (level4) highest_level = 4;
        if (level6) highest_level = 6;
        if ((highest_level != 0) && (dbg.trace_on)) {
            dbg_printf(DBGC_RED "\nHIGHEST LEVEL: %d l2:%04x l4:%04x l6:%04x cyc:%llu" DBGC_RST, highest_level, bus->io.SB_IML2NRM, bus->io.SB_IML4NRM, bus->io.SB_IML6NRM, bus->trace_cycles);
        }
        //if (highest_level != sh4.IRL_irq_level) {
        //printf("\nINTERRUPT HIGHEST LEVEL CHANGE TO %d cyc:%llu", highest_level, sh4.clock.trace_cycles);
        //printf("\nIML6NRN: %08x", bus->io.SB_IML6NRM & bus->io.SB_ISTNRM.u & 0x3FFFFF);
        //}
        //printf("\nHOLLY RAISING INTERRUPT ON STEP %llu", sh4.clock.trace_cycles);
        u32 lv = HOLLY::IRQ_outputs[highest_level];
#ifdef DCDBG_HOLLY_IRQ
        printf("\nSET HOLLY IRQ OUT LEVEL TO %d", lv);
#endif
        bus->sh4.set_IRL(lv);
    }


void core::eval_interrupt(interruptmasks irq_num, u32 is_true)
{
    if (is_true) raise_interrupt(irq_num, -1);
    else lower_interrupt(irq_num);
}

void core::lower_interrupt(interruptmasks irq_num)
{
    u32 imask = (1 << (irq_num & 0xFF)) ^ 0xFFFFFFFF;
    if (irq_num & 0x100)
        bus->io.SB_ISTEXT.u &= imask;
    else if (irq_num & 0x200)
        bus->io.SB_ISTERR.u &= imask;
    else
        bus->io.SB_ISTNRM.u &= imask;
    recalc_interrupts();
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

void holly_delayed_raise_interrupt(void* ptr, u64 key, u64 clock, u32 jitter)
{
    auto *th = static_cast<core *>(ptr);
    th->raise_interrupt(static_cast<interruptmasks>(key), 0);
}


void core::raise_interrupt(interruptmasks irq_num, i64 delay)
{
    if (delay > 0) {
        i64 tcode = static_cast<i64>(bus->trace_cycles) + delay;
        u64 key = irq_num;

        bus->scheduler.add_or_run_abs(tcode, key, this, &holly_delayed_raise_interrupt, nullptr);
        //scheduler_event *e = bus->scheduler.add_abs(tcode, key, 1);
        //bus->scheduler.bind_or_run(e, this, &holly_delayed_raise_interrupt, tcode, key, nullptr);
        return;
    }
    u32 imask = 1 << (irq_num & 0xFF);
    if (irq_num & 0x100)
        bus->io.SB_ISTEXT.u |= imask;
    else if (irq_num & 0x200)
        bus->io.SB_ISTERR.u |= imask;
    else
        bus->io.SB_ISTNRM.u |= imask;
#ifdef SH4_IRQ_DBG
    printf(DBGC_BLUE "\nHOLLY RAISE INTTERUPT %s val:%08x SB_ISTNRM:%08x SB_ISTEXT:%08x cyc:%llu" DBGC_RST, irq_strings[irq_num & 0xFF], imask, bus->io.SB_ISTNRM.u, bus->io.SB_ISTEXT.u, sh4.clock.trace_cycles);
#endif
    recalc_interrupts();
}

void core::recalc_frame_timing()
{
    // We need to know:
    // kind line to vblank in IRQ. cycle # in frame
    // kind line to vblank out IRQ. cycle # in frame
    // how many cycles per line
    // how many lines in frame
    //clock.cycles_per_frame;
    bus->clock.cycles_per_line = bus->clock.cycles_per_frame / SPG_LOAD.vcount;
    bus->clock.interrupt.vblank_in_start = SPG_VBLANK_INT.vblank_in_line * bus->clock.cycles_per_line;
    bus->clock.interrupt.vblank_out_start = SPG_VBLANK_INT.vblank_out_line * bus->clock.cycles_per_line;
}

#define B32(b31_b28, b27_24,b23_20,b19_16,b15_12,b11_8,b7_4,b3_0) ( \
  ((0b##b31_b28) << 28) | ((0b##b27_24) << 24) | \
  ((0b##b23_20) << 20) | ((0b##b19_16) << 16) | \
  ((0b##b15_12) << 12) | ((0b##b11_8) << 8) | \
  ((0b##b7_4) << 4) | (0b##b3_0))

#define B10_6_10 B32(0000,0011,1111,1111,0000,0011,1111,1111)

void core::TA_list_init()
{
    printf("\nTA LIST INIT!");

    if (!TA_LIST_CONT) {
        TA_NEXT_OPB = TA_NEXT_OPB_INIT;
        TA_ITP_CURRENT = TA_ISP_BASE;
    }
    ta.cmd_buffer_index = 0;
    ta.list_type = HPL_none;
}

u32 core::get_frame_cycle() {
    return static_cast<u32>(bus->clock.frame_start_cycle - bus->trace_cycles);
}

u32 core::get_SPG_line() {
    u32 cycle_num = get_frame_cycle();
    return cycle_num / bus->clock.cycles_per_line;
}

u64 core::read(u32 addr, bool* success) {
    *success = true;
    u32 v;
    switch ((addr & 0x0000FFFF) | 0x005F0000) {
// NOLINTNEXTLINE(bugprone-suspicious-include)
#include "generated/holly_reads.cpp"
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
            v = get_SPG_line() & 0x3FF;
            //TODO: blank, hsync, fieldnum
            v |= (bus->clock.in_vblank) << 13;
            return v;
    }

    printf("\nUNKNOWN HOLLY READ: %08x", addr);
    fflush(stdout);
    *success = false;
    return 0;
}

void core::write(u32 addr, u32 val, bool* success)
{
    *success = true;
    addr = (addr & 0x0000FFFF) | 0x005F0000;
    if ((addr >= 0x005F8200) && (addr <= 0x005F83FC)) {
        FOG_TABLE[(addr - 0x005F8200) >> 2] = val;
        return;
    }
    switch(addr) {
// NOLINTNEXTLINE(bugprone-suspicious-include)
#include "generated/holly_writes.cpp"
        case 0x005f8144: // TA_LIST_INIT
            if (val & 0x80000000) TA_list_init();
            return;
        case 0x005f8078: // FPU_CULL_VAL (float)
            FPU_CULL_VAL = *reinterpret_cast<float *>(&val);
            return;
        case 0x005f8084: // FPU_PERP_VAL (float)
            FPU_PERP_VAL = *reinterpret_cast<float *>(&val);
            return;
        case 0x005f8088: // ISP_BACKGND_D (float)
            ISP_BACKGND_D = *reinterpret_cast<float *>(&val);
            return;
        case 0x005F8008: // SOFTRESET
            soft_reset();
            return;
    }

    *success = false;
    printf("\nUNKNOWN HOLLY WRITE: %08x data:%08x cyc:%llu", addr, val, bus->trace_cycles);
    fflush(stdout);
}

void core::vblank_in()
{
    bus->clock.in_vblank = 1;
    bus->io.SB_ISTNRM.vblank_in = 1;
    raise_interrupt(hirq_vblank_in, -1);
}

void core::vblank_out() {
    bus->clock.in_vblank = 0;
    bus->io.SB_ISTNRM.vblank_out = 1;
    raise_interrupt(hirq_vblank_out, -1);
    ta.list_type = HPL_none;

    if ((bus->maple.SB_MDTSEL == 1) && bus->maple.SB_MDEN) {
        bus->maple.dma_init();
    }
}

enum VolumeInstruction {
    VI_Normal = 0,
    VI_InsideLastPolygon = 1,
    VI_OutsideLastPolygon = 2,
};

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable: 4700) // warning C4700: uninitialized local variable 'cmd' used
#endif

void core::TA_cmd() {

    if (ta.cmd_buffer_index % 8 != 0) return; // All commands are 8 or 16 bytes long
    assert(ta.cmd_buffer_index <= 64);
    PCW cmd;
    //cmd.u = *(u32 *)(&ta.cmd_buffer[0]);
    PCW_paratype ptype = static_cast<PCW_paratype>(cmd.para_type);
}

#if defined(_MSC_VER)
#pragma warning(pop)
#endif

void core::TA_FIFO_load(u32 src_addr, u32 tx_len, void* src)
{
    u32 bytes_tx = 0;
    printf("\nWANR UNDONE TA_FIFO_load");
    for (u32 i = 0; i < tx_len; i+= 8) {
        //memcpy(&ta.cmd_buffer[ta.cmd_buffer_index], (src+src_addr+i), tx_len);
        ta.cmd_buffer_index += 8;
        printf("\nSZ: %d", ta.cmd_buffer_index);
        bytes_tx += 8;
        TA_cmd();
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

void core::dump_RAM_to_console(u32 print_addr, void *src, u32 len)
{
    auto *ptr = static_cast<u8 *>(src);
    // 16 bytes per line
    u32 bytes_printed = 0;
    for (u32 laddr = 0; laddr < len; laddr += 16) {
        printf("\n%08X   ", print_addr+bytes_printed);
        for (u32 i = 0; i < 16; i++) {
            printf("%02X ", static_cast<u32>(*ptr));
            ptr++;
            bytes_printed++;
            if (bytes_printed >= len) return;
        }
    }
}

void core::TA_FIFO_DMA(u32 src_addr, u32 tx_len, void *src, u32 src_len)
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
    dump_RAM_to_console(src_addr, bus->RAM + (src_addr & 0xFFFFFF), tx_len);
    TA_FIFO_load(src_addr & 0xFFFFFF, tx_len, bus->RAM);
    bus->sh4.regs.DMAC_SAR2 = src_addr + tx_len;
    bus->sh4.regs.DMAC_CHCR2.u &= 0xFFFFFFFE;
    bus->sh4.regs.DMAC_DMATCR2 = 0x00000000;

    bus->io.SB_C2DST = 0x00000000;
    bus->io.SB_C2DLEN = 0x00000000;

    raise_interrupt(hirq_ch2_dma, 200);
}

void core::reset()
{
    VO_BORDER_COL.u = 0;
    SPG_VBLANK_INT.u = 0;
    SPG_VBLANK_INT.vblank_out_line = 0x150;
    SPG_VBLANK_INT.vblank_in_line = 0x104;
    SPG_LOAD.vcount = 400; // TODO: not correct but necessary to avoid divide by 0 in simple scheduler_t
}


}
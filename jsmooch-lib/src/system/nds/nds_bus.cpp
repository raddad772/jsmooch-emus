//
// Created by . on 12/4/24.
//
#include <cstring>
#include <cassert>

#include "nds_regs.h"
#include "nds_bus.h"
#include "nds_vram.h"
#include "nds_dma.h"
#include "nds_irq.h"
#include "nds_ipc.h"
#include "nds_rtc.h"
#include "nds_spi.h"
#include "nds_timers.h"
#include "nds_debugger.h"
#include "helpers/multisize_memaccess.cpp"

namespace NDS {

static constexpr u32 masksz[5] = { 0, 0xFF, 0xFFFF, 0, 0xFFFFFFFF};
static constexpr u32 maskalign[5] = {0, 0xFFFFFFFF, 0xFFFFFFFE, 0, 0xFFFFFFFC};

u32 core::busrd7_invalid(u32 addr, u8 sz, u32 access, bool has_effect) {
    printf("\nREAD7 UNKNOWN ADDR:%08x sz:%d", addr, sz);
    waitstates.current_transaction++;
    //dbg.var++;
    //if (dbg.var > 15) dbg_break("too many bad reads", clock.master_cycle_count);
    return 0;
}

u32 core::busrd9_invalid(u32 addr, u8 sz, u32 access, bool has_effect) {
    printf("\nREAD9 UNKNOWN ADDR:%08x sz:%d", addr, sz);
    waitstates.current_transaction++;
    //dbg.var++;
    //if (dbg.var > 15) dbg_break("too many bad reads", clock.master_cycle_count);
    return 0;
}

void core::buswr7_invalid(u32 addr, u8 sz, u32 access, u32 val) {
    printf("\nWRITE7 UNKNOWN ADDR:%08x sz:%d DATA:%08x", addr, sz, val);
    waitstates.current_transaction++;
    ::dbg.var++;
    //if (dbg.var > 15) dbg_break("too many bad writes", clock.master_cycle_count);
}

void core::buswr9_invalid(u32 addr, u8 sz, u32 access, u32 val) {
    waitstates.current_transaction++;
    static int pokemon_didit = 0;
    if ((addr == 0) && !pokemon_didit) {
        pokemon_didit = 1;
        printf("\nWRITE9 UNKNOWN ADDR:%08x sz:%d DATA:%08x", addr, sz, val);
    }
    if (addr != 0) {
        printf("\nWRITE9 UNKNOWN ADDR:%08x sz:%d DATA:%08x", addr, sz, val);
        ::dbg.var++;
        if (::dbg.var > 15) dbg_break("too many bad writes", clock.master_cycle_count7);
    }
    //dbg_break("unknown addr write9", clock.master_cycle_count7);
}

void core::buswr7_shared(u32 addr, u8 sz, u32 access, u32 val)
{
    if (addr >= 0x03800000) return cW[sz](mem.WRAM_arm7, addr & 0xFFFF, val);
    if (!mem.io.RAM7.disabled) cW[sz](mem.WRAM_share, (addr & mem.io.RAM7.mask) + mem.io.RAM7.base, val);
    else cW[sz](mem.WRAM_arm7, addr & 0xFFFF, val);
}

u32 core::busrd7_shared(u32 addr, u8 sz, u32 access, bool has_effect)
{
    if (addr >= 0x03800000) return cR[sz](mem.WRAM_arm7, addr & 0xFFFF);
    if (mem.io.RAM7.disabled) return cR[sz](mem.WRAM_arm7, addr & 0xFFFF);
    return cR[sz](mem.WRAM_share, (addr & mem.io.RAM7.mask) + mem.io.RAM7.base);
}

void core::buswr7_vram(u32 addr, u8 sz, u32 access, u32 val)
{
    u32 bank = (addr >> 17) & 1;
    if (mem.vram.map.arm7[bank]) return cW[sz](mem.vram.map.arm7[bank], addr & 0x1FFFF, val);

    //printf("\nWarning write7 to unmapped VRAM:%08x sz:%d data:%08x", addr, sz, val);
}

u32 core::busrd7_vram(u32 addr, u8 sz, u32 access, bool has_effect)
{
    u32 bank = (addr >> 17) & 1;
    if (mem.vram.map.arm7[bank]) return cR[sz](mem.vram.map.arm7[bank], addr & 0x1FFFF);

    return busrd7_invalid(addr, sz, access, has_effect);
}

void core::buswr7_gba_cart(u32 addr, u8 sz, u32 access, u32 val)
{
}

u32 core::busrd7_gba_cart(u32 addr, u8 sz, u32 access, bool has_effect)
{
    if (!io.rights.gba_slot) return (addr & 0x1FFFF) >> 1;
    return 0;
}

void core::buswr7_gba_sram(u32 addr, u8 sz, u32 access, u32 val)
{
}

u32 core::busrd7_gba_sram(u32 addr, u8 sz, u32 access, bool has_effect)
{
    if (!io.rights.gba_slot) return masksz[sz];
    return 0;
}

u32 core::busrd9_main(u32 addr, u8 sz, u32 access, bool has_effect)
{
    return cR[sz](mem.RAM, addr & 0x3FFFFF);
}

void core::buswr9_main(u32 addr, u8 sz, u32 access, u32 val)
{
    cW[sz](mem.RAM, addr & 0x3FFFFF, val);
}


void core::buswr9_gba_cart(u32 addr, u8 sz, u32 access, u32 val)
{
}

u32 core::busrd9_gba_cart(u32 addr, u8 sz, u32 access, bool has_effect)
{
    if (io.rights.gba_slot) return (addr & 0x1FFFF) >> 1;
    return 0;
}

void core::buswr9_gba_sram(u32 addr, u8 sz, u32 access, u32 val)
{
    return;
}

u32 core::busrd9_gba_sram(u32 addr, u8 sz, u32 access, bool has_effect)
{
    if (io.rights.gba_slot) return masksz[sz];
    return 0;
}

void core::buswr9_shared(u32 addr, u8 sz, u32 access, u32 val)
{
    if (!mem.io.RAM9.disabled) cW[sz](mem.WRAM_share, (addr & mem.io.RAM9.mask) + mem.io.RAM9.base, val);
}

u32 core::busrd9_shared(u32 addr, u8 sz, u32 access, bool has_effect)
{
    if (mem.io.RAM9.disabled) return 0; // undefined
    return cR[sz](mem.WRAM_share, (addr & mem.io.RAM9.mask) + mem.io.RAM9.base);
}

void core::buswr9_obj_and_palette(u32 addr, u8 sz, u32 access, u32 val)
{
    if (addr < 0x05000000) return;
    addr &= 0x7FF;
    if (addr < 0x200) return ppu.write_2d_bg_palette(0, addr & 0x1FF, sz, val);
    if (addr < 0x400) {
        //if (val != 0) printf("\nENG1 WRITE COLOR:%d VAL:%04x", (addr & 0x1FF) >> 1, val);
        return ppu.write_2d_obj_palette(0, addr & 0x1FF, sz, val);
    }
    if (addr < 0x600) return ppu.write_2d_bg_palette(1, addr & 0x1FF, sz, val);
    if (addr < 0x800) {
        return ppu.write_2d_obj_palette(1, addr & 0x1FF, sz, val);
    }
    buswr9_invalid(addr, sz, access, val);
}

u32 core::busrd9_obj_and_palette(u32 addr, u8 sz, u32 access, bool has_effect)
{
    if (addr < 0x05000000) return busrd9_invalid(addr, sz, access, has_effect);
    addr &= 0x7FF;
    if (addr < 200)
        return cR[sz](ppu.eng2d[0].mem.bg_palette, addr & 0x1FF);
    if (addr < 400)
        return cR[sz](ppu.eng2d[0].mem.obj_palette, addr & 0x1FF);
    if (addr < 600)
        return cR[sz](ppu.eng2d[1].mem.bg_palette, addr & 0x1FF);
    if (addr < 800)
        return cR[sz](ppu.eng2d[1].mem.obj_palette, addr & 0x1FF);
    return busrd9_invalid(addr, sz, access, has_effect);
}

void core::buswr9_vram(u32 addr, u8 sz, u32 access, u32 val)
{
    if (sz == 1) {
        static int a = 1;
        if (a) {
            printf("\nWarning ignore 8-bit vram write!");
            a = 0;
        }
        return; // 8-bit writes ignored
    }
    //printf("\nVRAM write addr:%08x vaddr:%06x MSTA:%d OFS:%d val:%04x", addr, addr & 0x1FFFF, mem.vram.io.bank[0].mst, mem.vram.io.bank[0].ofs, val);
    u8 *ptr = mem.vram.map.arm9[NDSVRAMSHIFT(addr) & NDSVRAMMASK];
    if (ptr) return cW[sz](ptr, addr & 0x3FFF, val);

    static int a = 2;
    if (a) {
        printf("\nInvalid VRAM write unmapped addr:%08x sz:%d val:%08x", addr, sz, val);
        a--;
        if (a == 0) printf("\nMuting invalid VRAM write messages...");
    }
    //dbg_break("Unmapped VRAM9 write", clock.master_cycle_count7);
}

u32 core::busrd9_vram(u32 addr, u8 sz, u32 access, bool has_effect)
{
    u8 *ptr = mem.vram.map.arm9[NDSVRAMSHIFT(addr) & NDSVRAMMASK];
    if (ptr) return cR[sz](ptr, addr & 0x3FFF);

    printf("\nInvalid VRAM read unmapped addr:%08x sz:%d", addr, sz);
    //dbg_break("Unmapped VRAM9 read", clock.master_cycle_count7);
    return 0;
}

void core::buswr9_oam(u32 addr, u8 sz, u32 access, u32 val)
{
    addr &= 0x7FF;
    if (addr < 0x400) return cW[sz](ppu.eng2d[0].mem.oam, addr & 0x3FF, val);
    else return cW[sz](ppu.eng2d[1].mem.oam, addr & 0x3FF, val);
    //buswr9_invalid(addr, sz, access, val);
}

u32 core::busrd9_oam(u32 addr, u8 sz, u32 access, bool has_effect)
{
    addr &= 0x7FF;
    if (addr < 0x400) return cR[sz](ppu.eng2d[0].mem.oam, addr & 0x3FF);
    else return cR[sz](ppu.eng2d[1].mem.oam, addr & 0x3FF);
}

u32 core::busrd7_bios7(u32 addr, u8 sz, u32 access, bool has_effect)
{
    return cR[sz](mem.bios7, addr & 0x3FFF);
}

void core::buswr7_bios7(u32 addr, u8 sz, u32 access, u32 val)
{
}

u32 core::busrd7_main(u32 addr, u8 sz, u32 access, bool has_effect)
{
    return cR[sz](mem.RAM, addr & 0x3FFFFF);
}

void core::buswr7_main(u32 addr, u8 sz, u32 access, u32 val)
{
    cW[sz](mem.RAM, addr & 0x3FFFFF, val);
}

static u32 DMA_CH_NUM(u32 addr)
{
    addr &= 0xFF;
    if (addr < 0xBC) return 0;
    if (addr < 0xC8) return 1;
    if (addr < 0xD4) return 2;
    return 3;
}

u32 core::busrd7_io8(u32 addr, u8 sz, u32 access, bool has_effect)
{
    u32 v;
    switch(addr) {
        case R_AUXSPICNT:
            if (!io.rights.nds_slot_is7) return 0;
            return cart.read_spicnt() & 0xFF;
        case R_AUXSPICNT+1:
            if (!io.rights.nds_slot_is7) return 0;
            return cart.read_spicnt() >> 8;

        case R_RCNT+0: return io.sio_data & 0xFF;
        case R_RCNT+1: return io.sio_data >> 8;
        case 0x04000138:
        case 0x04000139: return 0;
        case R7_SPICNT+0:
            spi.cnt.busy = clock.current7() < spi.busy_until;
            return spi.cnt.u & 0xFF;
        case R7_SPICNT+1:
            return spi.cnt.u >> 8;

        case R_POSTFLG:
            return io.arm7.POSTFLG;
        case R_POSTFLG+1:
            return 0;

        case R7_WIFIWAITCNT:
            return io.powcnt.wifi ? io.powcnt.wifi_waitcnt : 0;
        case R7_WIFIWAITCNT+1:
            return 0;

        case R7_POWCNT2+0:
            v = io.powcnt.speakers;
            v |= io.powcnt.wifi << 1;
            return v;
        case R7_POWCNT2+1:
            return 0;

        case R_KEYINPUT+0: // buttons!!!
            return controller.get_state(0);
        case R_KEYINPUT+1: // buttons!!!
            return controller.get_state(1);
        case R_EXTKEYIN+0:
            return controller.get_state(2);
        case R_EXTKEYIN+1:
            return 0;

        case R_IPCFIFOCNT+0:
            // send fifo from 7 is to_9
            v = io.ipc.to_arm9.is_empty();
            v |= io.ipc.to_arm9.is_full() << 1;
            v |= io.ipc.arm7.irq_on_send_fifo_empty << 2;
            return v;
        case R_IPCFIFOCNT+1:
            v = io.ipc.to_arm7.is_empty();
            v |= io.ipc.to_arm7.is_full() << 1;
            v |= io.ipc.arm7.irq_on_recv_fifo_not_empty << 2;
            v |= io.ipc.arm7.error << 6;
            v |= io.ipc.arm7.fifo_enable << 7;
            return v;


        case R_IPCSYNC+0:
            return io.ipc.arm7sync.dinput;
        case R_IPCSYNC+1:
            v = io.ipc.arm7sync.doutput;
            v |= io.ipc.arm7sync.enable_irq_from_remote << 6;
            return v;
        case R_IPCSYNC+2:
        case R_IPCSYNC+3:
            return 0;

        case R_IME: return io.arm7.IME;
        case R_IME+1: return 0;
        case R_IME+2: return 0;
        case R_IME+3: return 0;
        case R_IE+0: return io.arm7.IE & 0xFF;
        case R_IE+1: return (io.arm7.IE >> 8) & 0xFF;
        case R_IE+2: return (io.arm7.IE >> 16) & 0xFF;
        case R_IE+3: return (io.arm7.IE >> 24) & 0xFF;
        case R_IF+0: return io.arm7.IF & 0xFF;
        case R_IF+1: return (io.arm7.IF >> 8) & 0xFF;
        case R_IF+2: return (io.arm7.IF >> 16) & 0xFF;
        case R_IF+3: return (io.arm7.IF >> 24) & 0xFF;

        case R_DMA0SAD+0: return dma9[0].io.src_addr & 0xFF;
        case R_DMA0SAD+1: return (dma9[0].io.src_addr >> 8) & 0xFF;
        case R_DMA0SAD+2: return (dma9[0].io.src_addr >> 16) & 0xFF;
        case R_DMA0SAD+3: return (dma9[0].io.src_addr >> 24) & 0xFF;
        case R_DMA0DAD+0: return dma9[0].io.dest_addr & 0xFF;
        case R_DMA0DAD+1: return (dma9[0].io.dest_addr >> 8) & 0xFF;
        case R_DMA0DAD+2: return (dma9[0].io.dest_addr >> 16) & 0xFF;
        case R_DMA0DAD+3: return (dma9[0].io.dest_addr >> 24) & 0xFF;

        case R_DMA1SAD+0: return dma9[1].io.src_addr & 0xFF;
        case R_DMA1SAD+1: return (dma9[1].io.src_addr >> 8) & 0xFF;
        case R_DMA1SAD+2: return (dma9[1].io.src_addr >> 16) & 0xFF;
        case R_DMA1SAD+3: return (dma9[1].io.src_addr >> 24) & 0xFF;
        case R_DMA1DAD+0: return dma9[1].io.dest_addr & 0xFF;
        case R_DMA1DAD+1: return (dma9[1].io.dest_addr >> 8) & 0xFF;
        case R_DMA1DAD+2: return (dma9[1].io.dest_addr >> 16) & 0xFF;
        case R_DMA1DAD+3: return (dma9[1].io.dest_addr >> 24) & 0xFF;

        case R_DMA2SAD+0: return dma9[2].io.src_addr & 0xFF;
        case R_DMA2SAD+1: return (dma9[2].io.src_addr >> 8) & 0xFF;
        case R_DMA2SAD+2: return (dma9[2].io.src_addr >> 16) & 0xFF;
        case R_DMA2SAD+3: return (dma9[2].io.src_addr >> 24) & 0xFF;
        case R_DMA2DAD+0: return dma9[2].io.dest_addr & 0xFF;
        case R_DMA2DAD+1: return (dma9[2].io.dest_addr >> 8) & 0xFF;
        case R_DMA2DAD+2: return (dma9[2].io.dest_addr >> 16) & 0xFF;
        case R_DMA2DAD+3: return (dma9[2].io.dest_addr >> 24) & 0xFF;

        case R_DMA3SAD+0: return dma9[3].io.src_addr & 0xFF;
        case R_DMA3SAD+1: return (dma9[3].io.src_addr >> 8) & 0xFF;
        case R_DMA3SAD+2: return (dma9[3].io.src_addr >> 16) & 0xFF;
        case R_DMA3SAD+3: return (dma9[3].io.src_addr >> 24) & 0xFF;
        case R_DMA3DAD+0: return dma9[3].io.dest_addr & 0xFF;
        case R_DMA3DAD+1: return (dma9[3].io.dest_addr >> 8) & 0xFF;
        case R_DMA3DAD+2: return (dma9[3].io.dest_addr >> 16) & 0xFF;
        case R_DMA3DAD+3: return (dma9[3].io.dest_addr >> 24) & 0xFF;

        case R_DMA0CNT_L+0: return dma9[0].io.word_count & 0xFF;
        case R_DMA0CNT_L+1: return (dma9[0].io.word_count >> 8) & 0xFF;
        case R_DMA1CNT_L+0: return dma9[1].io.word_count & 0xFF;
        case R_DMA1CNT_L+1: return (dma9[1].io.word_count >> 8) & 0xFF;
        case R_DMA2CNT_L+0: return dma9[2].io.word_count & 0xFF;
        case R_DMA2CNT_L+1: return (dma9[2].io.word_count >> 8) & 0xFF;
        case R_DMA3CNT_L+0: return dma9[3].io.word_count & 0xFF;
        case R_DMA3CNT_L+1: return (dma9[3].io.word_count >> 8) & 0xFF;

        case R_DMA0CNT_H+0:
        case R_DMA1CNT_H+0:
        case R_DMA2CNT_H+0:
        case R_DMA3CNT_H+0: {
            DMA_ch *ch = &dma9[DMA_CH_NUM(addr)];
            v = ch->io.dest_addr_ctrl << 5;
            v |= (ch->io.src_addr_ctrl & 1) << 7;
            return v; }

        case R_DMA0CNT_H+1:
        case R_DMA1CNT_H+1:
        case R_DMA2CNT_H+1:
        case R_DMA3CNT_H+1: {
            u32 chnum = DMA_CH_NUM(addr);
            DMA_ch *ch = &dma9[chnum];
            v = ch->io.src_addr_ctrl = (ch->io.src_addr_ctrl >> 1) & 1;
            v |= ch->io.repeat << 1;
            v |= ch->io.transfer_size << 2;
            v |= ch->io.start_timing << 4;
            v |= ch->io.irq_on_end << 6;

            v |= ch->io.enable << 7;
            return v;}

        case R_TM0CNT_L+0: return (timer7[0].read() >> 0) & 0xFF;
        case R_TM0CNT_L+1: return (timer7[0].read() >> 8) & 0xFF;
        case R_TM1CNT_L+0: return (timer7[1].read() >> 0) & 0xFF;
        case R_TM1CNT_L+1: return (timer7[1].read() >> 8) & 0xFF;
        case R_TM2CNT_L+0: return (timer7[2].read() >> 0) & 0xFF;
        case R_TM2CNT_L+1: return (timer7[2].read() >> 8) & 0xFF;
        case R_TM3CNT_L+0: return (timer7[3].read() >> 0) & 0xFF;
        case R_TM3CNT_L+1: return (timer7[3].read() >> 8) & 0xFF;

        case R_TM0CNT_H+1: // TIMERCNT upper, not used.
        case R_TM1CNT_H+1:
        case R_TM2CNT_H+1:
        case R_TM3CNT_H+1:
            return 0;

        case R_TM0CNT_H+0:
        case R_TM1CNT_H+0:
        case R_TM2CNT_H+0:
        case R_TM3CNT_H+0: {
            u32 tn = (addr >> 2) & 3;
            v = timer7[tn].divider.io;
            v |= timer7[tn].cascade << 2;
            v |= timer7[tn].irq_on_overflow << 6;
            v |= timer7[tn].enabled() << 7;
            return v;
        }

        case R7_VRAMSTAT:
            v = mem.vram.io.bank[NVC].enable && (mem.vram.io.bank[NVC].mst == 2);
            v |= (mem.vram.io.bank[NVD].enable && (mem.vram.io.bank[NVD].mst == 2)) << 1;
            return v;
        case R7_WRAMSTAT:
            return mem.io.RAM9.val;
        case R7_EXMEMSTAT+0:
            return (io.arm7.EXMEM & 0x7F) | (io.arm9.EXMEM & 0x80);
        case R7_EXMEMSTAT+1:
            return (io.arm9.EXMEM >> 8) | (1 << 5);

        case 0x04004008: // DSi stuff
        case 0x04004009:
        case 0x0400400a:
        case 0x0400400b:
        case 0x04004700:
        case 0x04004701:
            return 0;
    }
    //printf("\nUnhandled BUSRD7IO8 addr:%08x", addr);
    return 0;
}

void core::start_div()
{
    // Set time and needs calculation
    io.div.needs_calc = 1;
    u64 num_clks = 20;
    switch(io.div.mode) {
        case 0:
            num_clks = 18;
        case 1:
        case 2:
            num_clks = 34;
    }
    io.div.busy_until = clock.current9() + num_clks;
}

void core::start_sqrt()
{
    io.sqrt.needs_calc = 1;
    io.div.busy_until = clock.current9() + 13;
}

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4146) // unary minus operator applied to unsigned type, result still unsigned
#endif

void core::div_calc()
{
    io.div.needs_calc = 0;

    switch (io.div.mode) {
        case 0: {
            i32 num = static_cast<i32>(io.div.numer.data32[0]);
            i32 den = static_cast<i32>(io.div.denom.data32[0]);
            if (den == 0) {
                io.div.result.data32[0] = (num<0) ? 1:-1;
                io.div.result.data32[1] = (num<0) ? -1:0;
                io.div.remainder.u = num;
            }
            else if (num == -0x80000000 && den == -1) {
                io.div.result.u = 0x80000000;
            }
            else {
                io.div.result.u = static_cast<i64>(num / den);
                io.div.remainder.u = static_cast<i64>(num % den);
            }
            break; }

        case 1:
        case 3: {
            i64 num = static_cast<i64>(io.div.numer.u);
            i32 den = static_cast<i32>(io.div.denom.data32[0]);
            if (den == 0) {
                io.div.result.u = (num<0) ? 1:-1;
                io.div.remainder.u = num;
            }
            else if (num == -0x8000000000000000 && den == -1) {
                io.div.result.u = 0x8000000000000000;
                io.div.remainder.u = 0;
            }
            else {
                io.div.result.u = (i64)(num / den);
                io.div.remainder.u = (i64)(num % den);
            }
            break; }

        case 2: {
            i64 num = static_cast<i64>(io.div.numer.u);
            i64 den = static_cast<i64>(io.div.denom.u);
            if (den == 0) {
                io.div.result.u = (num<0) ? 1:-1;
                io.div.remainder.u = num;
            }
            else if (num == -0x8000000000000000 && den == -1) {
                io.div.result.u = 0x8000000000000000;
                io.div.remainder.u = 0;
            }
            else {
                io.div.result.u = (i64)(num / den);
                io.div.remainder.u = (i64)(num % den);
            }
            break; }
    }

    io.div.by_zero |= io.div.denom.u == 0;
}

#ifdef _MSC_VER
#pragma warning(pop)
#endif

void core::sqrt_calc()
{
    io.sqrt.needs_calc = 0;
    u64 val;
    u32 res = 0;
    u64 rem = 0;
    u32 prod = 0;
    u32 nbits, topshift;

    if (io.sqrt.mode)
    {
        val = io.sqrt.param.u;
        nbits = 32;
        topshift = 62;
    }
    else
    {
        val = static_cast<u64>(io.sqrt.param.data32[0]);
        nbits = 16;
        topshift = 30;
    }

    for (u32 i = 0; i < nbits; i++)
    {
        rem = (rem << 2) + ((val >> topshift) & 0x3);
        val <<= 2;
        res <<= 1;

        prod = (res << 1) + 1;
        if (rem >= prod)
        {
            rem -= prod;
            res++;
        }
    }

    io.sqrt.result.u = res;
}

void core::buswr7_io8(u32 addr, u8 sz, u32 access, u32 val)
{
    switch(addr) {
        case R_RCNT+0:
            io.sio_data = (io.sio_data & 0xFF00) | val;
            return;
        case R_RCNT+1:
            io.sio_data = (io.sio_data & 0xFF) | (val << 8);
            return;
        case R_ROMCMD+0:
        case R_ROMCMD+1:
        case R_ROMCMD+2:
        case R_ROMCMD+3:
        case R_ROMCMD+4:
        case R_ROMCMD+5:
        case R_ROMCMD+6:
        case R_ROMCMD+7:
            if (!io.rights.nds_slot_is7) return;
            cart.write_cmd(addr - R_ROMCMD, val);
            return;

        case R7_SPICNT+0:
            spi.cnt.u = (spi.cnt.u & 0xFF80) | (val & 0b00000011);
            return;
        case R7_SPICNT+1: {
            if ((val & 0x80) && (!spi.cnt.bus_enable)) {
                // Enabling the bus releases hold on current device
                spi.chipsel = 0;
                SPI_release_hold();
            }
            // Don't change device while chipsel is hi?
            //if (spi.cnt.chipselect_hold)
            if (false)
                val = (val & 0b11111100) | spi.cnt.device;

            spi.cnt.u = (spi.cnt.u & 0xFF) | ((val & 0b11001111) << 8);
            return; }

        case R_POSTFLG:
            io.arm7.POSTFLG |= val & 1;
            return;

        case R7_WIFIWAITCNT+0:
            if (io.powcnt.wifi)
                io.powcnt.wifi_waitcnt = val;
            return;
        case R7_WIFIWAITCNT+1:
            return;
        case R7_HALTCNT+0: {
            u32 v = (val >> 6) & 3;
            switch(v) {
                case 0:
                    return;
                case 1:
                    printf("\nWARNING GBA MODE ATTEMPT");
                    return;
                case 2:
                    dbgloglog(NDS_CAT_ARM7_HALT, DBGLS_INFO, "HALT ARM7 cyc:%lld", clock.current7());
                    io.arm7.halted = 1;
                    return;
                case 3:
                    printf("\nWARNING SLEEP MODE ATTEMPT");
                    return;
            }
            return; }

        case R7_POWCNT2+0:
            io.powcnt.speakers = val & 1;
            io.powcnt.wifi = (val >> 1) & 1;
            return;
        case R7_POWCNT2+1:
            return;

        case R_KEYCNT+0:
            io.arm7.button_irq.buttons = (io.arm7.button_irq.buttons & 0b1100000000) | val;
            return;
        case R_KEYCNT+1: {
            io.arm7.button_irq.buttons = (io.arm7.button_irq.buttons & 0xFF) | ((val & 0b11) << 8);
            u32 old_enable = io.arm7.button_irq.enable;
            io.arm7.button_irq.enable = (val >> 6) & 1;
            if ((old_enable == 0) && io.arm7.button_irq.enable) {
                printf("\nWARNING BUTTON IRQ ENABLED ARM9...");
            }
            io.arm7.button_irq.condition = (val >> 7) & 1;
            return; }

        case R_IPCFIFOCNT+0: {
            u32 old_bits = io.ipc.arm7.irq_on_send_fifo_empty & io.ipc.to_arm9.is_empty();
            io.ipc.arm7.irq_on_send_fifo_empty = (val >> 2) & 1;
            if ((val >> 3) & 1) { // arm7's send fifo is to_arm9
                io.ipc.to_arm9.empty();
            }
            // Edge-sensitive trigger...
            if (io.ipc.arm7.irq_on_send_fifo_empty & !old_bits) {
                update_IF7(IRQ_IPC_SEND_EMPTY);
            }
            return; }
        case R_IPCFIFOCNT+1: {
            u32 old_bits = io.ipc.arm7.irq_on_recv_fifo_not_empty & io.ipc.to_arm7.is_not_empty();
            io.ipc.arm7.irq_on_recv_fifo_not_empty = (val >> 2) & 1;
            if ((val >> 6) & 1) io.ipc.arm7.error = 0;
            io.ipc.arm7.fifo_enable = (val >> 7) & 1;
            u32 new_bits = io.ipc.arm7.irq_on_recv_fifo_not_empty & io.ipc.to_arm7.is_not_empty();
            if (!old_bits && new_bits) {
                update_IF7(IRQ_IPC_RECV_NOT_EMPTY);
            }
            return; }

        case R_IPCSYNC+0:
        case R_IPCSYNC+2:
        case R_IPCSYNC+3:
            return;
        case R_IPCSYNC+1: {
            io.ipc.arm9sync.dinput = io.ipc.arm7sync.doutput = val & 15;

            u32 send_irq = (val >> 5) & 1;
            if (send_irq) printf("\nIPC IRQ REQUEST!");

            if (send_irq && io.ipc.arm9sync.enable_irq_from_remote) {
                update_IF9(IRQ_IPC_SYNC);
            }
            io.ipc.arm7sync.enable_irq_from_remote = (val >> 6) & 1;
            return; }

        case R_IME: io.arm7.IME = val & 1; eval_irqs_7(); return;
        case R_IME+1: return;
        case R_IME+2: return;
        case R_IME+3: return;
        case R_IF+0: io.arm7.IF &= ~val; eval_irqs_7(); return;
        case R_IF+1: io.arm7.IF &= ~(val << 8); eval_irqs_7(); return;
        case R_IF+2: io.arm7.IF &= ~(val << 16); eval_irqs_7(); return;
        case R_IF+3: io.arm7.IF &= ~(val << 24); eval_irqs_7(); return;

        case R_IE+0:
            io.arm7.IE = (io.arm7.IE & 0xFF00) | (val & 0xFF);
            io.arm7.IE &= ~0x80; // bit 7 doesn't get set on ARM9
            eval_irqs_7();
            return;
        case R_IE+1:
            io.arm7.IE = (io.arm7.IE & 0xFF) | (val << 8);
            eval_irqs_7();
            return;
        case R_IE+2:
            io.arm7.IE = (io.arm7.IE & 0xFF00FFFF) | (val << 16);
            eval_irqs_7();
            return;
        case R_IE+3:
            io.arm7.IE = (io.arm7.IE & 0x00FFFFFF) | (val << 24);
            eval_irqs_7();
            return;

        case R_DMA0SAD+0: dma7[0].io.src_addr = (dma7[0].io.src_addr & 0xFFFFFF00) | (val << 0); return; // DMA source address ch0
        case R_DMA0SAD+1: dma7[0].io.src_addr = (dma7[0].io.src_addr & 0xFFFF00FF) | (val << 8); return; // DMA source address ch0
        case R_DMA0SAD+2: dma7[0].io.src_addr = (dma7[0].io.src_addr & 0xFF00FFFF) | (val << 16); return; // DMA source address ch0
        case R_DMA0SAD+3: dma7[0].io.src_addr = (dma7[0].io.src_addr & 0x00FFFFFF) | ((val & 0x07) << 24); return; // DMA source address ch0
        case R_DMA0DAD+0: dma7[0].io.dest_addr = (dma7[0].io.dest_addr & 0xFFFFFF00) | (val << 0); return; // DMA source address ch0
        case R_DMA0DAD+1: dma7[0].io.dest_addr = (dma7[0].io.dest_addr & 0xFFFF00FF) | (val << 8); return; // DMA source address ch0
        case R_DMA0DAD+2: dma7[0].io.dest_addr = (dma7[0].io.dest_addr & 0xFF00FFFF) | (val << 16); return; // DMA source address ch0
        case R_DMA0DAD+3: dma7[0].io.dest_addr = (dma7[0].io.dest_addr & 0x00FFFFFF) | ((val & 0x07) << 24); return; // DMA source address ch0

        case R_DMA1SAD+0: dma7[1].io.src_addr = (dma7[1].io.src_addr & 0xFFFFFF00) | (val << 0); return; // DMA source address ch0
        case R_DMA1SAD+1: dma7[1].io.src_addr = (dma7[1].io.src_addr & 0xFFFF00FF) | (val << 8); return; // DMA source address ch0
        case R_DMA1SAD+2: dma7[1].io.src_addr = (dma7[1].io.src_addr & 0xFF00FFFF) | (val << 16); return; // DMA source address ch0
        case R_DMA1SAD+3: dma7[1].io.src_addr = (dma7[1].io.src_addr & 0x00FFFFFF) | ((val & 0x0F) << 24); return; // DMA source address ch0
        case R_DMA1DAD+0: dma7[1].io.dest_addr = (dma7[1].io.dest_addr & 0xFFFFFF00) | (val << 0); return; // DMA source address ch0
        case R_DMA1DAD+1: dma7[1].io.dest_addr = (dma7[1].io.dest_addr & 0xFFFF00FF) | (val << 8); return; // DMA source address ch0
        case R_DMA1DAD+2: dma7[1].io.dest_addr = (dma7[1].io.dest_addr & 0xFF00FFFF) | (val << 16); return; // DMA source address ch0
        case R_DMA1DAD+3: dma7[1].io.dest_addr = (dma7[1].io.dest_addr & 0x00FFFFFF) | ((val & 0x07) << 24); return; // DMA source address ch0

        case R_DMA2SAD+0: dma7[2].io.src_addr = (dma7[2].io.src_addr & 0xFFFFFF00) | (val << 0); return; // DMA source address ch0
        case R_DMA2SAD+1: dma7[2].io.src_addr = (dma7[2].io.src_addr & 0xFFFF00FF) | (val << 8); return; // DMA source address ch0
        case R_DMA2SAD+2: dma7[2].io.src_addr = (dma7[2].io.src_addr & 0xFF00FFFF) | (val << 16); return; // DMA source address ch0
        case R_DMA2SAD+3: dma7[2].io.src_addr = (dma7[2].io.src_addr & 0x00FFFFFF) | ((val & 0x0F) << 24); return; // DMA source address ch0
        case R_DMA2DAD+0: dma7[2].io.dest_addr = (dma7[2].io.dest_addr & 0xFFFFFF00) | (val << 0); return; // DMA source address ch0
        case R_DMA2DAD+1: dma7[2].io.dest_addr = (dma7[2].io.dest_addr & 0xFFFF00FF) | (val << 8); return; // DMA source address ch0
        case R_DMA2DAD+2: dma7[2].io.dest_addr = (dma7[2].io.dest_addr & 0xFF00FFFF) | (val << 16); return; // DMA source address ch0
        case R_DMA2DAD+3: dma7[2].io.dest_addr = (dma7[2].io.dest_addr & 0x00FFFFFF) | ((val & 0x07) << 24); return; // DMA source address ch0

        case R_DMA3SAD+0: dma7[3].io.src_addr = (dma7[3].io.src_addr & 0xFFFFFF00) | (val << 0); return; // DMA source address ch0
        case R_DMA3SAD+1: dma7[3].io.src_addr = (dma7[3].io.src_addr & 0xFFFF00FF) | (val << 8); return; // DMA source address ch0
        case R_DMA3SAD+2: dma7[3].io.src_addr = (dma7[3].io.src_addr & 0xFF00FFFF) | (val << 16); return; // DMA source address ch0
        case R_DMA3SAD+3: dma7[3].io.src_addr = (dma7[3].io.src_addr & 0x00FFFFFF) | ((val & 0x0F) << 24); return; // DMA source address ch0
        case R_DMA3DAD+0: dma7[3].io.dest_addr = (dma7[3].io.dest_addr & 0xFFFFFF00) | (val << 0); return; // DMA source address ch0
        case R_DMA3DAD+1: dma7[3].io.dest_addr = (dma7[3].io.dest_addr & 0xFFFF00FF) | (val << 8); return; // DMA source address ch0
        case R_DMA3DAD+2: dma7[3].io.dest_addr = (dma7[3].io.dest_addr & 0xFF00FFFF) | (val << 16); return; // DMA source address ch0
        case R_DMA3DAD+3: dma7[3].io.dest_addr = (dma7[3].io.dest_addr & 0x00FFFFFF) | ((val & 0x0F) << 24); return; // DMA source address ch0

        case R_DMA0CNT_L+0: dma7[0].io.word_count = (dma7[0].io.word_count & 0x3F00) | (val << 0); return;
        case R_DMA0CNT_L+1: dma7[0].io.word_count = (dma7[0].io.word_count & 0xFF) | ((val & 0x3F) << 8); return;
        case R_DMA1CNT_L+0: dma7[1].io.word_count = (dma7[1].io.word_count & 0x3F00) | (val << 0); return;
        case R_DMA1CNT_L+1: dma7[1].io.word_count = (dma7[1].io.word_count & 0xFF) | ((val & 0x3F) << 8); return;
        case R_DMA2CNT_L+0: dma7[2].io.word_count = (dma7[2].io.word_count & 0x3F00) | (val << 0); return;
        case R_DMA2CNT_L+1: dma7[2].io.word_count = (dma7[2].io.word_count & 0xFF) | ((val & 0x3F) << 8); return;
        case R_DMA3CNT_L+0: dma7[3].io.word_count = (dma7[3].io.word_count & 0xFF00) | (val << 0); return;
        case R_DMA3CNT_L+1: dma7[3].io.word_count = (dma7[3].io.word_count & 0xFF) | ((val & 0xFF) << 8); return;

        case R_DMA0CNT_H+0:
        case R_DMA1CNT_H+0:
        case R_DMA2CNT_H+0:
        case R_DMA3CNT_H+0: {
            DMA_ch *ch = &dma7[DMA_CH_NUM(addr)];
            ch->io.dest_addr_ctrl = (val >> 5) & 3;
            ch->io.src_addr_ctrl = (ch->io.src_addr_ctrl & 2) | ((val >> 7) & 1);
            return;}

        case R_DMA0CNT_H+1:
        case R_DMA1CNT_H+1:
        case R_DMA2CNT_H+1:
        case R_DMA3CNT_H+1: {
            u32 chnum = DMA_CH_NUM(addr);
            DMA_ch &ch = dma7[chnum];
            ch.io.src_addr_ctrl = (ch.io.src_addr_ctrl & 1) | ((val & 1) << 1);
            ch.io.repeat = (val >> 1) & 1;
            ch.io.transfer_size = (val >> 2) & 1;
            ch.io.start_timing = (val >> 4) & 3;
            if (ch.io.start_timing >= 2) {
                printf("\nWARN START TIMING %d NOT IMPLEMENT FOR ARM7 DMA!", ch.io.start_timing);
            }
            ch.io.irq_on_end = (val >> 6) & 1;
            u32 old_enable = ch.io.enable;
            ch.io.enable = (val >> 7) & 1;
            if ((ch.io.enable == 1) && (old_enable == 0)) {
                ch.op.first_run = 1;
                if (ch.io.start_timing == 0) {
                    dma7_start(ch, chnum);
                }
            }
            return;}

        case R_TM0CNT_H+1:
        case R_TM1CNT_H+1:
        case R_TM2CNT_H+1:
        case R_TM3CNT_H+1:
            return;

        case R_TM0CNT_H+0:
        case R_TM1CNT_H+0:
        case R_TM2CNT_H+0:
        case R_TM3CNT_H+0: {
            u32 tn = (addr >> 2) & 3;
            timer7[tn].write_cnt(val);
            return; }

        case R_TM0CNT_L+0: timer7[0].reload = (timer7[0].reload & 0xFF00) | val; return;
        case R_TM1CNT_L+0: timer7[1].reload = (timer7[1].reload & 0xFF00) | val; return;
        case R_TM2CNT_L+0: timer7[2].reload = (timer7[2].reload & 0xFF00) | val; return;
        case R_TM3CNT_L+0: timer7[3].reload = (timer7[3].reload & 0xFF00) | val; return;

        case R_TM0CNT_L+1: timer7[0].reload = (timer7[0].reload & 0xFF) | (val << 8); return;
        case R_TM1CNT_L+1: timer7[1].reload = (timer7[1].reload & 0xFF) | (val << 8); return;
        case R_TM2CNT_L+1: timer7[2].reload = (timer7[2].reload & 0xFF) | (val << 8); return;
        case R_TM3CNT_L+1: timer7[3].reload = (timer7[3].reload & 0xFF) | (val << 8); return;

        case R7_BIOSPROT+0:
            io.arm7.BIOSPROT = (io.arm7.BIOSPROT & 0xFF00) | val;
            return;
        case R7_BIOSPROT+1:
            io.arm7.BIOSPROT = (io.arm7.BIOSPROT & 0xFF) | (val << 8);
            return;
        case R7_EXMEMSTAT+0:
            io.arm7.EXMEM = val & 0x7F;
            return;
        case R7_EXMEMSTAT+1:
            return;

    }
    printf("\nUnhandled BUSWR7IO8 addr:%08x val:%08x", addr, val);
}

// --------------
u32 core::busrd9_io8(u32 addr, u8 sz, u32 access, bool has_effect)
{
    if (((addr >= 0x04000000) && (addr < 0x04000070)) || ((addr >= 0x04001000) && (addr < 0x04001070))) {
        return ppu.read9_io(addr, sz, access, has_effect);
    }
    u32 v;
    switch(addr) {
        case R_AUXSPICNT:
            if (io.rights.nds_slot_is7) return 0;
            return cart.read_spicnt() & 0xFF;
        case R_AUXSPICNT+1:
            if (io.rights.nds_slot_is7) return 0;
            return cart.read_spicnt() >> 8;
        case R9_POWCNT1+0:
            v = io.powcnt.lcd_enable;
            v |= ppu.eng2d[0].enable << 1;
            v |= re.enable << 2;
            v |= ge.enable << 3;
            return v;
        case R9_POWCNT1+1:
            v = ppu.eng2d[1].enable << 1;
            v |= ppu.io.display_swap << 7;
            return v;

        case R_KEYINPUT+0: // buttons!!!
            return controller.get_state(0);
        case R_KEYINPUT+1: // buttons!!!
            return controller.get_state(1);
        /*case R_EXTKEYIN+0:
            return controller.get_state(2);
        case R_EXTKEYIN+1:
            return 0;*/

        case R_IPCFIFOCNT+0:
            // send fifo from 9 is to_7
            v = io.ipc.to_arm7.is_empty();
            v |= io.ipc.to_arm7.is_full() << 1;
            printf("\nFIFO7 EMPTY:%d FULL:%d?", v & 1, (v >> 1));
            v |= io.ipc.arm9.irq_on_send_fifo_empty << 2;
            return v;
        case R_IPCFIFOCNT+1:
            v = io.ipc.to_arm9.is_empty();
            v |= io.ipc.to_arm9.is_full() << 1;
            printf("\nFIFO9 EMPTY:%d FULL:%d", v & 1, (v >> 1));
            v |= io.ipc.arm9.irq_on_recv_fifo_not_empty << 2;
            v |= io.ipc.arm9.error << 6;
            v |= io.ipc.arm9.fifo_enable << 7;
            return v;

        case R_IPCSYNC+0:
            return io.ipc.arm9sync.dinput;
        case R_IPCSYNC+1:
            v = io.ipc.arm9sync.doutput;
            v |= io.ipc.arm9sync.enable_irq_from_remote << 6;
            return v;
        case R_IPCSYNC+2:
        case R_IPCSYNC+3:
            return 0;

        case R9_DIVCNT+0:
            v = io.div.mode;
            return v;
        case R9_DIVCNT+1:
            v = io.div.by_zero << 6;
            v |= (clock.current9() < io.div.busy_until) << 7;
            return v;

        case R9_DIV_NUMER+0:
        case R9_DIV_NUMER+1:
        case R9_DIV_NUMER+2:
        case R9_DIV_NUMER+3:
        case R9_DIV_NUMER+4:
        case R9_DIV_NUMER+5:
        case R9_DIV_NUMER+6:
        case R9_DIV_NUMER+7:
            return io.div.numer.data[addr & 7];

        case R9_DIV_DENOM+0:
        case R9_DIV_DENOM+1:
        case R9_DIV_DENOM+2:
        case R9_DIV_DENOM+3:
        case R9_DIV_DENOM+4:
        case R9_DIV_DENOM+5:
        case R9_DIV_DENOM+6:
        case R9_DIV_DENOM+7:
            return io.div.denom.data[addr & 7];

        case R9_DIV_RESULT+0:
        case R9_DIV_RESULT+1:
        case R9_DIV_RESULT+2:
        case R9_DIV_RESULT+3:
        case R9_DIV_RESULT+4:
        case R9_DIV_RESULT+5:
        case R9_DIV_RESULT+6:
        case R9_DIV_RESULT+7:
            if (io.div.needs_calc) div_calc();
            return io.div.result.data[addr & 7];

        case R9_DIVREM_RESULT+0:
        case R9_DIVREM_RESULT+1:
        case R9_DIVREM_RESULT+2:
        case R9_DIVREM_RESULT+3:
        case R9_DIVREM_RESULT+4:
        case R9_DIVREM_RESULT+5:
        case R9_DIVREM_RESULT+6:
        case R9_DIVREM_RESULT+7:
            if (io.div.needs_calc) div_calc();
            return io.div.remainder.data[addr & 7];

        case R9_SQRTCNT+0:
            return io.sqrt.mode;
        case R9_SQRTCNT+1:
            return 0;

        case R9_SQRT_PARAM+0:
        case R9_SQRT_PARAM+1:
        case R9_SQRT_PARAM+2:
        case R9_SQRT_PARAM+3:
        case R9_SQRT_PARAM+4:
        case R9_SQRT_PARAM+5:
        case R9_SQRT_PARAM+6:
        case R9_SQRT_PARAM+7:
            return io.sqrt.param.data[addr & 7];

        case R9_SQRT_RESULT+0:
        case R9_SQRT_RESULT+1:
        case R9_SQRT_RESULT+2:
        case R9_SQRT_RESULT+3:
            if (io.sqrt.needs_calc) sqrt_calc();
            return io.sqrt.result.data[addr & 3];

        case R_IME: return io.arm9.IME;
        case R_IME+1: return 0;
        case R_IME+2: return 0;
        case R_IME+3: return 0;
        case R_IE+0: return io.arm9.IE & 0xFF;
        case R_IE+1: return (io.arm9.IE >> 8) & 0xFF;
        case R_IE+2: return (io.arm9.IE >> 16) & 0xFF;
        case R_IE+3: return (io.arm9.IE >> 24) & 0xFF;
        case R_IF+0: return io.arm9.IF & 0xFF;
        case R_IF+1: return (io.arm9.IF >> 8) & 0xFF;
        case R_IF+2: return (io.arm9.IF >> 16) & 0xFF;
        case R_IF+3: return (io.arm9.IF >> 24) & 0xFF;

        case R9_WRAMCNT:
            return mem.io.RAM9.val;
        case R9_EXMEMCNT+0:
            return io.arm9.EXMEM & 0xFF;
        case R9_EXMEMCNT+1:
            return (io.arm9.EXMEM >> 8) | (1 << 5);

        case R_DMA0SAD+0: return dma9[0].io.src_addr & 0xFF;
        case R_DMA0SAD+1: return (dma9[0].io.src_addr >> 8) & 0xFF;
        case R_DMA0SAD+2: return (dma9[0].io.src_addr >> 16) & 0xFF;
        case R_DMA0SAD+3: return (dma9[0].io.src_addr >> 24) & 0xFF;
        case R_DMA0DAD+0: return dma9[0].io.dest_addr & 0xFF;
        case R_DMA0DAD+1: return (dma9[0].io.dest_addr >> 8) & 0xFF;
        case R_DMA0DAD+2: return (dma9[0].io.dest_addr >> 16) & 0xFF;
        case R_DMA0DAD+3: return (dma9[0].io.dest_addr >> 24) & 0xFF;

        case R_DMA1SAD+0: return dma9[1].io.src_addr & 0xFF;
        case R_DMA1SAD+1: return (dma9[1].io.src_addr >> 8) & 0xFF;
        case R_DMA1SAD+2: return (dma9[1].io.src_addr >> 16) & 0xFF;
        case R_DMA1SAD+3: return (dma9[1].io.src_addr >> 24) & 0xFF;
        case R_DMA1DAD+0: return dma9[1].io.dest_addr & 0xFF;
        case R_DMA1DAD+1: return (dma9[1].io.dest_addr >> 8) & 0xFF;
        case R_DMA1DAD+2: return (dma9[1].io.dest_addr >> 16) & 0xFF;
        case R_DMA1DAD+3: return (dma9[1].io.dest_addr >> 24) & 0xFF;

        case R_DMA2SAD+0: return dma9[2].io.src_addr & 0xFF;
        case R_DMA2SAD+1: return (dma9[2].io.src_addr >> 8) & 0xFF;
        case R_DMA2SAD+2: return (dma9[2].io.src_addr >> 16) & 0xFF;
        case R_DMA2SAD+3: return (dma9[2].io.src_addr >> 24) & 0xFF;
        case R_DMA2DAD+0: return dma9[2].io.dest_addr & 0xFF;
        case R_DMA2DAD+1: return (dma9[2].io.dest_addr >> 8) & 0xFF;
        case R_DMA2DAD+2: return (dma9[2].io.dest_addr >> 16) & 0xFF;
        case R_DMA2DAD+3: return (dma9[2].io.dest_addr >> 24) & 0xFF;

        case R_DMA3SAD+0: return dma9[3].io.src_addr & 0xFF;
        case R_DMA3SAD+1: return (dma9[3].io.src_addr >> 8) & 0xFF;
        case R_DMA3SAD+2: return (dma9[3].io.src_addr >> 16) & 0xFF;
        case R_DMA3SAD+3: return (dma9[3].io.src_addr >> 24) & 0xFF;
        case R_DMA3DAD+0: return dma9[3].io.dest_addr & 0xFF;
        case R_DMA3DAD+1: return (dma9[3].io.dest_addr >> 8) & 0xFF;
        case R_DMA3DAD+2: return (dma9[3].io.dest_addr >> 16) & 0xFF;
        case R_DMA3DAD+3: return (dma9[3].io.dest_addr >> 24) & 0xFF;

        case R_DMA0CNT_L+0: return dma9[0].io.word_count & 0xFF;
        case R_DMA0CNT_L+1: return (dma9[0].io.word_count >> 8) & 0xFF;
        case R_DMA1CNT_L+0: return dma9[1].io.word_count & 0xFF;
        case R_DMA1CNT_L+1: return (dma9[1].io.word_count >> 8) & 0xFF;
        case R_DMA2CNT_L+0: return dma9[2].io.word_count & 0xFF;
        case R_DMA2CNT_L+1: return (dma9[2].io.word_count >> 8) & 0xFF;
        case R_DMA3CNT_L+0: return dma9[3].io.word_count & 0xFF;
        case R_DMA3CNT_L+1: return (dma9[3].io.word_count >> 8) & 0xFF;

        case R_DMA0CNT_H+0:
        case R_DMA1CNT_H+0:
        case R_DMA2CNT_H+0:
        case R_DMA3CNT_H+0: {
            DMA_ch &ch = dma9[DMA_CH_NUM(addr)];
            v = ch.io.word_count >> 16;
            v |= ch.io.dest_addr_ctrl << 5;
            v |= (ch.io.src_addr_ctrl & 1) << 7;
            return v; }

        case R_DMA0CNT_H+1:
        case R_DMA1CNT_H+1:
        case R_DMA2CNT_H+1:
        case R_DMA3CNT_H+1: {
            u32 chnum = DMA_CH_NUM(addr);
            DMA_ch &ch = dma9[chnum];
            v = ch.io.src_addr_ctrl = (ch.io.src_addr_ctrl >> 1) & 1;
            v |= ch.io.repeat << 1;
            v |= ch.io.transfer_size << 2;
            v |= ch.io.start_timing << 3;
            v |= ch.io.irq_on_end << 6;

            v |= ch.io.enable << 7;
            return v;}

        case R9_DMAFIL+0:
        case R9_DMAFIL+1:
        case R9_DMAFIL+2:
        case R9_DMAFIL+3:
        case R9_DMAFIL+4:
        case R9_DMAFIL+5:
        case R9_DMAFIL+6:
        case R9_DMAFIL+7:
        case R9_DMAFIL+8:
        case R9_DMAFIL+9:
        case R9_DMAFIL+10:
        case R9_DMAFIL+11:
        case R9_DMAFIL+12:
        case R9_DMAFIL+13:
        case R9_DMAFIL+14:
        case R9_DMAFIL+15:
            return io.dma.filldata[addr & 15];

        case R_TM0CNT_L+0: return (timer9[0].read() >> 0) & 0xFF;
        case R_TM0CNT_L+1: return (timer9[0].read() >> 8) & 0xFF;
        case R_TM1CNT_L+0: return (timer9[1].read() >> 0) & 0xFF;
        case R_TM1CNT_L+1: return (timer9[1].read() >> 8) & 0xFF;
        case R_TM2CNT_L+0: return (timer9[2].read() >> 0) & 0xFF;
        case R_TM2CNT_L+1: return (timer9[2].read() >> 8) & 0xFF;
        case R_TM3CNT_L+0: return (timer9[3].read() >> 0) & 0xFF;
        case R_TM3CNT_L+1: return (timer9[3].read() >> 8) & 0xFF;

        case R_TM0CNT_H+1: // TIMERCNT upper, not used.
        case R_TM1CNT_H+1:
        case R_TM2CNT_H+1:
        case R_TM3CNT_H+1:
            return 0;

        case R9_VRAMCNT+0:
        case R9_VRAMCNT+1:
        case R9_VRAMCNT+2:
        case R9_VRAMCNT+3:
        case R9_VRAMCNT+4:
        case R9_VRAMCNT+5:
        case R9_VRAMCNT+6:
        case R9_VRAMCNT+8:
        case R9_VRAMCNT+9: {
            assert(sz==1);
            u32 bank_num = addr - R9_VRAMCNT;
            if (bank_num >= 8) bank_num--;

            v = mem.vram.io.bank[bank_num].mst;
            v |= mem.vram.io.bank[bank_num].ofs << 3;
            v |= mem.vram.io.bank[bank_num].enable << 7;
            return v; }

        case R_TM0CNT_H+0:
        case R_TM1CNT_H+0:
        case R_TM2CNT_H+0:
        case R_TM3CNT_H+0: {
            u32 tn = (addr >> 2) & 3;
            v = timer9[tn].divider.io;
            v |= timer9[tn].cascade << 2;
            v |= timer9[tn].irq_on_overflow << 6;
            v |= timer9[tn].enabled() << 7;
            return v;
        }

        case R9_DIVCNT+2:
        case R9_DIVCNT+3:
        case 0x04004000:
            // NDS thing
            return 0;
        case 0x04004008: // new DSi stuff libnds cares about?
        case 0x04004009:
        case 0x0400400A:
        case 0x0400400B:
        case 0x04004010:
        case 0x04004011:
        case 0x04004004:
        case 0x04004005:
            return 0;
    }
    printf("\nUnhandled BUSRD9IO8 addr:%08x", addr);
    return 0;
}

void core::buswr9_io8(u32 addr, u8 sz, u32 access, u32 val)
{
    switch(addr) {
        case R_ROMCMD+0:
        case R_ROMCMD+1:
        case R_ROMCMD+2:
        case R_ROMCMD+3:
        case R_ROMCMD+4:
        case R_ROMCMD+5:
        case R_ROMCMD+6:
        case R_ROMCMD+7:
            if (io.rights.nds_slot_is7) return;
            cart.write_cmd(addr - R_ROMCMD, val);
            return;

        case R_POSTFLG:
            io.arm9.POSTFLG |= val & 1;
            io.arm9.POSTFLG = (io.arm9.POSTFLG & 1) | (val & 2);
            return;
        case R9_POWCNT1+0:
            io.powcnt.lcd_enable = val & 1;
            ppu.eng2d[0].enable = (val >> 1) & 1;
            re.enable = (val >> 2) & 1;
            ge.enable = (val >> 3) & 1;
            return;
        case R9_POWCNT1+1:
            ppu.eng2d[1].enable = (val >> 1) & 1;
            ppu.io.display_swap = (val >> 7) & 1;
            return;
        case R9_POWCNT1+2:
        case R9_POWCNT1+3:
            return;
        case R_KEYCNT+0:
            io.arm9.button_irq.buttons = (io.arm9.button_irq.buttons & 0b1100000000) | val;
            return;
        case R_KEYCNT+1: {
            io.arm9.button_irq.buttons = (io.arm9.button_irq.buttons & 0xFF) | ((val & 0b11) << 8);
            u32 old_enable = io.arm9.button_irq.enable;
            io.arm9.button_irq.enable = (val >> 6) & 1;
            if ((old_enable == 0) && io.arm9.button_irq.enable) {
                printf("\nWARNING BUTTON IRQ ENABLED ARM9...");
            }
            io.arm9.button_irq.condition = (val >> 7) & 1;
            return; }

        case R_IPCFIFOCNT+0: {
            u32 old_bits = io.ipc.arm9.irq_on_send_fifo_empty & io.ipc.to_arm7.is_empty();
            io.ipc.arm9.irq_on_send_fifo_empty = (val >> 2) & 1;
            if ((val >> 3) & 1) { // arm9's send fifo is to_arm7
                io.ipc.to_arm7.empty();
            }
            // Edge-sensitive trigger...
            if (io.ipc.arm9.irq_on_send_fifo_empty & !old_bits) {
                update_IF9(IRQ_IPC_SEND_EMPTY);
            }
            return; }
        case R_IPCFIFOCNT+1: {
            u32 old_bits = io.ipc.arm9.irq_on_recv_fifo_not_empty & io.ipc.to_arm9.is_not_empty();
            io.ipc.arm9.irq_on_recv_fifo_not_empty = (val >> 2) & 1;
            if ((val >> 6) & 1) io.ipc.arm9.error = 0;
            io.ipc.arm9.fifo_enable = (val >> 7) & 1;
            u32 new_bits = io.ipc.arm9.irq_on_recv_fifo_not_empty & io.ipc.to_arm9.is_not_empty();
            if (!old_bits && new_bits) {
                update_IF9(IRQ_IPC_RECV_NOT_EMPTY);
            }
            return; }

        case R_IPCSYNC+0:
        case R_IPCSYNC+2:
        case R_IPCSYNC+3:
            return;
        case R_IPCSYNC+1: {
            io.ipc.arm7sync.dinput = io.ipc.arm9sync.doutput = val & 15;
            u32 send_irq = (val >> 5) & 1;
            if (send_irq && io.ipc.arm7sync.enable_irq_from_remote) {
                update_IF7(IRQ_IPC_SYNC);
            }
            io.ipc.arm9sync.enable_irq_from_remote = (val >> 6) & 1;
            return;}

        case R_IME: {
            io.arm9.IME = val & 1;
            eval_irqs_9();
            return;
        }
        case R_IME+1: return;
        case R_IME+2: return;
        case R_IME+3: return;
        case R_IF+0: io.arm9.IF &= ~val; eval_irqs_9(); return;
        case R_IF+1: io.arm9.IF &= ~(val << 8); eval_irqs_9(); return;
        case R_IF+2: io.arm9.IF &= ~(val << 16); eval_irqs_9(); return;
        case R_IF+3: io.arm9.IF &= ~(val << 24); eval_irqs_9(); return;

        case R9_DIVCNT+0:
            io.div.mode = val & 3;
            if (io.div.mode == 3) {
                printf("\nFORBIDDEN DIV MODE");
            }
            start_div();
            return;
        case R9_DIVCNT+1:
            io.div.by_zero = (val >> 6) & 1;
            start_div();
            return;

        case R9_DIV_NUMER+0:
        case R9_DIV_NUMER+1:
        case R9_DIV_NUMER+2:
        case R9_DIV_NUMER+3:
        case R9_DIV_NUMER+4:
        case R9_DIV_NUMER+5:
        case R9_DIV_NUMER+6:
        case R9_DIV_NUMER+7:
            io.div.numer.data[addr & 7] = val;
            start_div();
            return;

        case R9_DIV_DENOM+0:
        case R9_DIV_DENOM+1:
        case R9_DIV_DENOM+2:
        case R9_DIV_DENOM+3:
        case R9_DIV_DENOM+4:
        case R9_DIV_DENOM+5:
        case R9_DIV_DENOM+6:
        case R9_DIV_DENOM+7:
            io.div.denom.data[addr & 7] = val;
            start_div();
            return;

        case R9_SQRTCNT+0:
            io.sqrt.mode = val & 1;
            start_sqrt();
            return;
        case R9_SQRTCNT+1:
            start_sqrt();
            return;

        case R9_SQRT_PARAM+0:
        case R9_SQRT_PARAM+1:
        case R9_SQRT_PARAM+2:
        case R9_SQRT_PARAM+3:
        case R9_SQRT_PARAM+4:
        case R9_SQRT_PARAM+5:
        case R9_SQRT_PARAM+6:
        case R9_SQRT_PARAM+7:
            io.sqrt.param.data[addr & 7] = val;
            start_sqrt();
            return;

        case R_IE+0:
            io.arm9.IE = (io.arm9.IE & 0xFFFFFF00) | (val & 0xFF);
            io.arm9.IE &= ~0x80; // bit 7 doesn't get set on ARM9
            eval_irqs_9();
            return;
        case R_IE+1:
            io.arm9.IE = (io.arm9.IE & 0xFFFF00FF) | (val << 8);
            eval_irqs_9();
            return;
        case R_IE+2:
            io.arm9.IE = (io.arm9.IE & 0xFF00FFFF) | (val << 16);
            eval_irqs_9();
            return;
        case R_IE+3:
            io.arm9.IE = (io.arm9.IE & 0x00FFFFFF) | (val << 24);
            eval_irqs_9();
            return;

        case R_DMA0SAD+0: dma9[0].io.src_addr = (dma9[0].io.src_addr & 0xFFFFFF00) | (val << 0); return;
        case R_DMA0SAD+1: dma9[0].io.src_addr = (dma9[0].io.src_addr & 0xFFFF00FF) | (val << 8); return;
        case R_DMA0SAD+2: dma9[0].io.src_addr = (dma9[0].io.src_addr & 0xFF00FFFF) | (val << 16); return;
        case R_DMA0SAD+3: dma9[0].io.src_addr = (dma9[0].io.src_addr & 0x00FFFFFF) | ((val & 0x0F) << 24); return;
        case R_DMA0DAD+0: dma9[0].io.dest_addr = (dma9[0].io.dest_addr & 0xFFFFFF00) | (val << 0); return;
        case R_DMA0DAD+1: dma9[0].io.dest_addr = (dma9[0].io.dest_addr & 0xFFFF00FF) | (val << 8); return;
        case R_DMA0DAD+2: dma9[0].io.dest_addr = (dma9[0].io.dest_addr & 0xFF00FFFF) | (val << 16); return;
        case R_DMA0DAD+3: dma9[0].io.dest_addr = (dma9[0].io.dest_addr & 0x00FFFFFF) | ((val & 0x0F) << 24); return;

        case R_DMA1SAD+0: dma9[1].io.src_addr = (dma9[1].io.src_addr & 0xFFFFFF00) | (val << 0); return;
        case R_DMA1SAD+1: dma9[1].io.src_addr = (dma9[1].io.src_addr & 0xFFFF00FF) | (val << 8); return;
        case R_DMA1SAD+2: dma9[1].io.src_addr = (dma9[1].io.src_addr & 0xFF00FFFF) | (val << 16); return;
        case R_DMA1SAD+3: dma9[1].io.src_addr = (dma9[1].io.src_addr & 0x00FFFFFF) | ((val & 0x0F) << 24); return;
        case R_DMA1DAD+0: dma9[1].io.dest_addr = (dma9[1].io.dest_addr & 0xFFFFFF00) | (val << 0); return;
        case R_DMA1DAD+1: dma9[1].io.dest_addr = (dma9[1].io.dest_addr & 0xFFFF00FF) | (val << 8); return;
        case R_DMA1DAD+2: dma9[1].io.dest_addr = (dma9[1].io.dest_addr & 0xFF00FFFF) | (val << 16); return;
        case R_DMA1DAD+3: dma9[1].io.dest_addr = (dma9[1].io.dest_addr & 0x00FFFFFF) | ((val & 0x0F) << 24); return;

        case R_DMA2SAD+0: dma9[2].io.src_addr = (dma9[2].io.src_addr & 0xFFFFFF00) | (val << 0); return;
        case R_DMA2SAD+1: dma9[2].io.src_addr = (dma9[2].io.src_addr & 0xFFFF00FF) | (val << 8); return;
        case R_DMA2SAD+2: dma9[2].io.src_addr = (dma9[2].io.src_addr & 0xFF00FFFF) | (val << 16); return;
        case R_DMA2SAD+3: dma9[2].io.src_addr = (dma9[2].io.src_addr & 0x00FFFFFF) | ((val & 0x0F) << 24); return;
        case R_DMA2DAD+0: dma9[2].io.dest_addr = (dma9[2].io.dest_addr & 0xFFFFFF00) | (val << 0); return;
        case R_DMA2DAD+1: dma9[2].io.dest_addr = (dma9[2].io.dest_addr & 0xFFFF00FF) | (val << 8); return;
        case R_DMA2DAD+2: dma9[2].io.dest_addr = (dma9[2].io.dest_addr & 0xFF00FFFF) | (val << 16); return;
        case R_DMA2DAD+3: dma9[2].io.dest_addr = (dma9[2].io.dest_addr & 0x00FFFFFF) | ((val & 0x0F) << 24); return;

        case R_DMA3SAD+0: dma9[3].io.src_addr = (dma9[3].io.src_addr & 0xFFFFFF00) | (val << 0); return;
        case R_DMA3SAD+1: dma9[3].io.src_addr = (dma9[3].io.src_addr & 0xFFFF00FF) | (val << 8); return;
        case R_DMA3SAD+2: dma9[3].io.src_addr = (dma9[3].io.src_addr & 0xFF00FFFF) | (val << 16); return;
        case R_DMA3SAD+3: dma9[3].io.src_addr = (dma9[3].io.src_addr & 0x00FFFFFF) | ((val & 0x0F) << 24); return;
        case R_DMA3DAD+0: dma9[3].io.dest_addr = (dma9[3].io.dest_addr & 0xFFFFFF00) | (val << 0); return;
        case R_DMA3DAD+1: dma9[3].io.dest_addr = (dma9[3].io.dest_addr & 0xFFFF00FF) | (val << 8); return;
        case R_DMA3DAD+2: dma9[3].io.dest_addr = (dma9[3].io.dest_addr & 0xFF00FFFF) | (val << 16); return;
        case R_DMA3DAD+3: dma9[3].io.dest_addr = (dma9[3].io.dest_addr & 0x00FFFFFF) | ((val & 0x0F) << 24); return;

        case R_DMA0CNT_L+0: dma9[0].io.word_count = (dma9[0].io.word_count & 0x1FFF00) | (val << 0); return;
        case R_DMA0CNT_L+1: dma9[0].io.word_count = (dma9[0].io.word_count & 0x1F00FF) | (val << 8); return;
        case R_DMA1CNT_L+0: dma9[1].io.word_count = (dma9[1].io.word_count & 0x1FFF00) | (val << 0); return;
        case R_DMA1CNT_L+1: dma9[1].io.word_count = (dma9[1].io.word_count & 0x1F00FF) | (val << 8); return;
        case R_DMA2CNT_L+0: dma9[2].io.word_count = (dma9[2].io.word_count & 0x1FFF00) | (val << 0); return;
        case R_DMA2CNT_L+1: dma9[2].io.word_count = (dma9[2].io.word_count & 0x1F00FF) | (val << 8); return;
        case R_DMA3CNT_L+0: dma9[3].io.word_count = (dma9[3].io.word_count & 0x1FFF00) | (val << 0); return;
        case R_DMA3CNT_L+1: dma9[3].io.word_count = (dma9[3].io.word_count & 0x1F00FF) | (val << 8); return;

        case R_DMA0CNT_H+0:
        case R_DMA1CNT_H+0:
        case R_DMA2CNT_H+0:
        case R_DMA3CNT_H+0: {
            DMA_ch &ch = dma9[DMA_CH_NUM(addr)];
            ch.io.word_count = (ch.io.word_count & 0xFFFF) | ((val & 31) << 16);
            ch.io.dest_addr_ctrl = (val >> 5) & 3;
            ch.io.src_addr_ctrl = (ch.io.src_addr_ctrl & 2) | ((val >> 7) & 1);
            return;}

        case R_DMA0CNT_H+1:
        case R_DMA1CNT_H+1:
        case R_DMA2CNT_H+1:
        case R_DMA3CNT_H+1: {
            u32 chnum = DMA_CH_NUM(addr);
            DMA_ch &ch = dma9[chnum];
            ch.io.src_addr_ctrl = (ch.io.src_addr_ctrl & 1) | ((val & 1) << 1);
            ch.io.repeat = (val >> 1) & 1;
            ch.io.transfer_size = (val >> 2) & 1;
            ch.io.start_timing = (val >> 3) & 7;
            ch.io.irq_on_end = (val >> 6) & 1;

            u32 old_enable = ch.io.enable;
            ch.io.enable = (val >> 7) & 1;
            if ((ch.io.enable == 1) && (old_enable == 0)) {
                ch.op.first_run = 1;
                if (ch.io.start_timing == 0) {
                    dma9_start(ch, chnum);
                }
            }

            if (ch.io.start_timing == DMA_GE_FIFO) {
                ch.op.first_run = 1;
                if (ge.fifo.len < 128) {
                    dma9_start(ch, chnum);
                }
            }
            else if (ch.io.start_timing > 3) {
                if (ch.io.start_timing != 5) printf("\nwarn DMA9 Start timing:%d", ch.io.start_timing);
            }
            return;}

        case R9_DMAFIL+0:
        case R9_DMAFIL+1:
        case R9_DMAFIL+2:
        case R9_DMAFIL+3:
        case R9_DMAFIL+4:
        case R9_DMAFIL+5:
        case R9_DMAFIL+6:
        case R9_DMAFIL+7:
        case R9_DMAFIL+8:
        case R9_DMAFIL+9:
        case R9_DMAFIL+10:
        case R9_DMAFIL+11:
        case R9_DMAFIL+12:
        case R9_DMAFIL+13:
        case R9_DMAFIL+14:
        case R9_DMAFIL+15:
            io.dma.filldata[addr & 15] = val;
            return;
        case R9_VRAMCNT+0:
        case R9_VRAMCNT+1:
        case R9_VRAMCNT+2:
        case R9_VRAMCNT+3:
        case R9_VRAMCNT+4:
        case R9_VRAMCNT+5:
        case R9_VRAMCNT+6:
        case R9_VRAMCNT+8:
        case R9_VRAMCNT+9: {
            u32 bank_num = addr - R9_VRAMCNT;
            if (bank_num >= 8) bank_num--;


            if ((bank_num < 2) || (bank_num >= 7)) mem.vram.io.bank[bank_num].mst = val & 3;
            else mem.vram.io.bank[bank_num].mst = val & 7;

            mem.vram.io.bank[bank_num].ofs = (val >> 3) & 3;
            mem.vram.io.bank[bank_num].enable = (val >> 7) & 1;

            //printf("\nWRITE VRAM val:%02x CNT:%d MST:%d OFS:%d enable:%d", val, bank_num, mem.vram.io.bank[bank_num].mst, mem.vram.io.bank[bank_num].ofs, mem.vram.io.bank[bank_num].enable);
            VRAM_resetup_banks();
            return; }

        case R_TM0CNT_H+1:
        case R_TM1CNT_H+1:
        case R_TM2CNT_H+1:
        case R_TM3CNT_H+1:
            return;

        case R_TM0CNT_H+0:
        case R_TM1CNT_H+0:
        case R_TM2CNT_H+0:
        case R_TM3CNT_H+0: {
            u32 tn = (addr >> 2) & 3;
            timer9[tn].write_cnt(val);
            return; }

        case R_TM0CNT_L+0: timer9[0].reload = (timer9[0].reload & 0xFF00) | val; return;
        case R_TM1CNT_L+0: timer9[1].reload = (timer9[1].reload & 0xFF00) | val; return;
        case R_TM2CNT_L+0: timer9[2].reload = (timer9[2].reload & 0xFF00) | val; return;
        case R_TM3CNT_L+0: timer9[3].reload = (timer9[3].reload & 0xFF00) | val; return;

        case R_TM0CNT_L+1: timer9[0].reload = (timer9[0].reload & 0xFF) | (val << 8); return;
        case R_TM1CNT_L+1: timer9[1].reload = (timer9[1].reload & 0xFF) | (val << 8); return;
        case R_TM2CNT_L+1: timer9[2].reload = (timer9[2].reload & 0xFF) | (val << 8); return;
        case R_TM3CNT_L+1: timer9[3].reload = (timer9[3].reload & 0xFF) | (val << 8); return;

        case R9_WRAMCNT: {
            mem.io.RAM9.val = val;
            switch (val & 3) {
                case 0: // 0 = 32k/0K, open-bus
                    mem.io.RAM9.base = 0;
                    mem.io.RAM9.mask = 0x7FFF;
                    mem.io.RAM9.disabled = 0;
                    mem.io.RAM7.base = 0;
                    mem.io.RAM7.mask = 0;
                    mem.io.RAM7.disabled = 1;
                    break;
                case 1: // 1 = hi 16K/ lo16K,
                    mem.io.RAM9.base = 0x4000;
                    mem.io.RAM9.mask = 0x3FFF;
                    mem.io.RAM9.disabled = 0;
                    mem.io.RAM7.base = 0;
                    mem.io.RAM7.mask = 0x3FFF;
                    mem.io.RAM7.disabled = 0;
                    break;
                case 2: // 2 = lo 16k/ hi16k,
                    mem.io.RAM9.base = 0;
                    mem.io.RAM9.mask = 0x3FFF;
                    mem.io.RAM9.disabled = 0;
                    mem.io.RAM7.base = 0x4000;
                    mem.io.RAM7.mask = 0x3FFF;
                    mem.io.RAM7.disabled = 0;
                    break;
                case 3: // 3 = 0k / 32k
                    mem.io.RAM9.base = 0;
                    mem.io.RAM9.mask = 0;
                    mem.io.RAM9.disabled = 1;
                    mem.io.RAM7.base = 0;
                    mem.io.RAM7.mask = 0x7FFF;
                    mem.io.RAM7.disabled = 0;
            }
            return; }

        case R9_EXMEMCNT+0:
            io.arm9.EXMEM = (io.arm9.EXMEM & 0xFF00) | val;
            io.rights.gba_slot = ((val >> 7) & 1) ^ 1;
            return;
        case R9_EXMEMCNT+1:
            io.arm9.EXMEM = (io.arm9.EXMEM & 0xFF) | (val << 8);
            io.rights.nds_slot_is7 = ((val >> 3) & 1);
            io.rights.main_memory = ((val >> 7) & 1);
            return;

    }
    printf("\nUnhandled BUSWR9IO8 addr:%08x val:%08x", addr, val);
}

// -----

static u32 busrd9_apu(u32 addr, u8 sz, u32 access, bool has_effect){
    static int already_did = 0;
    if (!already_did) {
        already_did = 1;
        printf("\nWARN: APU READ9!");
    }
    return 0;
}

u32 core::busrd9_io(u32 addr, u8 sz, u32 access, bool has_effect)
{
    u32 v;
    if (((addr >= 0x04000000) && (addr < 0x04000070)) || ((addr >= 0x04001000) && (addr < 0x04001070))) {
        return ppu.read9_io(addr, sz, access, has_effect);
    }
    if ((addr >= 0x04000320) && (addr < 0x04000700)) {
        return ge.read(addr, sz);
    }
    if ((addr >= 0x04000400) && (addr < 0x04000520)) return busrd9_apu(addr, sz, access, has_effect);
    switch(addr) {
        case R_ROMCTRL:
            if (io.rights.nds_slot_is7) return 0;
            return cart.read_romctrl();

        case R_ROMDATA+0: // 4100010
        case R_ROMDATA+1: // 4100011
        case R_ROMDATA+2: // 4100012
        case R_ROMDATA+3: // 4100013
            assert(sz==4);
            if (io.rights.nds_slot_is7) return 0;
            return cart.read_rom(addr, sz);

        case R_POSTFLG:
            return io.arm9.POSTFLG;

        case R_IPCFIFORECV+0:
        case R_IPCFIFORECV+1:
        case R_IPCFIFORECV+2:
        case R_IPCFIFORECV+3:
            // arm9 reads from to_arm9
            if (io.ipc.arm9.fifo_enable) {
                if (io.ipc.to_arm9.is_empty()) {
                    io.ipc.arm9.error |= 1;
                    v = io.ipc.to_arm9.peek_last();
                }
                else {
                    u32 old_bits7 = io.ipc.to_arm9.is_empty() & io.ipc.arm7.irq_on_send_fifo_empty;
                    v = io.ipc.to_arm9.pop();
                    u32 new_bits7 = io.ipc.to_arm9.is_empty() & io.ipc.arm7.irq_on_send_fifo_empty;
                    if (!old_bits7 && new_bits7) { // arm7 send is empty
                        update_IF7(IRQ_IPC_SEND_EMPTY);
                    }

                }
            }
            else {
                v = io.ipc.to_arm9.peek_last();
            };
            return v & masksz[sz];
    }

    v = busrd9_io8(addr, 1, access, has_effect);
    if (sz >= 2) v |= busrd9_io8(addr+1, 1, access, has_effect) << 8;
    if (sz == 4) {
        v |= busrd9_io8(addr+2, 1, access, has_effect) << 16;
        v |= busrd9_io8(addr+3, 1, access, has_effect) << 24;
    }
    return v;
}

static void buswr9_apu(u32 addr, u8 sz, u32 access, u32 val) {
    static int already_did = 0;
    if (!already_did) {
        already_did = 1;
        printf("\nWARN: APU WRITE9!");
    }
}


void core::buswr9_io(u32 addr, u8 sz, u32 access, u32 val)
{
    if (((addr >= 0x04000000) && (addr < 0x04000070)) || ((addr >= 0x04001000) && (addr < 0x04001070))) {
        ppu.write9_io(addr, sz, access, val);
        return;
    }
    if ((addr >= 0x04000320) && (addr < 0x04000700)) {
        ge.write(addr, sz, val);
        return;
    }

    if ((addr >= 0x04000400) && (addr < 0x04000520)) return buswr9_apu(addr, sz, access, val);
    switch(addr) {
        case R_AUXSPICNT: {
            if (io.rights.nds_slot_is7) return;
            cart.spi_write_spicnt(val & 0xFF, 0);
            if (sz >= 2)
                cart.spi_write_spicnt((val >> 8) & 0xFF, 1);
            if (sz == 4) {
                buswr9_io(R_AUXSPIDATA, 2, access, val >> 16);
            }
            return; }
        case R_AUXSPICNT+1:
            if (io.rights.nds_slot_is7) return;
            cart.spi_write_spicnt(val & 0xFF, 1);
            return;

        case R_AUXSPIDATA:
            if (io.rights.nds_slot_is7) return;
            assert(sz!=1);
            cart.spi_transaction(val & 0xFFFF);
            if (sz == 4) {
                buswr9_io(R_ROMCTRL, 2, access, val >> 16);
            }
            return;
        case R_ROMCTRL:
            if (io.rights.nds_slot_is7) return;
            cart.write_romctrl(val);
            return;

        case R_IPCFIFOSEND+0:
        case R_IPCFIFOSEND+1:
        case R_IPCFIFOSEND+2:
        case R_IPCFIFOSEND+3:
            // All writes are only 32 bits here
            printf("\nIPCIFOSEND!");
            if (io.ipc.arm9.fifo_enable) {
                if (sz == 2) {
                    val &= 0xFFFF;
                    val = (val << 16) | val;
                }
                if (sz == 1) {
                    val &= 0xFF;
                    val = (val << 24) | (val << 16) | (val << 8) | val;
                }
                // ARM9 writes to_arm7
                u32 old_bits = io.ipc.to_arm7.is_not_empty() & io.ipc.arm7.irq_on_recv_fifo_not_empty;
                if (io.ipc.arm9.fifo_enable) {
                    io.ipc.arm9.error |= io.ipc.to_arm7.push(val);
                    cart.detect_kind(9, val);
                }

                u32 new_bits = io.ipc.to_arm7.is_not_empty() & io.ipc.arm7.irq_on_recv_fifo_not_empty;
                if (!old_bits && new_bits) {
                    // Trigger ARM7 recv not empty
                    update_IF7(IRQ_IPC_RECV_NOT_EMPTY);
                }
            }
            return;
    }
    buswr9_io8(addr, 1, access, val & 0xFF);
    if (sz >= 2) buswr9_io8(addr+1, 1, access, (val >> 8) & 0xFF);
    if (sz == 4) {
        buswr9_io8(addr+2, 1, access, (val >> 16) & 0xFF);
        buswr9_io8(addr+3, 1, access, (val >> 24) & 0xFF);
    }
}

u32 core::busrd7_wifi(u32 addr, u8 sz, u32 access, bool has_effect) {
    // 0x04804000 and 0x480C000 are the two 8KB RAM sections, oops!
    if (addr < 0x04810000) return cR[sz](mem.wifi, addr & 0x1FFF);
    static int a = 1;
    if (a) {
        a = 0;
        printf("\nWARN read from WIFI!");
    }
    return 0;
}

void core::buswr7_wifi(u32 addr, u8 sz, u32 access, u32 val)
{
    if (addr < 0x04810000) return cW[sz](mem.wifi, addr & 0x1FFF, val);

    static int a = 1;
    if (a) {
        printf("\nWarning ignore WIFI WRITE(s)....");
        a = 0;
    }
    return;
}


u32 core::busrd7_io(u32 addr, u8 sz, u32 access, bool has_effect)
{
    if (((addr >= 0x04000000) && (addr < 0x04000070)) || ((addr >= 0x04001000) && (addr < 0x04001070))) {
        return ppu.read7_io(addr, sz, access, has_effect);
    }
    if ((addr >= 0x04000400) && (addr < 0x04000520)) return apu.read(addr, sz, access);
    if (addr >= 0x04800000) return busrd7_wifi(addr, sz, access, has_effect);
    u32 v;
    switch(addr) {
        case R_ROMCTRL:
            if (!io.rights.nds_slot_is7) return 0;
            return cart.read_romctrl();

        case R_ROMDATA+0:
        case R_ROMDATA+1:
        case R_ROMDATA+2:
        case R_ROMDATA+3:
            assert(sz==4);
            if (!io.rights.nds_slot_is7) return 0;
            return cart.read_rom(addr, sz);

        case R_AUXSPIDATA:
            if (!io.rights.nds_slot_is7) return 0;
            return cart.read_spi(0);
        case R_AUXSPIDATA+1:
            if (!io.rights.nds_slot_is7) return 0;
            return cart.read_spi(1);

        case R7_SPIDATA:
            return SPI_read(sz);

        case R_IPCFIFORECV+0:
        case R_IPCFIFORECV+1:
        case R_IPCFIFORECV+2:
        case R_IPCFIFORECV+3:
            // arm7 reads from to_arm7
            if (io.ipc.arm7.fifo_enable) {
                if (io.ipc.to_arm7.is_empty()) {
                    io.ipc.arm7.error |= 1;
                    v = io.ipc.to_arm7.peek_last();
                }
                else {
                    u32 old_bits = io.ipc.to_arm7.is_empty() & io.ipc.arm9.irq_on_send_fifo_empty;
                    v = io.ipc.to_arm7.pop();
                    u32 new_bits = io.ipc.to_arm7.is_empty() & io.ipc.arm9.irq_on_send_fifo_empty;
                    if (!old_bits && new_bits) { // arm7 send is empty
                        update_IF9(IRQ_IPC_SEND_EMPTY);
                    }
                }
            }
            else {
                v = io.ipc.to_arm7.peek_last();
            };
            return v & masksz[sz];
    }

    v = busrd7_io8(addr, 1, access, has_effect);
    if (sz >= 2) v |= busrd7_io8(addr+1, 1, access, has_effect) << 8;
    if (sz == 4) {
        v |= busrd7_io8(addr+2, 1, access, has_effect) << 16;
        v |= busrd7_io8(addr+3, 1, access, has_effect) << 24;
    }
    return v;
}

void core::buswr7_io(u32 addr, u8 sz, u32 access, u32 val)
{
    if (((addr >= 0x04000000) && (addr < 0x04000070)) || ((addr >= 0x04001000) && (addr < 0x04001070))) {
        ppu.write7_io(addr, sz, access, val);
        return;
    }
    if ((addr >= 0x04000400) && (addr < 0x04000520)) return apu.write(addr, sz, access, val);
    if (addr >= 0x04800000) return buswr7_wifi(addr, sz, access, val);

    switch(addr) {
        case R7_SPIDATA:
            SPI_write(sz, val);
            return;

        case R_AUXSPICNT: {
            if (!io.rights.nds_slot_is7) return;
            cart.spi_write_spicnt(val & 0xFF, 0);
            if (sz >= 2) {
                cart.spi_write_spicnt((val >> 8) & 0xFF, 1);
            }
            if (sz == 4) {
                buswr7_io(R_AUXSPIDATA, 2, access, val >> 16);
            }
            return; }
        case R_AUXSPICNT+1:
            if (!io.rights.nds_slot_is7) return;
            cart.spi_write_spicnt(val & 0xFF, 1);
            return;

        case R_AUXSPIDATA:
            if (!io.rights.nds_slot_is7) return;
            cart.spi_transaction(val & 0xFFFF);
            if (sz == 4) {
                buswr7_io(R_ROMCTRL, 2, access, val >> 16);
            }
            return;
        case R_ROMCTRL:
            if (!io.rights.nds_slot_is7) return;
            cart.write_romctrl(val);
            return;

        case R_RTC:
            write_RTC(sz, val & 0xFFFF);
            return;
        case R_IPCFIFOSEND+0:
        case R_IPCFIFOSEND+1:
        case R_IPCFIFOSEND+2:
        case R_IPCFIFOSEND+3:
            // All writes are only 32 bits here
            if (io.ipc.arm7.fifo_enable) {
                if (sz == 2) {
                    val &= 0xFFFF;
                    val = (val << 16) | val;
                }
                if (sz == 1) {
                    val &= 0xFF;
                    val = (val << 24) | (val << 16) | (val << 8) | val;
                }
                // ARM7 writes to_arm9
                u32 old_bits = io.ipc.to_arm9.is_not_empty() & io.ipc.arm9.irq_on_recv_fifo_not_empty;
                if (io.ipc.arm7.fifo_enable) {
                    io.ipc.arm7.error |= io.ipc.to_arm9.push(val);
                    //if (!cart.backup.detect.done) cart.detect_kind(7, val);
                }
                u32 new_bits = io.ipc.to_arm9.is_not_empty() & io.ipc.arm9.irq_on_recv_fifo_not_empty;
                if (!old_bits && new_bits) {
                    // Trigger ARM9 recv not empty
                    update_IF9(IRQ_IPC_RECV_NOT_EMPTY);
                }
            }
            return;
    }

    if (addr >= 0x04800000) return buswr7_wifi(addr, sz, access, val);
    buswr7_io8(addr, 1, access, val & 0xFF);
    if (sz >= 2) buswr7_io8(addr+1, 1, access, (val >> 8) & 0xFF);
    if (sz == 4) {
        buswr7_io8(addr+2, 1, access, (val >> 16) & 0xFF);
        buswr7_io8(addr+3, 1, access, (val >> 24) & 0xFF);
    }
}

void core::reset() {
    RTC_reset();
    spi.irq_id = 0;

    for (u32 i = 0; i < 4; i++) {
        timer7_t *t = &timer7[i];
        t->overflow_at = 0xFFFFFFFFFFFFFFFF;
        t->enable_at = 0xFFFFFFFFFFFFFFFF;
        timer9_t *p = &timer9[i];
        p->overflow_at = 0xFFFFFFFFFFFFFFFF;
        p->enable_at = 0xFFFFFFFFFFFFFFFF;
    }
}

core::core() :
    clock{&this->waitstates.current_transaction},
    scheduler{&clock.master_cycle_count7},
    arm7{&clock.master_cycle_count7, &waitstates.current_transaction, &scheduler}, arm9{&scheduler, &clock.master_cycle_count9, &waitstates.current_transaction}, ppu{this}, ge{this, &scheduler},
    re{this}, apu{this, &scheduler},
    cart{this}
{
    re.ge = &ge;
    ge.re = &re;
    for (u32 i = 0; i < 16; i++) {
        mem.rw[0].read[i] = &core::busrd7_invalid;
        mem.rw[0].write[i] = &core::buswr7_invalid;
        mem.rw[1].read[i] = &core::busrd9_invalid;
        mem.rw[1].write[i] = &core::buswr9_invalid;
    }
    memset(dbg_info.mgba.str, 0, sizeof(dbg_info.mgba.str));
#define BND9(page, func) { mem.rw[1].read[page] = &core::busrd9_##func; mem.rw[1].write[page] = &core::buswr9_##func; }
    BND9(0x2, main);
    BND9(0x3, shared);
    BND9(0x4, io);
    BND9(0x5, obj_and_palette);
    BND9(0x6, vram);
    BND9(0x7, oam);
    BND9(0x8, gba_cart);
    BND9(0x9, gba_cart);
    BND9(0xA, gba_sram);
#undef BND9

#define BND7(page, func) { mem.rw[0].read[page] = &core::busrd7_##func; mem.rw[0].write[page] = &core::buswr7_##func; }
    BND7(0x0, bios7);
    BND7(0x2, main);
    BND7(0x3, shared);
    BND7(0x4, io);
    BND7(0x6, vram);
    BND7(0x8, gba_cart);
    BND7(0x9, gba_cart);
    BND7(0xA, gba_sram);
#undef BND7
}

u32 core::mainbus_read7(void *ptr, u32 addr, u8 sz, u32 access, bool has_effect)
{
    addr &= maskalign[sz];
    auto *th = static_cast<core *>(ptr);
    if (has_effect) th->waitstates.current_transaction++;
    u32 v;

    if (addr < 0x10000000) v = (th->*(th->mem.rw[0].read[(addr >> 24) & 15]))(addr, sz, access, has_effect);
    else v = th->busrd7_invalid(addr, sz, access, has_effect);
    //if (dbg.trace_on) trace_read(addr, sz, v);
#ifdef TRACE
    printf("\n rd7:%08x sz:%d val:%08x", addr, sz, v);
#endif
    return v;
}

u32 core::rd9_bios(u32 addr, u8 sz) const {
    return cR[sz](mem.bios9, addr & 0xFFF);
}

u32 core::mainbus_read9(void *ptr, u32 addr, u8 sz, u32 access, bool has_effect)
{
    auto *th = static_cast<core *>(ptr);
    th->waitstates.current_transaction++;
    u32 v;

    if (addr < 0x10000000) v = (th->*(th->mem.rw[1].read[(addr >> 24) & 15]))(addr, sz, access, has_effect);
    else if ((addr & 0xFFFF0000) == 0xFFFF0000) v = th->rd9_bios(addr, sz);
    else v = th->busrd9_invalid(addr, sz, access, has_effect);
#ifdef TRACE
    printf("\n rd9:%08x sz:%d val:%08x", addr, sz, v);
#endif
    //if (dbg.trace_on) trace_read(addr, sz, v);
    return v;
}

u32 core::mainbus_fetchins9(void *ptr, u32 addr, u8 sz, u32 access)
{
    auto *th = static_cast<core *>(ptr);
    u32 v = th->mainbus_read9(ptr, addr, sz, access, true);
    switch(sz) {
        case 4:
            th->io.open_bus.arm9 = v;
            break;
        case 2:
            th->io.open_bus.arm9 = (v << 16) | v;
            break;
    }
    return v;
}


u32 core::mainbus_fetchins7(void *ptr, u32 addr, u8 sz, u32 access)
{
    auto *th = static_cast<core *>(ptr);
    u32 v = mainbus_read7(ptr, addr, sz, access, true);
    switch(sz) {
        case 4:
            th->io.open_bus.arm7 = v;
            break;
        case 2:
            th->io.open_bus.arm7 = (v << 16) | v;
            break;
    }
    return v;
}

void core::mainbus_write7(void *ptr, u32 addr, u8 sz, u32 access, u32 val)
{
    addr &= maskalign[sz];
    auto *th = static_cast<core *>(ptr);
    th->waitstates.current_transaction++;
#ifdef TRACE
    printf("\n wr7:%08x sz:%d val:%08x", addr, sz, val);
#endif
    //if (dbg.trace_on) trace_write(addr, sz, val);
    if (addr < 0x10000000) {
        //printf("\nWRITE addr:%08x sz:%d val:%08x", addr, sz, val);
        return (th->*(th->mem.rw[0].write[(addr >> 24) & 15]))(addr, sz, access, val);
    }

    th->buswr7_invalid(addr, sz, access, val);
}

void core::mainbus_write9(void *ptr, u32 addr, u8 sz, u32 access, u32 val)
{
    auto *th = static_cast<core *>(ptr);
    th->waitstates.current_transaction++;
    //if (dbg.trace_on) trace_write(addr, sz, val);
#ifdef TRACE
    printf("\n wr9:%08x sz:%d val:%08x", addr, sz, val);
#endif

    /*if ((addr >= 0x6890000) && (addr < 0x06894000)) {
        if (val != 0) printf("\nbankF pallete write %04x: %04x", addr & 0x3FFF, val);
    }*/

    if (addr < 0x10000000) {
        return (th->*(th->mem.rw[1].write[(addr >> 24) & 15]))(addr, sz, access, val);
    }

    th->buswr9_invalid(addr, sz, access, val);
}

}
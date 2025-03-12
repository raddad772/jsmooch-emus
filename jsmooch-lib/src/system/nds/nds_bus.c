//
// Created by . on 12/4/24.
//
#include <string.h>

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
#include "helpers/multisize_memaccess.c"

static const u32 masksz[5] = { 0, 0xFF, 0xFFFF, 0, 0xFFFFFFFF};

static u32 timer_reload_ticks(u32 reload)
{
    // So it overflows at 0x100
    // reload value is 0xFD
    // 0xFD ^ 0xFF = 2
    // How many ticks til 0x100? 256 - 253 = 3, correct!
    // 100. 256 - 100 = 156, correct!
    // Unfortunately if we set 0xFFFF, we need 0x1000 tiks...
    // ok but what about when we set 255? 256 - 255 = 1 which is wrong
    if (reload == 0xFFFF) return 0x10000;
    return 0x10000 - reload;
}


static u32 busrd7_invalid(struct NDS *this, u32 addr, u32 sz, u32 access, u32 has_effect) {
    printf("\nREAD7 UNKNOWN ADDR:%08x sz:%d", addr, sz);
    this->waitstates.current_transaction++;
    //dbg.var++;
    //if (dbg.var > 15) dbg_break("too many bad reads", this->clock.master_cycle_count);
    return 0;
}

static u32 busrd9_invalid(struct NDS *this, u32 addr, u32 sz, u32 access, u32 has_effect) {
    printf("\nREAD9 UNKNOWN ADDR:%08x sz:%d", addr, sz);
    this->waitstates.current_transaction++;
    //dbg.var++;
    //if (dbg.var > 15) dbg_break("too many bad reads", this->clock.master_cycle_count);
    return 0;
}

static void buswr7_invalid(struct NDS *this, u32 addr, u32 sz, u32 access, u32 val) {
    printf("\nWRITE7 UNKNOWN ADDR:%08x sz:%d DATA:%08x", addr, sz, val);
    this->waitstates.current_transaction++;
    dbg.var++;
    //if (dbg.var > 15) dbg_break("too many bad writes", this->clock.master_cycle_count);
}

static void buswr9_invalid(struct NDS *this, u32 addr, u32 sz, u32 access, u32 val) {
    this->waitstates.current_transaction++;
    static int pokemon_didit = 0;
    if ((addr == 0) && !pokemon_didit) {
        pokemon_didit = 1;
        printf("\nWRITE9 UNKNOWN ADDR:%08x sz:%d DATA:%08x", addr, sz, val);
    }
    if (addr != 0) {
        printf("\nWRITE9 UNKNOWN ADDR:%08x sz:%d DATA:%08x", addr, sz, val);
        dbg.var++;
        if (dbg.var > 15) dbg_break("too many bad writes", this->clock.master_cycle_count7);
    }
    //dbg_break("unknown addr write9", this->clock.master_cycle_count7);
}

static void buswr7_shared(struct NDS *this, u32 addr, u32 sz, u32 access, u32 val)
{
    if (addr >= 0x03800000) return cW[sz](this->mem.WRAM_arm7, addr & 0xFFFF, val);
    if (!this->mem.io.RAM7.disabled) cW[sz](this->mem.WRAM_share, (addr & this->mem.io.RAM7.mask) + this->mem.io.RAM7.base, val);
    else cW[sz](this->mem.WRAM_arm7, addr & 0xFFFF, val);
}

static u32 busrd7_shared(struct NDS *this, u32 addr, u32 sz, u32 access, u32 has_effect)
{
    if (addr >= 0x03800000) return cR[sz](this->mem.WRAM_arm7, addr & 0xFFFF);
    if (this->mem.io.RAM7.disabled) return cR[sz](this->mem.WRAM_arm7, addr & 0xFFFF);
    return cR[sz](this->mem.WRAM_share, (addr & this->mem.io.RAM7.mask) + this->mem.io.RAM7.base);
}

static void buswr7_vram(struct NDS *this, u32 addr, u32 sz, u32 access, u32 val)
{
    u32 bank = (addr >> 17) & 1;
    if (this->mem.vram.map.arm7[bank]) return cW[sz](this->mem.vram.map.arm7[bank], addr & 0x1FFFF, val);

    printf("\nWarning write7 to unmapped VRAM:%08x sz:%d data:%08x", addr, sz, val);
}

static u32 busrd7_vram(struct NDS *this, u32 addr, u32 sz, u32 access, u32 has_effect)
{
    u32 bank = (addr >> 17) & 1;
    if (this->mem.vram.map.arm7[bank]) return cR[sz](this->mem.vram.map.arm7[bank], addr & 0x1FFFF);

    return busrd7_invalid(this, addr, sz, access, has_effect);
}

static void buswr7_gba_cart(struct NDS *this, u32 addr, u32 sz, u32 access, u32 val)
{
    return;
}

static u32 busrd7_gba_cart(struct NDS *this, u32 addr, u32 sz, u32 access, u32 has_effect)
{
    if (!this->io.rights.gba_slot) return (addr & 0x1FFFF) >> 1;
    return 0;
}

static void buswr7_gba_sram(struct NDS *this, u32 addr, u32 sz, u32 access, u32 val)
{
    return;
}

static u32 busrd7_gba_sram(struct NDS *this, u32 addr, u32 sz, u32 access, u32 has_effect)
{
    if (!this->io.rights.gba_slot) return masksz[sz];
    return 0;
}

static u32 busrd9_main(struct NDS *this, u32 addr, u32 sz, u32 access, u32 has_effect)
{
    return cR[sz](this->mem.RAM, addr & 0x3FFFFF);
}

static void buswr9_main(struct NDS *this, u32 addr, u32 sz, u32 access, u32 val)
{
    cW[sz](this->mem.RAM, addr & 0x3FFFFF, val);
}


static void buswr9_gba_cart(struct NDS *this, u32 addr, u32 sz, u32 access, u32 val)
{
    return;
}

static u32 busrd9_gba_cart(struct NDS *this, u32 addr, u32 sz, u32 access, u32 has_effect)
{
    if (this->io.rights.gba_slot) return (addr & 0x1FFFF) >> 1;
    return 0;
}

static void buswr9_gba_sram(struct NDS *this, u32 addr, u32 sz, u32 access, u32 val)
{
    return;
}

static u32 busrd9_gba_sram(struct NDS *this, u32 addr, u32 sz, u32 access, u32 has_effect)
{
    if (this->io.rights.gba_slot) return masksz[sz];
    return 0;
}

static void buswr9_shared(struct NDS *this, u32 addr, u32 sz, u32 access, u32 val)
{
    if (!this->mem.io.RAM9.disabled) cW[sz](this->mem.WRAM_share, (addr & this->mem.io.RAM9.mask) + this->mem.io.RAM9.base, val);
}

static u32 busrd9_shared(struct NDS *this, u32 addr, u32 sz, u32 access, u32 has_effect)
{
    if (this->mem.io.RAM9.disabled) return 0; // undefined
    return cR[sz](this->mem.WRAM_share, (addr & this->mem.io.RAM9.mask) + this->mem.io.RAM9.base);
}

static void buswr9_obj_and_palette(struct NDS *this, u32 addr, u32 sz, u32 access, u32 val)
{
    if (addr < 0x05000000) return;
    addr &= 0x7FF;
    if (addr < 0x200) return NDS_PPU_write_2d_bg_palette(this, 0, addr & 0x1FF, sz, val);
    if (addr < 0x400) return NDS_PPU_write_2d_obj_palette(this, 0, addr & 0x1FF, sz, val);
    if (addr < 0x600) return NDS_PPU_write_2d_bg_palette(this, 1, addr & 0x1FF, sz, val);
    if (addr < 0x800) return NDS_PPU_write_2d_obj_palette(this, 1, addr & 0x1FF, sz, val);
    buswr9_invalid(this, addr, sz, access, val);
}

static u32 busrd9_obj_and_palette(struct NDS *this, u32 addr, u32 sz, u32 access, u32 has_effect)
{
    if (addr < 0x05000000) return busrd9_invalid(this, addr, sz, access, has_effect);
    addr &= 0x7FF;
    if (addr < 200) return NDS_PPU_read_2d_bg_palette(this, 0, addr & 0x1FF, sz);
    if (addr < 400) return NDS_PPU_read_2d_obj_palette(this, 0, addr & 0x1FF, sz);
    if (addr < 600) return NDS_PPU_read_2d_bg_palette(this, 1, addr & 0x1FF, sz);
    if (addr < 800) return NDS_PPU_read_2d_obj_palette(this, 1, addr & 0x1FF, sz);
    return busrd9_invalid(this, addr, sz, access, has_effect);
}

static void buswr9_vram(struct NDS *this, u32 addr, u32 sz, u32 access, u32 val)
{
    if (sz == 1) return; // 8-bit writes ignored
    u8 *ptr = this->mem.vram.map.arm9[NDSVRAMSHIFT(addr) & NDSVRAMMASK];
    if (ptr) return cW[sz](ptr, addr & 0x3FFF, val);

    printf("\nInvalid VRAM write unmapped addr:%08x sz:%d val:%08x", addr, sz, val);
    //dbg_break("Unmapped VRAM9 write", this->clock.master_cycle_count7);
}

static u32 busrd9_vram(struct NDS *this, u32 addr, u32 sz, u32 access, u32 has_effect)
{
    u8 *ptr = this->mem.vram.map.arm9[NDSVRAMSHIFT(addr) & NDSVRAMMASK];
    if (ptr) return cR[sz](ptr, addr & 0x3FFF);

    printf("\nInvalid VRAM read unmapped addr:%08x sz:%d", addr, sz);
    dbg_break("Unmapped VRAM9 read", this->clock.master_cycle_count7);
    return 0;
}

static void buswr9_oam(struct NDS *this, u32 addr, u32 sz, u32 access, u32 val)
{
    addr &= 0x7FF;
    if (addr < 0x400) return cW[sz](this->ppu.eng2d[0].mem.oam, addr & 0x3FF, val);
    else return cW[sz](this->ppu.eng2d[1].mem.oam, addr & 0x3FF, val);
    //buswr9_invalid(this, addr, sz, access, val);
}

static u32 busrd9_oam(struct NDS *this, u32 addr, u32 sz, u32 access, u32 has_effect)
{
    addr &= 0x7FF;
    if (addr < 0x400) return cR[sz](this->ppu.eng2d[0].mem.oam, addr & 0x3FF);
    else return cR[sz](this->ppu.eng2d[1].mem.oam, addr & 0x3FF);
}

static u32 busrd7_bios7(struct NDS *this, u32 addr, u32 sz, u32 access, u32 has_effect)
{
    return cR[sz](this->mem.bios7, addr & 0x3FFF);
}

static void buswr7_bios7(struct NDS *this, u32 addr, u32 sz, u32 access, u32 val)
{
}

static u32 busrd7_main(struct NDS *this, u32 addr, u32 sz, u32 access, u32 has_effect)
{
    return cR[sz](this->mem.RAM, addr & 0x3FFFFF);
}

static void buswr7_main(struct NDS *this, u32 addr, u32 sz, u32 access, u32 val)
{
    cW[sz](this->mem.RAM, addr & 0x3FFFFF, val);
}

static u32 DMA_CH_NUM(u32 addr)
{
    addr &= 0xFF;
    if (addr < 0xBC) return 0;
    if (addr < 0xC8) return 1;
    if (addr < 0xD4) return 2;
    return 3;
}

static u32 busrd7_io8(struct NDS *this, u32 addr, u32 sz, u32 access, u32 has_effect)
{
    u32 v;
    switch(addr) {
        case R7_SOUNDCNT+0:
            return this->apu.io.master_vol;
        case R7_SOUNDCNT+1:
            v = this->apu.io.left_output_from;
            v |= this->apu.io.right_output_from << 2;
            v |= this->apu.io.output_ch1_from_mixer << 4;
            v |= this->apu.io.output_ch3_from_mixer << 5;
            v |= this->apu.master_enable << 7;
            return v;
        case R7_SOUNDCNT+2:
        case R7_SOUNDCNT+3:
            return 0;

        case R7_SOUNDBIAS+0:
            return this->apu.io.SOUNDBIAS & 0xFF;
        case R7_SOUNDBIAS+1:
            return (this->apu.io.SOUNDBIAS >> 8) & 3;

        case R_RCNT+0: return this->io.sio_data & 0xFF;
        case R_RCNT+1: return this->io.sio_data >> 8;
        case 0x04000138:
        case 0x04000139: return 0;
        case R7_SPICNT+0:
            this->spi.cnt.busy = NDS_clock_current7(this) < this->spi.busy_until;
            return this->spi.cnt.u & 0xFF;
        case R7_SPICNT+1:
            return this->spi.cnt.u >> 8;

        case R_POSTFLG:
            return this->io.arm7.POSTFLG;
        case R_POSTFLG+1:
            return 0;

        case R7_WIFIWAITCNT:
            return this->io.powcnt.wifi ? this->io.powcnt.wifi_waitcnt : 0;
        case R7_WIFIWAITCNT+1:
            return 0;

        case R7_POWCNT2+0:
            v = this->io.powcnt.speakers;
            v |= this->io.powcnt.wifi << 1;
            return v;
        case R7_POWCNT2+1:
            return 0;

        case R_KEYINPUT+0: // buttons!!!
            return NDS_get_controller_state(this, 0);
        case R_KEYINPUT+1: // buttons!!!
            return NDS_get_controller_state(this, 1);
        case R_EXTKEYIN+0:
            return NDS_get_controller_state(this, 2);
        case R_EXTKEYIN+1:
            return 0;

        case R_IPCFIFOCNT+0:
            // send fifo from 7 is to_9
            v = NDS_IPC_fifo_is_empty(&this->io.ipc.to_arm9);
            v |= NDS_IPC_fifo_is_full(&this->io.ipc.to_arm9) << 1;
            v |= this->io.ipc.arm7.irq_on_send_fifo_empty << 2;
            return v;
        case R_IPCFIFOCNT+1:
            v = NDS_IPC_fifo_is_empty(&this->io.ipc.to_arm7);
            v |= NDS_IPC_fifo_is_full(&this->io.ipc.to_arm7) << 1;
            v |= this->io.ipc.arm7.irq_on_recv_fifo_not_empty << 2;
            v |= this->io.ipc.arm7.error << 6;
            v |= this->io.ipc.arm7.fifo_enable << 7;
            return v;


        case R_IPCSYNC+0:
            return this->io.ipc.arm7sync.dinput;
        case R_IPCSYNC+1:
            v = this->io.ipc.arm7sync.doutput;
            v |= this->io.ipc.arm7sync.enable_irq_from_remote << 6;
            return v;
        case R_IPCSYNC+2:
        case R_IPCSYNC+3:
            return 0;

        case R_IME: return this->io.arm7.IME;
        case R_IME+1: return 0;
        case R_IME+2: return 0;
        case R_IME+3: return 0;
        case R_IE+0: return this->io.arm7.IE & 0xFF;
        case R_IE+1: return (this->io.arm7.IE >> 8) & 0xFF;
        case R_IE+2: return (this->io.arm7.IE >> 16) & 0xFF;
        case R_IE+3: return (this->io.arm7.IE >> 24) & 0xFF;
        case R_IF+0: return this->io.arm7.IF & 0xFF;
        case R_IF+1: return (this->io.arm7.IF >> 8) & 0xFF;
        case R_IF+2: return (this->io.arm7.IF >> 16) & 0xFF;
        case R_IF+3: return (this->io.arm7.IF >> 24) & 0xFF;

        case R_DMA0SAD+0: return this->dma9[0].io.src_addr & 0xFF;
        case R_DMA0SAD+1: return (this->dma9[0].io.src_addr >> 8) & 0xFF;
        case R_DMA0SAD+2: return (this->dma9[0].io.src_addr >> 16) & 0xFF;
        case R_DMA0SAD+3: return (this->dma9[0].io.src_addr >> 24) & 0xFF;
        case R_DMA0DAD+0: return this->dma9[0].io.dest_addr & 0xFF;
        case R_DMA0DAD+1: return (this->dma9[0].io.dest_addr >> 8) & 0xFF;
        case R_DMA0DAD+2: return (this->dma9[0].io.dest_addr >> 16) & 0xFF;
        case R_DMA0DAD+3: return (this->dma9[0].io.dest_addr >> 24) & 0xFF;

        case R_DMA1SAD+0: return this->dma9[1].io.src_addr & 0xFF;
        case R_DMA1SAD+1: return (this->dma9[1].io.src_addr >> 8) & 0xFF;
        case R_DMA1SAD+2: return (this->dma9[1].io.src_addr >> 16) & 0xFF;
        case R_DMA1SAD+3: return (this->dma9[1].io.src_addr >> 24) & 0xFF;
        case R_DMA1DAD+0: return this->dma9[1].io.dest_addr & 0xFF;
        case R_DMA1DAD+1: return (this->dma9[1].io.dest_addr >> 8) & 0xFF;
        case R_DMA1DAD+2: return (this->dma9[1].io.dest_addr >> 16) & 0xFF;
        case R_DMA1DAD+3: return (this->dma9[1].io.dest_addr >> 24) & 0xFF;

        case R_DMA2SAD+0: return this->dma9[2].io.src_addr & 0xFF;
        case R_DMA2SAD+1: return (this->dma9[2].io.src_addr >> 8) & 0xFF;
        case R_DMA2SAD+2: return (this->dma9[2].io.src_addr >> 16) & 0xFF;
        case R_DMA2SAD+3: return (this->dma9[2].io.src_addr >> 24) & 0xFF;
        case R_DMA2DAD+0: return this->dma9[2].io.dest_addr & 0xFF;
        case R_DMA2DAD+1: return (this->dma9[2].io.dest_addr >> 8) & 0xFF;
        case R_DMA2DAD+2: return (this->dma9[2].io.dest_addr >> 16) & 0xFF;
        case R_DMA2DAD+3: return (this->dma9[2].io.dest_addr >> 24) & 0xFF;

        case R_DMA3SAD+0: return this->dma9[3].io.src_addr & 0xFF;
        case R_DMA3SAD+1: return (this->dma9[3].io.src_addr >> 8) & 0xFF;
        case R_DMA3SAD+2: return (this->dma9[3].io.src_addr >> 16) & 0xFF;
        case R_DMA3SAD+3: return (this->dma9[3].io.src_addr >> 24) & 0xFF;
        case R_DMA3DAD+0: return this->dma9[3].io.dest_addr & 0xFF;
        case R_DMA3DAD+1: return (this->dma9[3].io.dest_addr >> 8) & 0xFF;
        case R_DMA3DAD+2: return (this->dma9[3].io.dest_addr >> 16) & 0xFF;
        case R_DMA3DAD+3: return (this->dma9[3].io.dest_addr >> 24) & 0xFF;

        case R_DMA0CNT_L+0: return this->dma9[0].io.word_count & 0xFF;
        case R_DMA0CNT_L+1: return (this->dma9[0].io.word_count >> 8) & 0xFF;
        case R_DMA1CNT_L+0: return this->dma9[1].io.word_count & 0xFF;
        case R_DMA1CNT_L+1: return (this->dma9[1].io.word_count >> 8) & 0xFF;
        case R_DMA2CNT_L+0: return this->dma9[2].io.word_count & 0xFF;
        case R_DMA2CNT_L+1: return (this->dma9[2].io.word_count >> 8) & 0xFF;
        case R_DMA3CNT_L+0: return this->dma9[3].io.word_count & 0xFF;
        case R_DMA3CNT_L+1: return (this->dma9[3].io.word_count >> 8) & 0xFF;

        case R_DMA0CNT_H+0:
        case R_DMA1CNT_H+0:
        case R_DMA2CNT_H+0:
        case R_DMA3CNT_H+0: {
            struct NDS_DMA_ch *ch = &this->dma9[DMA_CH_NUM(addr)];
            v = ch->io.dest_addr_ctrl << 5;
            v |= (ch->io.src_addr_ctrl & 1) << 7;
            return v; }

        case R_DMA0CNT_H+1:
        case R_DMA1CNT_H+1:
        case R_DMA2CNT_H+1:
        case R_DMA3CNT_H+1: {
            u32 chnum = DMA_CH_NUM(addr);
            struct NDS_DMA_ch *ch = &this->dma9[chnum];
            v = ch->io.src_addr_ctrl = (ch->io.src_addr_ctrl >> 1) & 1;
            v |= ch->io.repeat << 1;
            v |= ch->io.transfer_size << 2;
            v |= ch->io.start_timing << 4;
            v |= ch->io.irq_on_end << 6;

            v |= ch->io.enable << 7;
            return v;}

        case R_TM0CNT_L+0: return (NDS_read_timer7(this, 0) >> 0) & 0xFF;
        case R_TM0CNT_L+1: return (NDS_read_timer7(this, 0) >> 8) & 0xFF;
        case R_TM1CNT_L+0: return (NDS_read_timer7(this, 1) >> 0) & 0xFF;
        case R_TM1CNT_L+1: return (NDS_read_timer7(this, 1) >> 8) & 0xFF;
        case R_TM2CNT_L+0: return (NDS_read_timer7(this, 2) >> 0) & 0xFF;
        case R_TM2CNT_L+1: return (NDS_read_timer7(this, 2) >> 8) & 0xFF;
        case R_TM3CNT_L+0: return (NDS_read_timer7(this, 3) >> 0) & 0xFF;
        case R_TM3CNT_L+1: return (NDS_read_timer7(this, 3) >> 8) & 0xFF;

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
            v = this->timer7[tn].divider.io;
            v |= this->timer7[tn].cascade << 2;
            v |= this->timer7[tn].irq_on_overflow << 6;
            v |= NDS_timer7_enabled(this, tn) << 7;
            return v;
        }

        case R7_VRAMSTAT:
            v = this->mem.vram.io.bank[NVC].enable && (this->mem.vram.io.bank[NVC].mst == 2);
            v |= (this->mem.vram.io.bank[NVD].enable && (this->mem.vram.io.bank[NVD].mst == 2)) << 1;
            return v;
        case R7_WRAMSTAT:
            return this->mem.io.RAM9.val;
        case R7_EXMEMSTAT+0:
            return (this->io.arm7.EXMEM & 0x7F) | (this->io.arm9.EXMEM & 0x80);
        case R7_EXMEMSTAT+1:
            return (this->io.arm9.EXMEM >> 8) | (1 << 5);
    }
    printf("\nUnhandled BUSRD7IO8 addr:%08x", addr);
    return 0;
}

static void start_div(struct NDS *this)
{
    // Set time and needs calculation
    this->io.div.needs_calc = 1;
    u64 num_clks = 20;
    switch(this->io.div.mode) {
        case 0:
            num_clks = 18;
        case 1:
        case 2:
            num_clks = 34;
    }
    this->io.div.busy_until = NDS_clock_current9(this) + num_clks;
}

static void start_sqrt(struct NDS *this)
{
    this->io.sqrt.needs_calc = 1;
    this->io.div.busy_until = NDS_clock_current9(this) + 13;
}

static void div_calc(struct NDS *this)
{
    this->io.div.needs_calc = 0;

    switch (this->io.div.mode) {
        case 0: {
            i32 num = (i32)this->io.div.numer.data32[0];
            i32 den = (i32)this->io.div.denom.data32[0];
            if (den == 0) {
                this->io.div.result.data32[0] = (num<0) ? 1:-1;
                this->io.div.result.data32[1] = (num<0) ? -1:0;
                this->io.div.remainder.u = num;
            }
            else if (num == -0x80000000 && den == -1) {
                this->io.div.result.u = 0x80000000;
            }
            else {
                this->io.div.result.u = (i64)(num / den);
                this->io.div.remainder.u = (i64)(num % den);
            }
            break; }

        case 1:
        case 3: {
            i64 num = (i64)this->io.div.numer.u;
            i32 den = (i32)this->io.div.denom.data32[0];
            if (den == 0) {
                this->io.div.result.u = (num<0) ? 1:-1;
                this->io.div.remainder.u = num;
            }
            else if (num == -0x8000000000000000 && den == -1) {
                this->io.div.result.u = 0x8000000000000000;
                this->io.div.remainder.u = 0;
            }
            else {
                this->io.div.result.u = (i64)(num / den);
                this->io.div.remainder.u = (i64)(num % den);
            }
            break; }

        case 2: {
            i64 num = (i64)this->io.div.numer.u;
            i64 den = (i64)this->io.div.denom.u;
            if (den == 0) {
                this->io.div.result.u = (num<0) ? 1:-1;
                this->io.div.remainder.u = num;
            }
            else if (num == -0x8000000000000000 && den == -1) {
                this->io.div.result.u = 0x8000000000000000;
                this->io.div.remainder.u = 0;
            }
            else {
                this->io.div.result.u = (i64)(num / den);
                this->io.div.remainder.u = (i64)(num % den);
            }
            break; }
    }

    this->io.div.by_zero |= this->io.div.denom.u == 0;
}

static void sqrt_calc(struct NDS *this)
{
    this->io.sqrt.needs_calc = 0;
    u64 val;
    u32 res = 0;
    u64 rem = 0;
    u32 prod = 0;
    u32 nbits, topshift;

    if (this->io.sqrt.mode)
    {
        val = this->io.sqrt.param.u;
        nbits = 32;
        topshift = 62;
    }
    else
    {
        val = (u64)this->io.sqrt.param.data32[0];
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

    this->io.sqrt.result.u = res;
}

static void buswr7_io8(struct NDS *this, u32 addr, u32 sz, u32 access, u32 val)
{
    switch(addr) {

        case R7_SOUNDCNT+0:
            this->apu.io.master_vol = val & 0x7F;
            return;
        case R7_SOUNDCNT+1:
            this->apu.io.left_output_from = val & 3;
            this->apu.io.right_output_from = (val >> 2) & 3;
            this->apu.io.output_ch1_from_mixer = (val >> 4) & 1;
            this->apu.io.output_ch3_from_mixer = (val >> 5) & 1;
            this->apu.master_enable = (val >> 7) & 1;
            return;
        case R7_SOUNDCNT+2:
        case R7_SOUNDCNT+3:
            return;

        case R7_SOUNDBIAS+0:
            this->apu.io.SOUNDBIAS = (this->apu.io.SOUNDBIAS & 0x300) | val;
            return;
        case R7_SOUNDBIAS+1:
            this->apu.io.SOUNDBIAS = (this->apu.io.SOUNDBIAS & 0xFF) | ((val & 3) << 8);
            return;

        case R_RCNT+0:
            this->io.sio_data = (this->io.sio_data & 0xFF00) | val;
            return;
        case R_RCNT+1:
            this->io.sio_data = (this->io.sio_data & 0xFF) | (val << 8);
            return;
        case R_ROMCMD+0:
        case R_ROMCMD+1:
        case R_ROMCMD+2:
        case R_ROMCMD+3:
        case R_ROMCMD+4:
        case R_ROMCMD+5:
        case R_ROMCMD+6:
        case R_ROMCMD+7:
            NDS_cart_write_cmd(this, addr - R_ROMCMD, val);
            return;

        case R7_SPICNT+0:
            this->spi.cnt.u = (this->spi.cnt.u & 0xFF80) | (val & 0b00000011);
            return;
        case R7_SPICNT+1: {
            if ((val & 0x80) && (!this->spi.cnt.bus_enable)) {
                // Enabling the bus releases hold on current device
                this->spi.chipsel = 0;
                NDS_SPI_release_hold(this);
            }
            // Don't change device while chipsel is hi?
            //if (this->spi.cnt.chipselect_hold)
            if (false)
                val = (val & 0b11111100) | this->spi.cnt.device;

            this->spi.cnt.u = (this->spi.cnt.u & 0xFF) | ((val & 0b11001111) << 8);
            return; }

        case R_POSTFLG:
            this->io.arm7.POSTFLG |= val & 1;
            return;

        case R7_WIFIWAITCNT+0:
            if (this->io.powcnt.wifi)
                this->io.powcnt.wifi_waitcnt = val;
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
                    dbgloglog(NDS_CAT_ARM7_HALT, DBGLS_INFO, "HALT ARM7 cyc:%lld", NDS_clock_current7(this));
                    this->io.arm7.halted = 1;
                    return;
                case 3:
                    printf("\nWARNING SLEEP MODE ATTEMPT");
                    return;
            }
            return; }

        case R7_POWCNT2+0:
            this->io.powcnt.speakers = val & 1;
            this->io.powcnt.wifi = (val >> 1) & 1;
            return;
        case R7_POWCNT2+1:
            return;

        case R_KEYCNT+0:
            this->io.arm7.button_irq.buttons = (this->io.arm7.button_irq.buttons & 0b1100000000) | val;
            return;
        case R_KEYCNT+1: {
            this->io.arm7.button_irq.buttons = (this->io.arm7.button_irq.buttons & 0xFF) | ((val & 0b11) << 8);
            u32 old_enable = this->io.arm7.button_irq.enable;
            this->io.arm7.button_irq.enable = (val >> 6) & 1;
            if ((old_enable == 0) && this->io.arm7.button_irq.enable) {
                printf("\nWARNING BUTTON IRQ ENABLED ARM9...");
            }
            this->io.arm7.button_irq.condition = (val >> 7) & 1;
            return; }

        case R_IPCFIFOCNT+0: {
            u32 old_bits = this->io.ipc.arm7.irq_on_send_fifo_empty & NDS_IPC_fifo_is_empty(&this->io.ipc.to_arm9);
            this->io.ipc.arm7.irq_on_send_fifo_empty = (val >> 2) & 1;
            if ((val >> 3) & 1) { // arm7's send fifo is to_arm9
                NDS_IPC_empty_fifo(this, &this->io.ipc.to_arm9);
            }
            // Edge-sensitive trigger...
            if (this->io.ipc.arm7.irq_on_send_fifo_empty & !old_bits) {
                NDS_update_IF7(this, NDS_IRQ_IPC_SEND_EMPTY);
            }
            return; }
        case R_IPCFIFOCNT+1: {
            u32 old_bits = this->io.ipc.arm7.irq_on_recv_fifo_not_empty & NDS_IPC_fifo_is_not_empty(&this->io.ipc.to_arm7);
            this->io.ipc.arm7.irq_on_recv_fifo_not_empty = (val >> 2) & 1;
            if ((val >> 6) & 1) this->io.ipc.arm7.error = 0;
            this->io.ipc.arm7.fifo_enable = (val >> 7) & 1;
            u32 new_bits = this->io.ipc.arm7.irq_on_recv_fifo_not_empty & NDS_IPC_fifo_is_not_empty(&this->io.ipc.to_arm7);
            if (!old_bits && new_bits) {
                NDS_update_IF7(this, NDS_IRQ_IPC_RECV_NOT_EMPTY);
            }
            return; }

        case R_IPCSYNC+0:
        case R_IPCSYNC+2:
        case R_IPCSYNC+3:
            return;
        case R_IPCSYNC+1:
            this->io.ipc.arm9sync.dinput = this->io.ipc.arm7sync.doutput = val & 15;

            u32 send_irq = (val >> 5) & 1;
            if (send_irq && this->io.ipc.arm9sync.enable_irq_from_remote) {
                NDS_update_IF9(this, NDS_IRQ_IPC_SYNC);
            }
            this->io.ipc.arm7sync.enable_irq_from_remote = (val >> 6) & 1;
            return;

        case R_IME: this->io.arm7.IME = val & 1; NDS_eval_irqs_7(this); return;
        case R_IME+1: return;
        case R_IME+2: return;
        case R_IME+3: return;
        case R_IF+0: this->io.arm7.IF &= ~val; NDS_eval_irqs_7(this); return;
        case R_IF+1: this->io.arm7.IF &= ~(val << 8); NDS_eval_irqs_7(this); return;
        case R_IF+2: this->io.arm7.IF &= ~(val << 16); NDS_eval_irqs_7(this); return;
        case R_IF+3: this->io.arm7.IF &= ~(val << 24); NDS_eval_irqs_7(this); return;

        case R_IE+0:
            this->io.arm7.IE = (this->io.arm7.IE & 0xFF00) | (val & 0xFF);
            this->io.arm7.IE &= ~0x80; // bit 7 doesn't get set on ARM9
            NDS_eval_irqs_7(this);
            return;
        case R_IE+1:
            this->io.arm7.IE = (this->io.arm7.IE & 0xFF) | (val << 8);
            NDS_eval_irqs_7(this);
            return;
        case R_IE+2:
            this->io.arm7.IE = (this->io.arm7.IE & 0xFF00FFFF) | (val << 16);
            NDS_eval_irqs_7(this);
            return;
        case R_IE+3:
            this->io.arm7.IE = (this->io.arm7.IE & 0x00FFFFFF) | (val << 24);
            NDS_eval_irqs_7(this);
            return;

        case R_DMA0SAD+0: this->dma7[0].io.src_addr = (this->dma7[0].io.src_addr & 0xFFFFFF00) | (val << 0); return; // DMA source address ch0
        case R_DMA0SAD+1: this->dma7[0].io.src_addr = (this->dma7[0].io.src_addr & 0xFFFF00FF) | (val << 8); return; // DMA source address ch0
        case R_DMA0SAD+2: this->dma7[0].io.src_addr = (this->dma7[0].io.src_addr & 0xFF00FFFF) | (val << 16); return; // DMA source address ch0
        case R_DMA0SAD+3: this->dma7[0].io.src_addr = (this->dma7[0].io.src_addr & 0x00FFFFFF) | ((val & 0x07) << 24); return; // DMA source address ch0
        case R_DMA0DAD+0: this->dma7[0].io.dest_addr = (this->dma7[0].io.dest_addr & 0xFFFFFF00) | (val << 0); return; // DMA source address ch0
        case R_DMA0DAD+1: this->dma7[0].io.dest_addr = (this->dma7[0].io.dest_addr & 0xFFFF00FF) | (val << 8); return; // DMA source address ch0
        case R_DMA0DAD+2: this->dma7[0].io.dest_addr = (this->dma7[0].io.dest_addr & 0xFF00FFFF) | (val << 16); return; // DMA source address ch0
        case R_DMA0DAD+3: this->dma7[0].io.dest_addr = (this->dma7[0].io.dest_addr & 0x00FFFFFF) | ((val & 0x07) << 24); return; // DMA source address ch0

        case R_DMA1SAD+0: this->dma7[1].io.src_addr = (this->dma7[1].io.src_addr & 0xFFFFFF00) | (val << 0); return; // DMA source address ch0
        case R_DMA1SAD+1: this->dma7[1].io.src_addr = (this->dma7[1].io.src_addr & 0xFFFF00FF) | (val << 8); return; // DMA source address ch0
        case R_DMA1SAD+2: this->dma7[1].io.src_addr = (this->dma7[1].io.src_addr & 0xFF00FFFF) | (val << 16); return; // DMA source address ch0
        case R_DMA1SAD+3: this->dma7[1].io.src_addr = (this->dma7[1].io.src_addr & 0x00FFFFFF) | ((val & 0x0F) << 24); return; // DMA source address ch0
        case R_DMA1DAD+0: this->dma7[1].io.dest_addr = (this->dma7[1].io.dest_addr & 0xFFFFFF00) | (val << 0); return; // DMA source address ch0
        case R_DMA1DAD+1: this->dma7[1].io.dest_addr = (this->dma7[1].io.dest_addr & 0xFFFF00FF) | (val << 8); return; // DMA source address ch0
        case R_DMA1DAD+2: this->dma7[1].io.dest_addr = (this->dma7[1].io.dest_addr & 0xFF00FFFF) | (val << 16); return; // DMA source address ch0
        case R_DMA1DAD+3: this->dma7[1].io.dest_addr = (this->dma7[1].io.dest_addr & 0x00FFFFFF) | ((val & 0x07) << 24); return; // DMA source address ch0

        case R_DMA2SAD+0: this->dma7[2].io.src_addr = (this->dma7[2].io.src_addr & 0xFFFFFF00) | (val << 0); return; // DMA source address ch0
        case R_DMA2SAD+1: this->dma7[2].io.src_addr = (this->dma7[2].io.src_addr & 0xFFFF00FF) | (val << 8); return; // DMA source address ch0
        case R_DMA2SAD+2: this->dma7[2].io.src_addr = (this->dma7[2].io.src_addr & 0xFF00FFFF) | (val << 16); return; // DMA source address ch0
        case R_DMA2SAD+3: this->dma7[2].io.src_addr = (this->dma7[2].io.src_addr & 0x00FFFFFF) | ((val & 0x0F) << 24); return; // DMA source address ch0
        case R_DMA2DAD+0: this->dma7[2].io.dest_addr = (this->dma7[2].io.dest_addr & 0xFFFFFF00) | (val << 0); return; // DMA source address ch0
        case R_DMA2DAD+1: this->dma7[2].io.dest_addr = (this->dma7[2].io.dest_addr & 0xFFFF00FF) | (val << 8); return; // DMA source address ch0
        case R_DMA2DAD+2: this->dma7[2].io.dest_addr = (this->dma7[2].io.dest_addr & 0xFF00FFFF) | (val << 16); return; // DMA source address ch0
        case R_DMA2DAD+3: this->dma7[2].io.dest_addr = (this->dma7[2].io.dest_addr & 0x00FFFFFF) | ((val & 0x07) << 24); return; // DMA source address ch0

        case R_DMA3SAD+0: this->dma7[3].io.src_addr = (this->dma7[3].io.src_addr & 0xFFFFFF00) | (val << 0); return; // DMA source address ch0
        case R_DMA3SAD+1: this->dma7[3].io.src_addr = (this->dma7[3].io.src_addr & 0xFFFF00FF) | (val << 8); return; // DMA source address ch0
        case R_DMA3SAD+2: this->dma7[3].io.src_addr = (this->dma7[3].io.src_addr & 0xFF00FFFF) | (val << 16); return; // DMA source address ch0
        case R_DMA3SAD+3: this->dma7[3].io.src_addr = (this->dma7[3].io.src_addr & 0x00FFFFFF) | ((val & 0x0F) << 24); return; // DMA source address ch0
        case R_DMA3DAD+0: this->dma7[3].io.dest_addr = (this->dma7[3].io.dest_addr & 0xFFFFFF00) | (val << 0); return; // DMA source address ch0
        case R_DMA3DAD+1: this->dma7[3].io.dest_addr = (this->dma7[3].io.dest_addr & 0xFFFF00FF) | (val << 8); return; // DMA source address ch0
        case R_DMA3DAD+2: this->dma7[3].io.dest_addr = (this->dma7[3].io.dest_addr & 0xFF00FFFF) | (val << 16); return; // DMA source address ch0
        case R_DMA3DAD+3: this->dma7[3].io.dest_addr = (this->dma7[3].io.dest_addr & 0x00FFFFFF) | ((val & 0x0F) << 24); return; // DMA source address ch0

        case R_DMA0CNT_L+0: this->dma7[0].io.word_count = (this->dma7[0].io.word_count & 0x3F00) | (val << 0); return;
        case R_DMA0CNT_L+1: this->dma7[0].io.word_count = (this->dma7[0].io.word_count & 0xFF) | ((val & 0x3F) << 8); return;
        case R_DMA1CNT_L+0: this->dma7[1].io.word_count = (this->dma7[1].io.word_count & 0x3F00) | (val << 0); return;
        case R_DMA1CNT_L+1: this->dma7[1].io.word_count = (this->dma7[1].io.word_count & 0xFF) | ((val & 0x3F) << 8); return;
        case R_DMA2CNT_L+0: this->dma7[2].io.word_count = (this->dma7[2].io.word_count & 0x3F00) | (val << 0); return;
        case R_DMA2CNT_L+1: this->dma7[2].io.word_count = (this->dma7[2].io.word_count & 0xFF) | ((val & 0x3F) << 8); return;
        case R_DMA3CNT_L+0: this->dma7[3].io.word_count = (this->dma7[3].io.word_count & 0xFF00) | (val << 0); return;
        case R_DMA3CNT_L+1: this->dma7[3].io.word_count = (this->dma7[3].io.word_count & 0xFF) | ((val & 0xFF) << 8); return;

        case R_DMA0CNT_H+0:
        case R_DMA1CNT_H+0:
        case R_DMA2CNT_H+0:
        case R_DMA3CNT_H+0: {
            struct NDS_DMA_ch *ch = &this->dma7[DMA_CH_NUM(addr)];
            ch->io.dest_addr_ctrl = (val >> 5) & 3;
            ch->io.src_addr_ctrl = (ch->io.src_addr_ctrl & 2) | ((val >> 7) & 1);
            return;}

        case R_DMA0CNT_H+1:
        case R_DMA1CNT_H+1:
        case R_DMA2CNT_H+1:
        case R_DMA3CNT_H+1: {
            u32 chnum = DMA_CH_NUM(addr);
            struct NDS_DMA_ch *ch = &this->dma7[chnum];
            ch->io.src_addr_ctrl = (ch->io.src_addr_ctrl & 1) | ((val & 1) << 1);
            ch->io.repeat = (val >> 1) & 1;
            ch->io.transfer_size = (val >> 2) & 1;
            ch->io.start_timing = (val >> 4) & 3;
            if (ch->io.start_timing >= 2) {
                printf("\nWARN START TIMING %d NOT IMPLEMENT FOR ARM7 DMA!", ch->io.start_timing);
            }
            ch->io.irq_on_end = (val >> 6) & 1;
            u32 old_enable = ch->io.enable;
            ch->io.enable = (val >> 7) & 1;
            if ((ch->io.enable == 1) && (old_enable == 0)) {
                ch->op.first_run = 1;
                if (ch->io.start_timing == 0) {
                    NDS_dma7_start(this, ch, chnum);
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
            NDS_timer7_write_cnt(this, tn, val);
            return; }

        case R_TM0CNT_L+0: this->timer7[0].reload = (this->timer7[0].reload & 0xFF00) | val; return;
        case R_TM1CNT_L+0: this->timer7[1].reload = (this->timer7[1].reload & 0xFF00) | val; return;
        case R_TM2CNT_L+0: this->timer7[2].reload = (this->timer7[2].reload & 0xFF00) | val; return;
        case R_TM3CNT_L+0: this->timer7[3].reload = (this->timer7[3].reload & 0xFF00) | val; return;

        case R_TM0CNT_L+1: this->timer7[0].reload = (this->timer7[0].reload & 0xFF) | (val << 8); return;
        case R_TM1CNT_L+1: this->timer7[1].reload = (this->timer7[1].reload & 0xFF) | (val << 8); return;
        case R_TM2CNT_L+1: this->timer7[2].reload = (this->timer7[2].reload & 0xFF) | (val << 8); return;
        case R_TM3CNT_L+1: this->timer7[3].reload = (this->timer7[3].reload & 0xFF) | (val << 8); return;

        case R7_BIOSPROT+0:
            this->io.arm7.BIOSPROT = (this->io.arm7.BIOSPROT & 0xFF00) | val;
            return;
        case R7_BIOSPROT+1:
            this->io.arm7.BIOSPROT = (this->io.arm7.BIOSPROT & 0xFF) | (val << 8);
            return;
        case R7_EXMEMSTAT+0:
            this->io.arm7.EXMEM = val & 0x7F;
            return;
        case R7_EXMEMSTAT+1:
            return;

    }
    printf("\nUnhandled BUSWR7IO8 addr:%08x val:%08x", addr, val);
}

// --------------
static u32 busrd9_io8(struct NDS *this, u32 addr, u32 sz, u32 access, u32 has_effect)
{
    if (((addr >= 0x04000000) && (addr < 0x04000070)) || ((addr >= 0x04001000) && (addr < 0x04001070))) {
        return NDS_PPU_read9_io(this, addr, sz, access, has_effect);
    }
    u32 v;
    switch(addr) {
        case 0x04004000:
            // NDS thing
            return 0;
        case R9_POWCNT1+0:
            v = this->io.powcnt.lcd_enable;
            v |= this->ppu.eng2d[0].enable << 1;
            v |= this->re.enable << 2;
            v |= this->ge.enable << 3;
            return v;
        case R9_POWCNT1+1:
            v = this->ppu.eng2d[1].enable << 1;
            v |= this->ppu.io.display_swap << 7;
            return v;

        case R_KEYINPUT+0: // buttons!!!
            return NDS_get_controller_state(this, 0);
        case R_KEYINPUT+1: // buttons!!!
            return NDS_get_controller_state(this, 1);
        case R_EXTKEYIN+0:
            return NDS_get_controller_state(this, 2);
        case R_EXTKEYIN+1:
            return 0;

        case R_IPCFIFOCNT+0:
            // send fifo from 9 is to_7
            v = NDS_IPC_fifo_is_empty(&this->io.ipc.to_arm7);
            v |= NDS_IPC_fifo_is_full(&this->io.ipc.to_arm7) << 1;
            v |= this->io.ipc.arm9.irq_on_send_fifo_empty << 2;
            return v;
        case R_IPCFIFOCNT+1:
            v = NDS_IPC_fifo_is_empty(&this->io.ipc.to_arm9);
            v |= NDS_IPC_fifo_is_full(&this->io.ipc.to_arm9) << 1;
            v |= this->io.ipc.arm9.irq_on_recv_fifo_not_empty << 2;
            v |= this->io.ipc.arm9.error << 6;
            v |= this->io.ipc.arm9.fifo_enable << 7;
            return v;

        case R_IPCSYNC+0:
            return this->io.ipc.arm9sync.dinput;
        case R_IPCSYNC+1:
            v = this->io.ipc.arm9sync.doutput;
            v |= this->io.ipc.arm9sync.enable_irq_from_remote << 6;
            return v;
        case R_IPCSYNC+2:
        case R_IPCSYNC+3:
            return 0;

        case R9_DIVCNT+0:
            v = this->io.div.mode;
            return v;
        case R9_DIVCNT+1:
            v = this->io.div.by_zero << 6;
            v |= (NDS_clock_current9(this) < this->io.div.busy_until) << 7;
            return v;

        case R9_DIV_NUMER+0:
        case R9_DIV_NUMER+1:
        case R9_DIV_NUMER+2:
        case R9_DIV_NUMER+3:
        case R9_DIV_NUMER+4:
        case R9_DIV_NUMER+5:
        case R9_DIV_NUMER+6:
        case R9_DIV_NUMER+7:
            return this->io.div.numer.data[addr & 7];

        case R9_DIV_DENOM+0:
        case R9_DIV_DENOM+1:
        case R9_DIV_DENOM+2:
        case R9_DIV_DENOM+3:
        case R9_DIV_DENOM+4:
        case R9_DIV_DENOM+5:
        case R9_DIV_DENOM+6:
        case R9_DIV_DENOM+7:
            return this->io.div.denom.data[addr & 7];

        case R9_DIV_RESULT+0:
        case R9_DIV_RESULT+1:
        case R9_DIV_RESULT+2:
        case R9_DIV_RESULT+3:
        case R9_DIV_RESULT+4:
        case R9_DIV_RESULT+5:
        case R9_DIV_RESULT+6:
        case R9_DIV_RESULT+7:
            if (this->io.div.needs_calc) div_calc(this);
            return this->io.div.result.data[addr & 7];

        case R9_DIVREM_RESULT+0:
        case R9_DIVREM_RESULT+1:
        case R9_DIVREM_RESULT+2:
        case R9_DIVREM_RESULT+3:
        case R9_DIVREM_RESULT+4:
        case R9_DIVREM_RESULT+5:
        case R9_DIVREM_RESULT+6:
        case R9_DIVREM_RESULT+7:
            if (this->io.div.needs_calc) div_calc(this);
            return this->io.div.remainder.data[addr & 7];

        case R9_SQRTCNT+0:
            return this->io.sqrt.mode;
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
            return this->io.sqrt.param.data[addr & 7];

        case R9_SQRT_RESULT+0:
        case R9_SQRT_RESULT+1:
        case R9_SQRT_RESULT+2:
        case R9_SQRT_RESULT+3:
            if (this->io.sqrt.needs_calc) sqrt_calc(this);
            return this->io.sqrt.result.data[addr & 3];

        case R_IME: return this->io.arm9.IME;
        case R_IME+1: return 0;
        case R_IME+2: return 0;
        case R_IME+3: return 0;
        case R_IE+0: return this->io.arm9.IE & 0xFF;
        case R_IE+1: return (this->io.arm9.IE >> 8) & 0xFF;
        case R_IE+2: return (this->io.arm9.IE >> 16) & 0xFF;
        case R_IE+3: return (this->io.arm9.IE >> 24) & 0xFF;
        case R_IF+0: return this->io.arm9.IF & 0xFF;
        case R_IF+1: return (this->io.arm9.IF >> 8) & 0xFF;
        case R_IF+2: return (this->io.arm9.IF >> 16) & 0xFF;
        case R_IF+3: return (this->io.arm9.IF >> 24) & 0xFF;

        case R9_WRAMCNT:
            return this->mem.io.RAM9.val;
        case R9_EXMEMCNT+0:
            return this->io.arm9.EXMEM & 0xFF;
        case R9_EXMEMCNT+1:
            return (this->io.arm9.EXMEM >> 8) | (1 << 5);

        case R_DMA0SAD+0: return this->dma9[0].io.src_addr & 0xFF;
        case R_DMA0SAD+1: return (this->dma9[0].io.src_addr >> 8) & 0xFF;
        case R_DMA0SAD+2: return (this->dma9[0].io.src_addr >> 16) & 0xFF;
        case R_DMA0SAD+3: return (this->dma9[0].io.src_addr >> 24) & 0xFF;
        case R_DMA0DAD+0: return this->dma9[0].io.dest_addr & 0xFF;
        case R_DMA0DAD+1: return (this->dma9[0].io.dest_addr >> 8) & 0xFF;
        case R_DMA0DAD+2: return (this->dma9[0].io.dest_addr >> 16) & 0xFF;
        case R_DMA0DAD+3: return (this->dma9[0].io.dest_addr >> 24) & 0xFF;

        case R_DMA1SAD+0: return this->dma9[1].io.src_addr & 0xFF;
        case R_DMA1SAD+1: return (this->dma9[1].io.src_addr >> 8) & 0xFF;
        case R_DMA1SAD+2: return (this->dma9[1].io.src_addr >> 16) & 0xFF;
        case R_DMA1SAD+3: return (this->dma9[1].io.src_addr >> 24) & 0xFF;
        case R_DMA1DAD+0: return this->dma9[1].io.dest_addr & 0xFF;
        case R_DMA1DAD+1: return (this->dma9[1].io.dest_addr >> 8) & 0xFF;
        case R_DMA1DAD+2: return (this->dma9[1].io.dest_addr >> 16) & 0xFF;
        case R_DMA1DAD+3: return (this->dma9[1].io.dest_addr >> 24) & 0xFF;

        case R_DMA2SAD+0: return this->dma9[2].io.src_addr & 0xFF;
        case R_DMA2SAD+1: return (this->dma9[2].io.src_addr >> 8) & 0xFF;
        case R_DMA2SAD+2: return (this->dma9[2].io.src_addr >> 16) & 0xFF;
        case R_DMA2SAD+3: return (this->dma9[2].io.src_addr >> 24) & 0xFF;
        case R_DMA2DAD+0: return this->dma9[2].io.dest_addr & 0xFF;
        case R_DMA2DAD+1: return (this->dma9[2].io.dest_addr >> 8) & 0xFF;
        case R_DMA2DAD+2: return (this->dma9[2].io.dest_addr >> 16) & 0xFF;
        case R_DMA2DAD+3: return (this->dma9[2].io.dest_addr >> 24) & 0xFF;

        case R_DMA3SAD+0: return this->dma9[3].io.src_addr & 0xFF;
        case R_DMA3SAD+1: return (this->dma9[3].io.src_addr >> 8) & 0xFF;
        case R_DMA3SAD+2: return (this->dma9[3].io.src_addr >> 16) & 0xFF;
        case R_DMA3SAD+3: return (this->dma9[3].io.src_addr >> 24) & 0xFF;
        case R_DMA3DAD+0: return this->dma9[3].io.dest_addr & 0xFF;
        case R_DMA3DAD+1: return (this->dma9[3].io.dest_addr >> 8) & 0xFF;
        case R_DMA3DAD+2: return (this->dma9[3].io.dest_addr >> 16) & 0xFF;
        case R_DMA3DAD+3: return (this->dma9[3].io.dest_addr >> 24) & 0xFF;

        case R_DMA0CNT_L+0: return this->dma9[0].io.word_count & 0xFF;
        case R_DMA0CNT_L+1: return (this->dma9[0].io.word_count >> 8) & 0xFF;
        case R_DMA1CNT_L+0: return this->dma9[1].io.word_count & 0xFF;
        case R_DMA1CNT_L+1: return (this->dma9[1].io.word_count >> 8) & 0xFF;
        case R_DMA2CNT_L+0: return this->dma9[2].io.word_count & 0xFF;
        case R_DMA2CNT_L+1: return (this->dma9[2].io.word_count >> 8) & 0xFF;
        case R_DMA3CNT_L+0: return this->dma9[3].io.word_count & 0xFF;
        case R_DMA3CNT_L+1: return (this->dma9[3].io.word_count >> 8) & 0xFF;

        case R_DMA0CNT_H+0:
        case R_DMA1CNT_H+0:
        case R_DMA2CNT_H+0:
        case R_DMA3CNT_H+0: {
            struct NDS_DMA_ch *ch = &this->dma9[DMA_CH_NUM(addr)];
            v = ch->io.word_count >> 16;
            v |= ch->io.dest_addr_ctrl << 5;
            v |= (ch->io.src_addr_ctrl & 1) << 7;
            return v; }

        case R_DMA0CNT_H+1:
        case R_DMA1CNT_H+1:
        case R_DMA2CNT_H+1:
        case R_DMA3CNT_H+1: {
            u32 chnum = DMA_CH_NUM(addr);
            struct NDS_DMA_ch *ch = &this->dma9[chnum];
            v = ch->io.src_addr_ctrl = (ch->io.src_addr_ctrl >> 1) & 1;
            v |= ch->io.repeat << 1;
            v |= ch->io.transfer_size << 2;
            v |= ch->io.start_timing << 3;
            v |= ch->io.irq_on_end << 6;

            v |= ch->io.enable << 7;
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
            return this->io.dma.filldata[addr & 15];

        case R_TM0CNT_L+0: return (NDS_read_timer9(this, 0) >> 0) & 0xFF;
        case R_TM0CNT_L+1: return (NDS_read_timer9(this, 0) >> 8) & 0xFF;
        case R_TM1CNT_L+0: return (NDS_read_timer9(this, 1) >> 0) & 0xFF;
        case R_TM1CNT_L+1: return (NDS_read_timer9(this, 1) >> 8) & 0xFF;
        case R_TM2CNT_L+0: return (NDS_read_timer9(this, 2) >> 0) & 0xFF;
        case R_TM2CNT_L+1: return (NDS_read_timer9(this, 2) >> 8) & 0xFF;
        case R_TM3CNT_L+0: return (NDS_read_timer9(this, 3) >> 0) & 0xFF;
        case R_TM3CNT_L+1: return (NDS_read_timer9(this, 3) >> 8) & 0xFF;

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
            v = this->timer9[tn].divider.io;
            v |= this->timer9[tn].cascade << 2;
            v |= this->timer9[tn].irq_on_overflow << 6;
            v |= NDS_timer9_enabled(this, tn) << 7;
            return v;
        }

        case 0x04000240: // These next 4 are write-only
        case 0x04000241:
        case 0x04000242:
        case 0x04000243:
        case 0x04004008: // new DSi stuff libnds cares about?
        case 0x04004009:
        case 0x0400400A:
        case 0x0400400B:
            return 0;
    }
    printf("\nUnhandled BUSRD9IO8 addr:%08x", addr);
    return 0;
}

static void buswr9_io8(struct NDS *this, u32 addr, u32 sz, u32 access, u32 val)
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
            NDS_cart_write_cmd(this, addr - R_ROMCMD, val);
            return;

        case R_POSTFLG:
            this->io.arm9.POSTFLG |= val & 1;
            this->io.arm9.POSTFLG = (this->io.arm9.POSTFLG & 1) | (val & 2);
            return;
        case R9_POWCNT1+0:
            this->io.powcnt.lcd_enable = val & 1;
            this->ppu.eng2d[0].enable = (val >> 1) & 1;
            this->re.enable = (val >> 2) & 1;
            this->ge.enable = (val >> 3) & 1;
            return;
        case R9_POWCNT1+1:
            this->ppu.eng2d[1].enable = (val >> 1) & 1;
            this->ppu.io.display_swap = (val >> 7) & 1;
            return;
        case R9_POWCNT1+2:
        case R9_POWCNT1+3:
            return;
        case R_KEYCNT+0:
            this->io.arm9.button_irq.buttons = (this->io.arm9.button_irq.buttons & 0b1100000000) | val;
            return;
        case R_KEYCNT+1: {
            this->io.arm9.button_irq.buttons = (this->io.arm9.button_irq.buttons & 0xFF) | ((val & 0b11) << 8);
            u32 old_enable = this->io.arm9.button_irq.enable;
            this->io.arm9.button_irq.enable = (val >> 6) & 1;
            if ((old_enable == 0) && this->io.arm9.button_irq.enable) {
                printf("\nWARNING BUTTON IRQ ENABLED ARM9...");
            }
            this->io.arm9.button_irq.condition = (val >> 7) & 1;
            return; }

        case R_IPCFIFOCNT+0: {
            u32 old_bits = this->io.ipc.arm9.irq_on_send_fifo_empty & NDS_IPC_fifo_is_empty(&this->io.ipc.to_arm7);
            this->io.ipc.arm9.irq_on_send_fifo_empty = (val >> 2) & 1;
            if ((val >> 3) & 1) { // arm9's send fifo is to_arm7
                NDS_IPC_empty_fifo(this, &this->io.ipc.to_arm7);
            }
            // Edge-sensitive trigger...
            if (this->io.ipc.arm9.irq_on_send_fifo_empty & !old_bits) {
                NDS_update_IF9(this, NDS_IRQ_IPC_SEND_EMPTY);
            }
            return; }
        case R_IPCFIFOCNT+1: {
            u32 old_bits = this->io.ipc.arm9.irq_on_recv_fifo_not_empty & NDS_IPC_fifo_is_not_empty(&this->io.ipc.to_arm9);
            this->io.ipc.arm9.irq_on_recv_fifo_not_empty = (val >> 2) & 1;
            if ((val >> 6) & 1) this->io.ipc.arm9.error = 0;
            this->io.ipc.arm9.fifo_enable = (val >> 7) & 1;
            u32 new_bits = this->io.ipc.arm9.irq_on_recv_fifo_not_empty & NDS_IPC_fifo_is_not_empty(&this->io.ipc.to_arm9);
            if (!old_bits && new_bits) {
                NDS_update_IF9(this, NDS_IRQ_IPC_RECV_NOT_EMPTY);
            }
            return; }

        case R_IPCSYNC+0:
        case R_IPCSYNC+2:
        case R_IPCSYNC+3:
            return;
        case R_IPCSYNC+1:
            this->io.ipc.arm7sync.dinput = this->io.ipc.arm9sync.doutput = val & 15;
            u32 send_irq = (val >> 5) & 1;
            if (send_irq && this->io.ipc.arm7sync.enable_irq_from_remote) {
                NDS_update_IF7(this, NDS_IRQ_IPC_SYNC);
            }
            this->io.ipc.arm9sync.enable_irq_from_remote = (val >> 6) & 1;
            return;

        case R_IME: this->io.arm9.IME = val & 1; NDS_eval_irqs_9(this); return;
        case R_IME+1: return;
        case R_IME+2: return;
        case R_IME+3: return;
        case R_IF+0: this->io.arm9.IF &= ~val; NDS_eval_irqs_9(this); return;
        case R_IF+1: this->io.arm9.IF &= ~(val << 8); NDS_eval_irqs_9(this); return;
        case R_IF+2: this->io.arm9.IF &= ~(val << 16); NDS_eval_irqs_9(this); return;
        case R_IF+3: this->io.arm9.IF &= ~(val << 24); NDS_eval_irqs_9(this); return;

        case R9_DIVCNT+0:
            this->io.div.mode = val & 3;
            if (this->io.div.mode == 3) {
                printf("\nFORBIDDEN DIV MODE");
            }
            start_div(this);
            return;
        case R9_DIVCNT+1:
            this->io.div.by_zero = (val >> 6) & 1;
            start_div(this);
            return;

        case R9_DIV_NUMER+0:
        case R9_DIV_NUMER+1:
        case R9_DIV_NUMER+2:
        case R9_DIV_NUMER+3:
        case R9_DIV_NUMER+4:
        case R9_DIV_NUMER+5:
        case R9_DIV_NUMER+6:
        case R9_DIV_NUMER+7:
            this->io.div.numer.data[addr & 7] = val;
            start_div(this);
            return;

        case R9_DIV_DENOM+0:
        case R9_DIV_DENOM+1:
        case R9_DIV_DENOM+2:
        case R9_DIV_DENOM+3:
        case R9_DIV_DENOM+4:
        case R9_DIV_DENOM+5:
        case R9_DIV_DENOM+6:
        case R9_DIV_DENOM+7:
            this->io.div.denom.data[addr & 7] = val;
            start_div(this);
            return;

        case R9_SQRTCNT+0:
            this->io.sqrt.mode = val & 1;
            start_sqrt(this);
            return;
        case R9_SQRTCNT+1:
            start_sqrt(this);
            return;

        case R9_SQRT_PARAM+0:
        case R9_SQRT_PARAM+1:
        case R9_SQRT_PARAM+2:
        case R9_SQRT_PARAM+3:
        case R9_SQRT_PARAM+4:
        case R9_SQRT_PARAM+5:
        case R9_SQRT_PARAM+6:
        case R9_SQRT_PARAM+7:
            this->io.sqrt.param.data[addr & 7] = val;
            start_sqrt(this);
            return;

        case R_IE+0:
            this->io.arm9.IE = (this->io.arm9.IE & 0xFFFFFF00) | (val & 0xFF);
            this->io.arm9.IE &= ~0x80; // bit 7 doesn't get set on ARM9
            NDS_eval_irqs_9(this);
            return;
        case R_IE+1:
            this->io.arm9.IE = (this->io.arm9.IE & 0xFFFF00FF) | (val << 8);
            NDS_eval_irqs_9(this);
            return;
        case R_IE+2:
            this->io.arm9.IE = (this->io.arm9.IE & 0xFF00FFFF) | (val << 16);
            NDS_eval_irqs_9(this);
            return;
        case R_IE+3:
            this->io.arm9.IE = (this->io.arm9.IE & 0x00FFFFFF) | (val << 24);
            NDS_eval_irqs_9(this);
            return;

        case R_DMA0SAD+0: this->dma9[0].io.src_addr = (this->dma9[0].io.src_addr & 0xFFFFFF00) | (val << 0); return;
        case R_DMA0SAD+1: this->dma9[0].io.src_addr = (this->dma9[0].io.src_addr & 0xFFFF00FF) | (val << 8); return;
        case R_DMA0SAD+2: this->dma9[0].io.src_addr = (this->dma9[0].io.src_addr & 0xFF00FFFF) | (val << 16); return;
        case R_DMA0SAD+3: this->dma9[0].io.src_addr = (this->dma9[0].io.src_addr & 0x00FFFFFF) | ((val & 0x0F) << 24); return;
        case R_DMA0DAD+0: this->dma9[0].io.dest_addr = (this->dma9[0].io.dest_addr & 0xFFFFFF00) | (val << 0); return;
        case R_DMA0DAD+1: this->dma9[0].io.dest_addr = (this->dma9[0].io.dest_addr & 0xFFFF00FF) | (val << 8); return;
        case R_DMA0DAD+2: this->dma9[0].io.dest_addr = (this->dma9[0].io.dest_addr & 0xFF00FFFF) | (val << 16); return;
        case R_DMA0DAD+3: this->dma9[0].io.dest_addr = (this->dma9[0].io.dest_addr & 0x00FFFFFF) | ((val & 0x0F) << 24); return;

        case R_DMA1SAD+0: this->dma9[1].io.src_addr = (this->dma9[1].io.src_addr & 0xFFFFFF00) | (val << 0); return;
        case R_DMA1SAD+1: this->dma9[1].io.src_addr = (this->dma9[1].io.src_addr & 0xFFFF00FF) | (val << 8); return;
        case R_DMA1SAD+2: this->dma9[1].io.src_addr = (this->dma9[1].io.src_addr & 0xFF00FFFF) | (val << 16); return;
        case R_DMA1SAD+3: this->dma9[1].io.src_addr = (this->dma9[1].io.src_addr & 0x00FFFFFF) | ((val & 0x0F) << 24); return;
        case R_DMA1DAD+0: this->dma9[1].io.dest_addr = (this->dma9[1].io.dest_addr & 0xFFFFFF00) | (val << 0); return;
        case R_DMA1DAD+1: this->dma9[1].io.dest_addr = (this->dma9[1].io.dest_addr & 0xFFFF00FF) | (val << 8); return;
        case R_DMA1DAD+2: this->dma9[1].io.dest_addr = (this->dma9[1].io.dest_addr & 0xFF00FFFF) | (val << 16); return;
        case R_DMA1DAD+3: this->dma9[1].io.dest_addr = (this->dma9[1].io.dest_addr & 0x00FFFFFF) | ((val & 0x0F) << 24); return;

        case R_DMA2SAD+0: this->dma9[2].io.src_addr = (this->dma9[2].io.src_addr & 0xFFFFFF00) | (val << 0); return;
        case R_DMA2SAD+1: this->dma9[2].io.src_addr = (this->dma9[2].io.src_addr & 0xFFFF00FF) | (val << 8); return;
        case R_DMA2SAD+2: this->dma9[2].io.src_addr = (this->dma9[2].io.src_addr & 0xFF00FFFF) | (val << 16); return;
        case R_DMA2SAD+3: this->dma9[2].io.src_addr = (this->dma9[2].io.src_addr & 0x00FFFFFF) | ((val & 0x0F) << 24); return;
        case R_DMA2DAD+0: this->dma9[2].io.dest_addr = (this->dma9[2].io.dest_addr & 0xFFFFFF00) | (val << 0); return;
        case R_DMA2DAD+1: this->dma9[2].io.dest_addr = (this->dma9[2].io.dest_addr & 0xFFFF00FF) | (val << 8); return;
        case R_DMA2DAD+2: this->dma9[2].io.dest_addr = (this->dma9[2].io.dest_addr & 0xFF00FFFF) | (val << 16); return;
        case R_DMA2DAD+3: this->dma9[2].io.dest_addr = (this->dma9[2].io.dest_addr & 0x00FFFFFF) | ((val & 0x0F) << 24); return;

        case R_DMA3SAD+0: this->dma9[3].io.src_addr = (this->dma9[3].io.src_addr & 0xFFFFFF00) | (val << 0); return;
        case R_DMA3SAD+1: this->dma9[3].io.src_addr = (this->dma9[3].io.src_addr & 0xFFFF00FF) | (val << 8); return;
        case R_DMA3SAD+2: this->dma9[3].io.src_addr = (this->dma9[3].io.src_addr & 0xFF00FFFF) | (val << 16); return;
        case R_DMA3SAD+3: this->dma9[3].io.src_addr = (this->dma9[3].io.src_addr & 0x00FFFFFF) | ((val & 0x0F) << 24); return;
        case R_DMA3DAD+0: this->dma9[3].io.dest_addr = (this->dma9[3].io.dest_addr & 0xFFFFFF00) | (val << 0); return;
        case R_DMA3DAD+1: this->dma9[3].io.dest_addr = (this->dma9[3].io.dest_addr & 0xFFFF00FF) | (val << 8); return;
        case R_DMA3DAD+2: this->dma9[3].io.dest_addr = (this->dma9[3].io.dest_addr & 0xFF00FFFF) | (val << 16); return;
        case R_DMA3DAD+3: this->dma9[3].io.dest_addr = (this->dma9[3].io.dest_addr & 0x00FFFFFF) | ((val & 0x0F) << 24); return;

        case R_DMA0CNT_L+0: this->dma9[0].io.word_count = (this->dma9[0].io.word_count & 0x1FFF00) | (val << 0); return;
        case R_DMA0CNT_L+1: this->dma9[0].io.word_count = (this->dma9[0].io.word_count & 0x1F00FF) | (val << 8); return;
        case R_DMA1CNT_L+0: this->dma9[1].io.word_count = (this->dma9[1].io.word_count & 0x1FFF00) | (val << 0); return;
        case R_DMA1CNT_L+1: this->dma9[1].io.word_count = (this->dma9[1].io.word_count & 0x1F00FF) | (val << 8); return;
        case R_DMA2CNT_L+0: this->dma9[2].io.word_count = (this->dma9[2].io.word_count & 0x1FFF00) | (val << 0); return;
        case R_DMA2CNT_L+1: this->dma9[2].io.word_count = (this->dma9[2].io.word_count & 0x1F00FF) | (val << 8); return;
        case R_DMA3CNT_L+0: this->dma9[3].io.word_count = (this->dma9[3].io.word_count & 0x1FFF00) | (val << 0); return;
        case R_DMA3CNT_L+1: this->dma9[3].io.word_count = (this->dma9[3].io.word_count & 0x1F00FF) | (val << 8); return;

        case R_DMA0CNT_H+0:
        case R_DMA1CNT_H+0:
        case R_DMA2CNT_H+0:
        case R_DMA3CNT_H+0: {
            struct NDS_DMA_ch *ch = &this->dma9[DMA_CH_NUM(addr)];
            ch->io.word_count = (ch->io.word_count & 0xFFFF) | ((val & 31) << 16);
            ch->io.dest_addr_ctrl = (val >> 5) & 3;
            ch->io.src_addr_ctrl = (ch->io.src_addr_ctrl & 2) | ((val >> 7) & 1);
            return;}

        case R_DMA0CNT_H+1:
        case R_DMA1CNT_H+1:
        case R_DMA2CNT_H+1:
        case R_DMA3CNT_H+1: {
            u32 chnum = DMA_CH_NUM(addr);
            struct NDS_DMA_ch *ch = &this->dma9[chnum];
            ch->io.src_addr_ctrl = (ch->io.src_addr_ctrl & 1) | ((val & 1) << 1);
            ch->io.repeat = (val >> 1) & 1;
            ch->io.transfer_size = (val >> 2) & 1;
            ch->io.start_timing = (val >> 3) & 7;
            if (ch->io.start_timing > 3) {
                printf("\nwarn DMA9 Start timing:%d", ch->io.start_timing);
            }
            ch->io.irq_on_end = (val >> 6) & 1;

            u32 old_enable = ch->io.enable;
            ch->io.enable = (val >> 7) & 1;
            if ((ch->io.enable == 1) && (old_enable == 0)) {
                ch->op.first_run = 1;
                if (ch->io.start_timing == 0) {
                    NDS_dma9_start(this, ch, chnum);
                }
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
            this->io.dma.filldata[addr & 15] = val;
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

            if ((bank_num < 2) || (bank_num >= 7)) this->mem.vram.io.bank[bank_num].mst = val & 3;
            else this->mem.vram.io.bank[bank_num].mst = val & 7;

            this->mem.vram.io.bank[bank_num].ofs = (val >> 3) & 3;
            this->mem.vram.io.bank[bank_num].enable = (val >> 7) & 1;

            //printf("\nWRITE VRAM val:%02x CNT:%d MST:%d OFS:%d enable:%d", val, bank_num, this->mem.vram.io.bank[bank_num].mst, this->mem.vram.io.bank[bank_num].ofs, this->mem.vram.io.bank[bank_num].enable);
            NDS_VRAM_resetup_banks(this);
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
            NDS_timer9_write_cnt(this, tn, val);
            return; }

        case R_TM0CNT_L+0: this->timer9[0].reload = (this->timer9[0].reload & 0xFF00) | val; return;
        case R_TM1CNT_L+0: this->timer9[1].reload = (this->timer9[1].reload & 0xFF00) | val; return;
        case R_TM2CNT_L+0: this->timer9[2].reload = (this->timer9[2].reload & 0xFF00) | val; return;
        case R_TM3CNT_L+0: this->timer9[3].reload = (this->timer9[3].reload & 0xFF00) | val; return;

        case R_TM0CNT_L+1: this->timer9[0].reload = (this->timer9[0].reload & 0xFF) | (val << 8); return;
        case R_TM1CNT_L+1: this->timer9[1].reload = (this->timer9[1].reload & 0xFF) | (val << 8); return;
        case R_TM2CNT_L+1: this->timer9[2].reload = (this->timer9[2].reload & 0xFF) | (val << 8); return;
        case R_TM3CNT_L+1: this->timer9[3].reload = (this->timer9[3].reload & 0xFF) | (val << 8); return;

        case R9_WRAMCNT: {
            this->mem.io.RAM9.val = val;
            switch (val & 3) {
                case 0: // 0 = 32k/0K, open-bus
                    this->mem.io.RAM9.base = 0;
                    this->mem.io.RAM9.mask = 0x7FFF;
                    this->mem.io.RAM9.disabled = 0;
                    this->mem.io.RAM7.base = 0;
                    this->mem.io.RAM7.mask = 0;
                    this->mem.io.RAM7.disabled = 1;
                    break;
                case 1: // 1 = hi 16K/ lo16K,
                    this->mem.io.RAM9.base = 0x4000;
                    this->mem.io.RAM9.mask = 0x3FFF;
                    this->mem.io.RAM9.disabled = 0;
                    this->mem.io.RAM7.base = 0;
                    this->mem.io.RAM7.mask = 0x3FFF;
                    this->mem.io.RAM7.disabled = 0;
                    break;
                case 2: // 2 = lo 16k/ hi16k,
                    this->mem.io.RAM9.base = 0;
                    this->mem.io.RAM9.mask = 0x3FFF;
                    this->mem.io.RAM9.disabled = 0;
                    this->mem.io.RAM7.base = 0x4000;
                    this->mem.io.RAM7.mask = 0x3FFF;
                    this->mem.io.RAM7.disabled = 0;
                    break;
                case 3: // 3 = 0k / 32k
                    this->mem.io.RAM9.base = 0;
                    this->mem.io.RAM9.mask = 0;
                    this->mem.io.RAM9.disabled = 1;
                    this->mem.io.RAM7.base = 0;
                    this->mem.io.RAM7.mask = 0x7FFF;
                    this->mem.io.RAM7.disabled = 0;
            }
            return; }

        case R9_EXMEMCNT+0:
            this->io.arm9.EXMEM = (this->io.arm9.EXMEM & 0xFF00) | val;
            this->io.rights.gba_slot = ((val >> 7) & 1) ^ 1;
            return;
        case R9_EXMEMCNT+1:
            this->io.arm9.EXMEM = (this->io.arm9.EXMEM & 0xFF) | (val << 8);
            this->io.rights.nds_slot = ((val >> 3) & 1) ^ 1;
            this->io.rights.main_memory = ((val >> 7) & 1) ^ 1;
            return;

    }
    printf("\nUnhandled BUSWR9IO8 addr:%08x val:%08x", addr, val);
}

// -----

static u32 busrd9_apu(struct NDS *this, u32 addr, u32 sz, u32 access, u32 has_effect){
    static int already_did = 0;
    if (!already_did) {
        already_did = 1;
        printf("\nWARN: APU READ9!");
    }
    return 0;
}

static u32 busrd9_io(struct NDS *this, u32 addr, u32 sz, u32 access, u32 has_effect)
{
    u32 v;
    if (((addr >= 0x04000000) && (addr < 0x04000070)) || ((addr >= 0x04001000) && (addr < 0x04001070))) {
        return NDS_PPU_read9_io(this, addr, sz, access, has_effect);
    }
    if ((addr >= 0x04000320) && (addr < 0x04000700)) {
        return NDS_GE_read(this, addr, sz);
    }
    if ((addr >= 0x04000400) && (addr < 0x04000520)) return busrd9_apu(this, addr, sz, access, has_effect);
    switch(addr) {
        case R_ROMCTRL:
            return NDS_cart_read_romctrl(this);

        case R_ROMDATA+0: // 4100010
        case R_ROMDATA+1: // 4100010
        case R_ROMDATA+2: // 4100010
        case R_ROMDATA+3: // 4100010
            assert(sz==4);
            return NDS_cart_read_rom(this, addr, sz);

        case R_POSTFLG:
            return this->io.arm9.POSTFLG;

        case R_IPCFIFORECV+0:
        case R_IPCFIFORECV+1:
        case R_IPCFIFORECV+2:
        case R_IPCFIFORECV+3:
            // arm9 reads from to_arm9
            if (this->io.ipc.arm9.fifo_enable) {
                if (NDS_IPC_fifo_is_empty(&this->io.ipc.to_arm9)) {
                    this->io.ipc.arm9.error |= 1;
                    v = NDS_IPC_fifo_peek_last(&this->io.ipc.to_arm9);
                }
                else {
                    u32 old_bits7 = NDS_IPC_fifo_is_empty(&this->io.ipc.to_arm9) & this->io.ipc.arm7.irq_on_send_fifo_empty;
                    v = NDS_IPC_fifo_pop(&this->io.ipc.to_arm9);
                    u32 new_bits7 = NDS_IPC_fifo_is_empty(&this->io.ipc.to_arm9) & this->io.ipc.arm7.irq_on_send_fifo_empty;
                    if (!old_bits7 && new_bits7) { // arm7 send is empty
                        NDS_update_IF7(this, NDS_IRQ_IPC_SEND_EMPTY);
                    }

                }
            }
            else {
                v = NDS_IPC_fifo_peek_last(&this->io.ipc.to_arm9);
            };
            return v & masksz[sz];
    }

    v = busrd9_io8(this, addr, 1, access, has_effect);
    if (sz >= 2) v |= busrd9_io8(this, addr+1, 1, access, has_effect) << 8;
    if (sz == 4) {
        v |= busrd9_io8(this, addr+2, 1, access, has_effect) << 16;
        v |= busrd9_io8(this, addr+3, 1, access, has_effect) << 24;
    }
    return v;
}

static u32 busrd7_apu(struct NDS *this, u32 addr, u32 sz, u32 access, u32 has_effect){
    static int already_did = 0;
    if (!already_did) {
        already_did = 1;
        printf("\nWARN: APU READ7!");
    }
    return 0;
}

static void buswr7_apu(struct NDS *this, u32 addr, u32 sz, u32 access, u32 val) {
    static int already_did = 0;
    if (!already_did) {
        already_did = 1;
        printf("\nWARN: APU WRITE7!");
    }
}


static void buswr9_apu(struct NDS *this, u32 addr, u32 sz, u32 access, u32 val) {
    static int already_did = 0;
    if (!already_did) {
        already_did = 1;
        printf("\nWARN: APU WRITE9!");
    }
}


static void buswr9_io(struct NDS *this, u32 addr, u32 sz, u32 access, u32 val)
{
    if (((addr >= 0x04000000) && (addr < 0x04000070)) || ((addr >= 0x04001000) && (addr < 0x04001070))) {
        NDS_PPU_write9_io(this, addr, sz, access, val);
        return;
    }
    if ((addr >= 0x04000320) && (addr < 0x04000700)) {
        NDS_GE_write(this, addr, sz, val);
        return;
    }

    if ((addr >= 0x04000400) && (addr < 0x04000520)) return buswr9_apu(this, addr, sz, access, val);
    switch(addr) {
        case R_AUXSPICNT: {
            NDS_cart_spi_write_spicnt(this, val & 0xFF, 0);
            if (sz >= 2)
                NDS_cart_spi_write_spicnt(this, (val >> 8) & 0xFF, 1);
            if (sz == 4) {
                buswr9_io(this, R_AUXSPIDATA, 2, access, val >> 16);
            }
            return; }
        case R_AUXSPICNT+1:
            NDS_cart_spi_write_spicnt(this, val & 0xFF, 1);
            return;

        case R_AUXSPIDATA:
            assert(sz!=1);
            NDS_cart_spi_transaction(this, val & 0xFFFF);
            if (sz == 4) {
                buswr9_io(this, R_ROMCTRL, 2, access, val >> 16);
            }
            return;
        case R_ROMCTRL:
            NDS_cart_write_romctrl(this, val);
            return;

        case R_IPCFIFOSEND+0:
        case R_IPCFIFOSEND+1:
        case R_IPCFIFOSEND+2:
        case R_IPCFIFOSEND+3:
            // All writes are only 32 bits here
            if (this->io.ipc.arm9.fifo_enable) {
                if (sz == 2) {
                    val &= 0xFFFF;
                    val = (val << 16) | val;
                }
                if (sz == 1) {
                    val &= 0xFF;
                    val = (val << 24) | (val << 16) | (val << 8) | val;
                }
                // ARM9 writes to_arm7
                u32 old_bits = NDS_IPC_fifo_is_not_empty(&this->io.ipc.to_arm7) & this->io.ipc.arm7.irq_on_recv_fifo_not_empty;
                if (this->io.ipc.arm9.fifo_enable) this->io.ipc.arm9.error |= NDS_IPC_fifo_push(&this->io.ipc.to_arm7, val);
                u32 new_bits = NDS_IPC_fifo_is_not_empty(&this->io.ipc.to_arm7) & this->io.ipc.arm7.irq_on_recv_fifo_not_empty;
                if (!old_bits && new_bits) {
                    // Trigger ARM7 recv not empty
                    NDS_update_IF7(this, NDS_IRQ_IPC_RECV_NOT_EMPTY);
                }
            }
            return;
    }
    buswr9_io8(this, addr, 1, access, val & 0xFF);
    if (sz >= 2) buswr9_io8(this, addr+1, 1, access, (val >> 8) & 0xFF);
    if (sz == 4) {
        buswr9_io8(this, addr+2, 1, access, (val >> 16) & 0xFF);
        buswr9_io8(this, addr+3, 1, access, (val >> 24) & 0xFF);
    }
}

static u32 busrd7_wifi(struct NDS *this, u32 addr, u32 sz, u32 access, u32 has_effect) {
    if (addr < 0x04808000) return cR[sz](this->mem.wifi, addr & 0x1FFF);

    return busrd7_invalid(this, addr, sz, access, has_effect);
}

static void buswr7_wifi(struct NDS *this, u32 addr, u32 sz, u32 access, u32 val)
{
    if (addr < 0x04808000) return cW[sz](this->mem.wifi, addr & 0x1FFF, val);

    switch(addr) {
        case 0x048080ae:
            printf("\nWarning ignore WIFI WRITE....");
            return;
    }
    buswr7_invalid(this, addr, sz, access, val);
}


static u32 busrd7_io(struct NDS *this, u32 addr, u32 sz, u32 access, u32 has_effect)
{
    if (((addr >= 0x04000000) && (addr < 0x04000070)) || ((addr >= 0x04001000) && (addr < 0x04001070))) {
        return NDS_PPU_read7_io(this, addr, sz, access, has_effect);
    }
    if ((addr >= 0x04000400) && (addr < 0x04000520)) return busrd7_apu(this, addr, sz, access, has_effect);
    if (addr >= 0x04800000) return busrd7_wifi(this, addr, sz, access, has_effect);
    u32 v;
    switch(addr) {
        case R_ROMCTRL:
            return NDS_cart_read_romctrl(this);

        case R_ROMDATA+0:
        case R_ROMDATA+1:
        case R_ROMDATA+2:
        case R_ROMDATA+3:
            assert(sz==4);
            return NDS_cart_read_rom(this, addr, sz);

        case R7_SPIDATA:
            return NDS_SPI_read(this, sz);

        case R_IPCFIFORECV+0:
        case R_IPCFIFORECV+1:
        case R_IPCFIFORECV+2:
        case R_IPCFIFORECV+3:
            // arm7 reads from to_arm7
            if (this->io.ipc.arm7.fifo_enable) {
                if (NDS_IPC_fifo_is_empty(&this->io.ipc.to_arm7)) {
                    this->io.ipc.arm7.error |= 1;
                    v = NDS_IPC_fifo_peek_last(&this->io.ipc.to_arm7);
                }
                else {
                    u32 old_bits = NDS_IPC_fifo_is_empty(&this->io.ipc.to_arm7) & this->io.ipc.arm9.irq_on_send_fifo_empty;
                    v = NDS_IPC_fifo_pop(&this->io.ipc.to_arm7);
                    u32 new_bits = NDS_IPC_fifo_is_empty(&this->io.ipc.to_arm7) & this->io.ipc.arm9.irq_on_send_fifo_empty;
                    if (!old_bits && new_bits) { // arm7 send is empty
                        NDS_update_IF9(this, NDS_IRQ_IPC_SEND_EMPTY);
                    }
                }
            }
            else {
                v = NDS_IPC_fifo_peek_last(&this->io.ipc.to_arm7);
            };
            return v & masksz[sz];
    }

    v = busrd7_io8(this, addr, 1, access, has_effect);
    if (sz >= 2) v |= busrd7_io8(this, addr+1, 1, access, has_effect) << 8;
    if (sz == 4) {
        v |= busrd7_io8(this, addr+2, 1, access, has_effect) << 16;
        v |= busrd7_io8(this, addr+3, 1, access, has_effect) << 24;
    }
    return v;
}

static void buswr7_io(struct NDS *this, u32 addr, u32 sz, u32 access, u32 val)
{
    if (((addr >= 0x04000000) && (addr < 0x04000070)) || ((addr >= 0x04001000) && (addr < 0x04001070))) {
        NDS_PPU_write7_io(this, addr, sz, access, val);
        return;
    }
    if ((addr >= 0x04000400) && (addr < 0x04000520)) return buswr7_apu(this, addr, sz, access, val);
    if (addr >= 0x04800000) return buswr7_wifi(this, addr, sz, access, val);

    switch(addr) {
        case R7_SPIDATA:
            NDS_SPI_write(this, sz, val);
            return;

        case R_AUXSPICNT: {
            NDS_cart_spi_write_spicnt(this, val & 0xFF, 0);
            if (sz >= 2) {
                NDS_cart_spi_write_spicnt(this, (val >> 8) & 0xFF, 1);
            }
            if (sz == 4) {
                buswr7_io(this, R_AUXSPIDATA, 2, access, val >> 16);
            }
            return; }
        case R_AUXSPICNT+1:
            NDS_cart_spi_write_spicnt(this, val & 0xFF, 1);
            return;

        case R_AUXSPIDATA:
            NDS_cart_spi_transaction(this, val & 0xFFFF);
            if (sz == 4) {
                buswr7_io(this, R_ROMCTRL, 2, access, val >> 16);
            }
            return;
        case R_ROMCTRL:
            NDS_cart_write_romctrl(this, val);
            return;

        case R_RTC:
            NDS_write_RTC(this, sz, val & 0xFFFF);
            return;
        case R_IPCFIFOSEND+0:
        case R_IPCFIFOSEND+1:
        case R_IPCFIFOSEND+2:
        case R_IPCFIFOSEND+3:
            // All writes are only 32 bits here
            if (this->io.ipc.arm7.fifo_enable) {
                if (sz == 2) {
                    val &= 0xFFFF;
                    val = (val << 16) | val;
                }
                if (sz == 1) {
                    val &= 0xFF;
                    val = (val << 24) | (val << 16) | (val << 8) | val;
                }
                // ARM7 writes to_arm9
                u32 old_bits = NDS_IPC_fifo_is_not_empty(&this->io.ipc.to_arm9) & this->io.ipc.arm9.irq_on_recv_fifo_not_empty;
                if (this->io.ipc.arm7.fifo_enable) this->io.ipc.arm7.error |= NDS_IPC_fifo_push(&this->io.ipc.to_arm9, val);
                u32 new_bits = NDS_IPC_fifo_is_not_empty(&this->io.ipc.to_arm9) & this->io.ipc.arm9.irq_on_recv_fifo_not_empty;
                if (!old_bits && new_bits) {
                    // Trigger ARM9 recv not empty
                    NDS_update_IF9(this, NDS_IRQ_IPC_RECV_NOT_EMPTY);
                }
            }
            return;
    }

    if (addr >= 0x04800000) return buswr7_wifi(this, addr, sz, access, val);
    buswr7_io8(this, addr, 1, access, val & 0xFF);
    if (sz >= 2) buswr7_io8(this, addr+1, 1, access, (val >> 8) & 0xFF);
    if (sz == 4) {
        buswr7_io8(this, addr+2, 1, access, (val >> 16) & 0xFF);
        buswr7_io8(this, addr+3, 1, access, (val >> 24) & 0xFF);
    }
}

void NDS_bus_reset(struct NDS *this) {
    NDS_RTC_reset(this);
    this->spi.irq_id = 0;

    for (u32 i = 0; i < 4; i++) {
        struct NDS_TIMER *t = &this->timer7[i];
        t->overflow_at = 0xFFFFFFFFFFFFFFFF;
        t->enable_at = 0xFFFFFFFFFFFFFFFF;
        t = &this->timer9[i];
        t->overflow_at = 0xFFFFFFFFFFFFFFFF;
        t->enable_at = 0xFFFFFFFFFFFFFFFF;
    }
}

void NDS_bus_init(struct NDS *this)
{
    for (u32 i = 0; i < 4; i++) {
        struct NDS_TIMER *t = &this->timer7[i];
        t->overflow_at = 0xFFFFFFFFFFFFFFFF;
        t->enable_at = 0xFFFFFFFFFFFFFFFF;
        t = &this->timer9[i];
        t->overflow_at = 0xFFFFFFFFFFFFFFFF;
        t->enable_at = 0xFFFFFFFFFFFFFFFF;
    }
    for (u32 i = 0; i < 16; i++) {
        this->mem.rw[0].read[i] = &busrd7_invalid;
        this->mem.rw[0].write[i] = &buswr7_invalid;
        this->mem.rw[1].read[i] = &busrd9_invalid;
        this->mem.rw[1].write[i] = &buswr9_invalid;
    }
    memset(this->dbg_info.mgba.str, 0, sizeof(this->dbg_info.mgba.str));
#define BND9(page, func) { this->mem.rw[1].read[page] = &busrd9_##func; this->mem.rw[1].write[page] = &buswr9_##func; }
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

#define BND7(page, func) { this->mem.rw[0].read[page] = &busrd7_##func; this->mem.rw[0].write[page] = &buswr7_##func; }
    BND7(0x0, bios7);
    BND7(0x2, main);
    BND7(0x3, shared);
    BND7(0x4, io);
    BND7(0x6, vram);
    BND7(0x8, gba_cart);
    BND7(0x9, gba_cart);
    BND7(0xA, gba_sram);
#undef BND7

    NDS_RTC_init(this);
}

/*static void trace_read(struct NDS *this, u32 addr, u32 sz, u32 val)
{
    struct trace_view *tv = this->cpu.dbg.tvptr;
    if (!tv) return;
    trace_view_startline(tv, 2);
    trace_view_printf(tv, 0, "BUSrd");
    trace_view_printf(tv, 1, "%lld", this->clock.master_cycle_count + this->waitstates.current_transaction);
    trace_view_printf(tv, 2, "%08x", addr);
    trace_view_printf(tv, 3, "%08x", val);
    trace_view_endline(tv);
}

static void trace_write(struct NDS *this, u32 addr, u32 sz, u32 val)
{
    struct trace_view *tv = this->cpu.dbg.tvptr;
    if (!tv) return;
    trace_view_startline(tv, 2);
    trace_view_printf(tv, 0, "BUSwr");
    trace_view_printf(tv, 1, "%lld", this->clock.master_cycle_count + this->waitstates.current_transaction);
    trace_view_printf(tv, 2, "%08x", addr);
    trace_view_printf(tv, 3, "%08x", val);
    trace_view_endline(tv);
}*/


u32 NDS_mainbus_read7(void *ptr, u32 addr, u32 sz, u32 access, u32 has_effect)
{
    struct NDS *this = (struct NDS *)ptr;
    this->waitstates.current_transaction++;
    u32 v;

    if (addr < 0x10000000) v = this->mem.rw[0].read[(addr >> 24) & 15](this, addr, sz, access, has_effect);
    else v = busrd7_invalid(this, addr, sz, access, has_effect);
    //if (dbg.trace_on) trace_read(this, addr, sz, v);
#ifdef TRACE
    printf("\n rd7:%08x sz:%d val:%08x", addr, sz, v);
#endif
    return v;
}

static u32 rd9_bios(struct NDS *this, u32 addr, u32 sz)
{
    return cR[sz](this->mem.bios9, addr & 0xFFF);
}

u32 NDS_mainbus_read9(void *ptr, u32 addr, u32 sz, u32 access, u32 has_effect)
{
    struct NDS *this = (struct NDS *)ptr;
    this->waitstates.current_transaction++;
    u32 v;

    if (addr < 0x10000000) v = this->mem.rw[1].read[(addr >> 24) & 15](this, addr, sz, access, has_effect);
    else if ((addr & 0xFFFF0000) == 0xFFFF0000) v = rd9_bios(this, addr, sz);
    else v = busrd9_invalid(this, addr, sz, access, has_effect);
#ifdef TRACE
    printf("\n rd9:%08x sz:%d val:%08x", addr, sz, v);
#endif
    //if (dbg.trace_on) trace_read(this, addr, sz, v);
    return v;
}

u32 NDS_mainbus_fetchins9(void *ptr, u32 addr, u32 sz, u32 access)
{
    struct NDS *this = (struct NDS*)ptr;
    u32 v = NDS_mainbus_read9(ptr, addr, sz, access, 1);
    switch(sz) {
        case 4:
            this->io.open_bus.arm9 = v;
            break;
        case 2:
            this->io.open_bus.arm9 = (v << 16) | v;
            break;
    }
    return v;
}


u32 NDS_mainbus_fetchins7(void *ptr, u32 addr, u32 sz, u32 access)
{
    struct NDS *this = (struct NDS*)ptr;
    u32 v = NDS_mainbus_read7(ptr, addr, sz, access, 1);
    switch(sz) {
        case 4:
            this->io.open_bus.arm7 = v;
            break;
        case 2:
            this->io.open_bus.arm7 = (v << 16) | v;
            break;
    }
    return v;
}

void NDS_mainbus_write7(void *ptr, u32 addr, u32 sz, u32 access, u32 val)
{
    struct NDS *this = (struct NDS *)ptr;
    this->waitstates.current_transaction++;
#ifdef TRACE
    printf("\n wr7:%08x sz:%d val:%08x", addr, sz, val);
#endif
    //if (dbg.trace_on) trace_write(this, addr, sz, val);
    if (addr < 0x10000000) {
        //printf("\nWRITE addr:%08x sz:%d val:%08x", addr, sz, val);
        return this->mem.rw[0].write[(addr >> 24) & 15](this, addr, sz, access, val);
    }

    buswr7_invalid(this, addr, sz, access, val);
}

void NDS_mainbus_write9(void *ptr, u32 addr, u32 sz, u32 access, u32 val)
{
    struct NDS *this = (struct NDS *)ptr;
    this->waitstates.current_transaction++;
    //if (dbg.trace_on) trace_write(this, addr, sz, val);
#ifdef TRACE
    printf("\n wr9:%08x sz:%d val:%08x", addr, sz, val);
#endif
    if (addr < 0x10000000) {
        return this->mem.rw[1].write[(addr >> 24) & 15](this, addr, sz, access, val);
    }

    buswr9_invalid(this, addr, sz, access, val);
}

u64 NDS_clock_current7(struct NDS *this)
{
    return this->clock.master_cycle_count7 + this->waitstates.current_transaction;
}

u64 NDS_clock_current9(struct NDS *this)
{
    return this->clock.master_cycle_count9 + this->waitstates.current_transaction;
}

//
// Created by . on 12/4/24.
//
#include <string.h>

#include "nds_regs.h"
#include "nds_bus.h"
#include "nds_vram.h"
#include "nds_dma.h"
#include "nds_timers.h"
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
    printf("\nWRITE9 UNKNOWN ADDR:%08x sz:%d DATA:%08x", addr, sz, val);
    this->waitstates.current_transaction++;
    dbg.var++;
    //if (dbg.var > 15) dbg_break("too many bad writes", this->clock.master_cycle_count);
}

static void buswr7_shared(struct NDS *this, u32 addr, u32 sz, u32 access, u32 val)
{
    if (addr >= 0x03800000) return cW[sz](this->mem.WRAM_arm7, addr & 0xFFFF, val);
    if (!this->mem.io.RAM7.disabled) cW[sz](this->mem.WRAM_share, (addr & this->mem.io.RAM7.mask) + this->mem.io.RAM7.base, val);
}

static u32 busrd7_shared(struct NDS *this, u32 addr, u32 sz, u32 access, u32 has_effect)
{
    if (addr >= 0x03800000) return cR[sz](this->mem.WRAM_arm7, addr & 0xFFFF);
    if (this->mem.io.RAM7.disabled) return 0; // undefined
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
    if (addr < 0x050000200) NDS_PPU_write_2d_bg_palette(this, 0, addr & 0x1FF, sz, val);
    if (addr < 0x050000400) NDS_PPU_write_2d_obj_palette(this, 0, addr & 0x1FF, sz, val);
    if (addr < 0x050000600) NDS_PPU_write_2d_bg_palette(this, 1, addr & 0x1FF, sz, val);
    if (addr < 0x050000800) NDS_PPU_write_2d_obj_palette(this, 1, addr & 0x1FF, sz, val);
    buswr9_invalid(this, addr, sz, access, val);
}

static u32 busrd9_obj_and_palette(struct NDS *this, u32 addr, u32 sz, u32 access, u32 has_effect)
{
    if (addr < 0x05000000) return busrd9_invalid(this, addr, sz, access, has_effect);
    if (addr < 0x050000200) return NDS_PPU_read_2d_bg_palette(this, 0, addr & 0x1FF, sz);
    if (addr < 0x050000400) return NDS_PPU_read_2d_obj_palette(this, 0, addr & 0x1FF, sz);
    if (addr < 0x050000600) return NDS_PPU_read_2d_bg_palette(this, 1, addr & 0x1FF, sz);
    if (addr < 0x050000800) return NDS_PPU_read_2d_obj_palette(this, 1, addr & 0x1FF, sz);
    return busrd9_invalid(this, addr, sz, access, has_effect);
}

static void buswr9_vram(struct NDS *this, u32 addr, u32 sz, u32 access, u32 val)
{
    if (sz == 1) return; // 8-bit writes ignored
    u8 *ptr = this->mem.vram.map.arm9[(addr >> 14) & 0x3FF];
    if (ptr) cW[sz](ptr, addr & 0x3FFF, val);

    printf("\nInvalid VRAM write unmapped addr:%08x sz:%d val:%08x", addr, sz, val);
}

static u32 busrd9_vram(struct NDS *this, u32 addr, u32 sz, u32 access, u32 has_effect)
{
    u8 *ptr = this->mem.vram.map.arm9[(addr >> 14) & 0x3FF];
    if (ptr) return cR[sz](ptr, addr & 0x3FFF);

    printf("\nInvalid VRAM read unmapped addr:%08x sz:%d", addr, sz);
    return 0;
}

static void buswr9_oam(struct NDS *this, u32 addr, u32 sz, u32 access, u32 val)
{
    if (addr < 0x07000000) return;
    if (addr < 0x07000400) return cW[sz](this->ppu.eng2d[0].mem.oam, addr, val);
    if (addr < 0x07000800) return cW[sz](this->ppu.eng2d[0].mem.oam, addr, val);
    buswr9_invalid(this, addr, sz, access, val);
}

static u32 busrd9_oam(struct NDS *this, u32 addr, u32 sz, u32 access, u32 has_effect)
{
    if (addr < 0x07000000) return busrd9_invalid(this, addr, sz, access, has_effect);
    if (addr < 0x07000400) return cR[sz](this->ppu.eng2d[0].mem.oam, addr);
    if (addr < 0x07000800) return cR[sz](this->ppu.eng2d[0].mem.oam, addr);
    return busrd9_invalid(this, addr, sz, access, has_effect);
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

static void buswr7_io8(struct NDS *this, u32 addr, u32 sz, u32 access, u32 val)
{
    switch(addr) {
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
            ch->io.irq_on_end = (val >> 6) & 1;
            u32 old_enable = ch->io.enable;
            ch->io.enable = (val >> 7) & 1;
            if ((ch->io.enable == 1) && (old_enable == 0)) {
                ch->op.first_run = 1;
                if (ch->io.start_timing == 0) {
                    NDS_dma7_start(ch, chnum);
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
            struct NDS_TIMER *t = &this->timer7[tn];
            u32 old_enable = NDS_timer7_enabled(this, tn);
            t->divider.io = val & 3;
            switch(val & 3) {
                case 0: t->shift = 0; break;
                case 1: t->shift = 6; break;
                case 2: t->shift = 8; break;
                case 3: t->shift = 10; break;
            }
            u32 new_enable = ((val >> 7) & 1);
            if (old_enable && !new_enable) { // turn off
                t->val_at_stop = NDS_read_timer7(this, tn);
                t->enable_at = 0xFFFFFFFFFFFFFFFF; // the infinite future!
                t->overflow_at = 0xFFFFFFFFFFFFFFFF;
            }
            u32 old_cascade = t->cascade;
            t->cascade = (val >> 2) & 1;
            if (old_cascade && !t->cascade && (old_enable == new_enable == 1)) { // update overflow time
                t->enable_at = NDS_clock_current7(this);
                t->overflow_at = t->enable_at + (timer_reload_ticks(t->val_at_stop) << t->shift);
            }
            if (!old_enable && new_enable) { // turn on
                t->enable_at = NDS_clock_current7(this) + 1;
                t->reload_ticks = timer_reload_ticks(t->reload) << t->shift;
                t->overflow_at = t->enable_at + t->reload_ticks;
                t->val_at_stop = t->reload;
            }
            t->irq_on_overflow = (val >> 6) & 1;
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
    u32 v;
    switch(addr) {
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

    }
    printf("\nUnhandled BUSRD9IO8 addr:%08x", addr);
    return 0;
}

static void buswr9_io8(struct NDS *this, u32 addr, u32 sz, u32 access, u32 val)
{
    switch(addr) {
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
            ch->io.irq_on_end = (val >> 6) & 1;

            u32 old_enable = ch->io.enable;
            ch->io.enable = (val >> 7) & 1;
            if ((ch->io.enable == 1) && (old_enable == 0)) {
                ch->op.first_run = 1;
                if (ch->io.start_timing == 0) {
                    NDS_dma9_start(ch, chnum);
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
            struct NDS_TIMER *t = &this->timer9[tn];
            u32 old_enable = NDS_timer9_enabled(this, tn);
            t->divider.io = val & 3;
            switch(val & 3) {
                case 0: t->shift = 0; break;
                case 1: t->shift = 6; break;
                case 2: t->shift = 8; break;
                case 3: t->shift = 10; break;
            }
            u32 new_enable = ((val >> 7) & 1);
            if (old_enable && !new_enable) { // turn off
                t->val_at_stop = NDS_read_timer9(this, tn);
                t->enable_at = 0xFFFFFFFFFFFFFFFF; // the infinite future!
                t->overflow_at = 0xFFFFFFFFFFFFFFFF;
            }
            u32 old_cascade = t->cascade;
            t->cascade = (val >> 2) & 1;
            if (old_cascade && !t->cascade && (old_enable == new_enable == 1)) { // update overflow time
                t->enable_at = NDS_clock_current9(this);
                t->overflow_at = t->enable_at + (timer_reload_ticks(t->val_at_stop) << t->shift);
            }
            if (!old_enable && new_enable) { // turn on
                t->enable_at = NDS_clock_current9(this) + 1;
                t->reload_ticks = timer_reload_ticks(t->reload) << t->shift;
                t->overflow_at = t->enable_at + t->reload_ticks;
                t->val_at_stop = t->reload;
            }
            t->irq_on_overflow = (val >> 6) & 1;
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
            switch (val & 3) {
                this->mem.io.RAM9.val = val;
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

static u32 busrd9_io(struct NDS *this, u32 addr, u32 sz, u32 access, u32 has_effect)
{
    u32 v = busrd9_io8(this, addr, 1, access, has_effect);
    if (sz >= 2) v |= busrd9_io8(this, addr+1, 1, access, has_effect) << 8;
    if (sz == 4) {
        v |= busrd9_io8(this, addr+2, 1, access, has_effect) << 16;
        v |= busrd9_io8(this, addr+3, 1, access, has_effect) << 24;
    }
    return v;
}

static void buswr9_io(struct NDS *this, u32 addr, u32 sz, u32 access, u32 val)
{
    buswr9_io8(this, addr, 1, access, val & 0xFF);
    if (sz >= 2) buswr9_io8(this, addr+1, 1, access, (val >> 8) & 0xFF);
    if (sz == 4) {
        buswr9_io8(this, addr+2, 1, access, (val >> 16) & 0xFF);
        buswr9_io8(this, addr+3, 1, access, (val >> 24) & 0xFF);
    }
}
// 0x2000 bytes
static u32 busrd7_wifi(struct NDS *this, u32 addr, u32 sz, u32 access, u32 has_effect) {
    if (addr < 0x04808000) return cR[sz](this->mem.wifi, addr & 0x1FFF);

    return busrd7_invalid(this, addr, sz, access, has_effect);
}

static void buswr7_wifi(struct NDS *this, u32 addr, u32 sz, u32 access, u32 val)
{
    if (addr < 0x04808000) return cW[sz](this->mem.wifi, addr & 0x1FFF, val);

    buswr7_invalid(this, addr, sz, access, val);
}


static u32 busrd7_io(struct NDS *this, u32 addr, u32 sz, u32 access, u32 has_effect)
{
    if (addr >= 0x04800000) return busrd7_wifi(this, addr, sz, access, has_effect);
    u32 v = busrd7_io8(this, addr, 1, access, has_effect);
    if (sz >= 2) v |= busrd7_io8(this, addr+1, 1, access, has_effect) << 8;
    if (sz == 4) {
        v |= busrd7_io8(this, addr+2, 1, access, has_effect) << 16;
        v |= busrd7_io8(this, addr+3, 1, access, has_effect) << 24;
    }
    return v;
}

static void buswr7_io(struct NDS *this, u32 addr, u32 sz, u32 access, u32 val)
{
    if (addr >= 0x04800000) return buswr7_wifi(this, addr, sz, access, val);
    buswr7_io8(this, addr, 1, access, val & 0xFF);
    if (sz >= 2) buswr7_io8(this, addr+1, 1, access, (val >> 8) & 0xFF);
    if (sz == 4) {
        buswr7_io8(this, addr+2, 1, access, (val >> 16) & 0xFF);
        buswr7_io8(this, addr+3, 1, access, (val >> 24) & 0xFF);
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
    u32 v;

    if (addr < 0x10000000) v = this->mem.rw[0].read[(addr >> 24) & 15](this, addr, sz, access, has_effect);
    else v = busrd7_invalid(this, addr, sz, access, has_effect);
    //if (dbg.trace_on) trace_read(this, addr, sz, v);
    return v;
}

static u32 rd9_bios(struct NDS *this, u32 addr, u32 sz)
{
    return cR[sz](this->mem.bios9, addr & 0xFFF);
}

u32 NDS_mainbus_read9(void *ptr, u32 addr, u32 sz, u32 access, u32 has_effect)
{
    struct NDS *this = (struct NDS *)ptr;
    u32 v;

    if (addr < 0x10000000) v = this->mem.rw[1].read[(addr >> 24) & 15](this, addr, sz, access, has_effect);
    else if ((addr & 0xFFFF0000) == 0xFFFF0000) v = rd9_bios(this, addr & 0xFFFF, sz);
    else v = busrd9_invalid(this, addr, sz, access, has_effect);
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
    //if (dbg.trace_on) trace_write(this, addr, sz, val);
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

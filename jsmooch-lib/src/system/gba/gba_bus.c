//
// Created by . on 12/4/24.
//

#include "gba_apu.h"
#include "gba_bus.h"
#include "helpers/multisize_memaccess.c"

static u32 busrd_invalid(struct GBA *this, u32 addr, u32 sz, u32 access, u32 has_effect) {
    printf("\nREAD UNKNOWN ADDR:%08x sz:%d", addr, sz);
    dbg.var++;
    if (dbg.var > 15) dbg_break("too many bad reads", this->clock.master_cycle_count);
    return 0;
}

static void buswr_invalid(struct GBA *this, u32 addr, u32 sz, u32 access, u32 val) {
    printf("\nWRITE UNKNOWN ADDR:%08x sz:%d DATA:%08x", addr, sz, val);
    dbg.var++;
    if (dbg.var > 15) dbg_break("too many bad writes", this->clock.master_cycle_count);
}

static u32 busrd_bios(struct GBA *this, u32 addr, u32 sz, u32 access, u32 has_effect) {
    if (addr < 0x40000) return cR[sz](this->BIOS.data, addr);
    return this->io.open_bus;
}

static void buswr_bios(struct GBA *this, u32 addr, u32 sz, u32 access, u32 val) {
    printf("\nWarning write to BIOS...");
}


static u32 busrd_WRAM_slow(struct GBA *this, u32 addr, u32 sz, u32 access, u32 has_effect) {
    return cR[sz](this->WRAM_slow, addr & 0x3FFFF);
}

static u32 busrd_WRAM_fast(struct GBA *this, u32 addr, u32 sz, u32 access, u32 has_effect) {
    return cR[sz](this->WRAM_fast, addr & 0x7FFF);
}

static void buswr_WRAM_slow(struct GBA *this, u32 addr, u32 sz, u32 access, u32 val) {
    cW[sz](this->WRAM_slow, addr & 0x3FFFF, val);
}

static void dma_start(struct GBA_DMA_ch *ch, u32 i)
{
    ch->op.started = 1;
    if (ch->op.first_run) {
        ch->op.dest_addr = ch->io.dest_addr & 0x0FFFFFFF;
        ch->op.src_addr = ch->io.src_addr & 0x0FFFFFFF;
    }
    else if (ch->io.dest_addr_ctrl == 3) {
        ch->op.dest_addr = ch->io.dest_addr & 0x0FFFFFFF;
    }
    ch->op.word_count = ch->io.word_count;
    ch->op.sz = ch->io.transfer_size ? 4 : 2;
    ch->op.word_mask = i == 3 ? 0xFFFF : 0x3FFF;
    //printf("\nDMA src:%08x dst:%08x sz:%d num:%d", ch->op.src_addr, ch->op.dest_addr, ch->op.sz, ch->op.word_count);
}


static void buswr_WRAM_fast(struct GBA *this, u32 addr, u32 sz, u32 access, u32 val) {
    cW[sz](this->WRAM_fast, addr & 0x7FFF, val);
}

static u32 busrd_IO(struct GBA *this, u32 addr, u32 sz, u32 access, u32 has_effect) {
    if (addr < 0x4000060) return GBA_PPU_mainbus_read_IO(this, addr, sz, access, has_effect);
    u32 r = 0;
    if (sz >= 2) {
        r |= busrd_IO(this, addr, 1, access, has_effect) << 0;
        r |= busrd_IO(this, addr+1, 1, access, has_effect) << 8;
    }
    if (sz == 4) {
        r |= busrd_IO(this, addr+2, 1, access, has_effect) << 16;
        r |= busrd_IO(this, addr+3, 1, access, has_effect) << 24;
    }
    if (sz != 1) return r;

    switch(addr) {
        case 0x04000130: // buttons!!!
            return GBA_get_controller_state(this->controller.pio) & 0xFF;
        case 0x04000131: // buttons!!!
            return GBA_get_controller_state(this->controller.pio) >> 8;
        case 0x04000200: return this->io.IE & 0xFF;
        case 0x04000201: return this->io.IE >> 8;
        case 0x04000202: return this->io.IF & 0xFF;
        case 0x04000203: return this->io.IF >> 8;
        case 0x04000208: return this->io.IME;

        case 0x040000B0: return (this->dma[0].io.src_addr >> 0) & 0xFF;
        case 0x040000B1: return (this->dma[0].io.src_addr >> 8) & 0xFF;
        case 0x040000B2: return (this->dma[0].io.src_addr >> 16) & 0xFF;
        case 0x040000B3: return (this->dma[0].io.src_addr >> 24) & 0xFF;
        case 0x040000B4: return (this->dma[0].io.dest_addr >> 0) & 0xFF;
        case 0x040000B5: return (this->dma[0].io.dest_addr >> 8) & 0xFF;
        case 0x040000B6: return (this->dma[0].io.dest_addr >> 16) & 0xFF;
        case 0x040000B7: return (this->dma[0].io.dest_addr >> 24) & 0xFF;
        case 0x040000B8: return (this->dma[0].io.word_count >> 0) & 0xFF;
        case 0x040000B9: return (this->dma[0].io.word_count >> 8) & 0xFF;

        case 0x040000BC: return (this->dma[1].io.src_addr >> 0) & 0xFF;
        case 0x040000BD: return (this->dma[1].io.src_addr >> 8) & 0xFF;
        case 0x040000BE: return (this->dma[1].io.src_addr >> 16) & 0xFF;
        case 0x040000BF: return (this->dma[1].io.src_addr >> 24) & 0xFF;
        case 0x040000C0: return (this->dma[1].io.dest_addr >> 0) & 0xFF;
        case 0x040000C1: return (this->dma[1].io.dest_addr >> 8) & 0xFF;
        case 0x040000C2: return (this->dma[1].io.dest_addr >> 16) & 0xFF;
        case 0x040000C3: return (this->dma[1].io.dest_addr >> 24) & 0xFF;
        case 0x040000C4: return (this->dma[1].io.word_count >> 0) & 0xFF;
        case 0x040000C5: return (this->dma[1].io.word_count >> 8) & 0xFF;

        case 0x040000C8: return (this->dma[2].io.src_addr >> 0) & 0xFF;
        case 0x040000C9: return (this->dma[2].io.src_addr >> 8) & 0xFF;
        case 0x040000CA: return (this->dma[2].io.src_addr >> 16) & 0xFF;
        case 0x040000CB: return (this->dma[2].io.src_addr >> 24) & 0xFF;
        case 0x040000CC: return (this->dma[2].io.dest_addr >> 0) & 0xFF;
        case 0x040000CD: return (this->dma[2].io.dest_addr >> 8) & 0xFF;
        case 0x040000CE: return (this->dma[2].io.dest_addr >> 16) & 0xFF;
        case 0x040000CF: return (this->dma[2].io.dest_addr >> 24) & 0xFF;
        case 0x040000D0: return (this->dma[2].io.word_count >> 0) & 0xFF;
        case 0x040000D1: return (this->dma[2].io.word_count >> 8) & 0xFF;

        case 0x040000D4: return (this->dma[3].io.src_addr >> 0) & 0xFF;
        case 0x040000D5: return (this->dma[3].io.src_addr >> 8) & 0xFF;
        case 0x040000D6: return (this->dma[3].io.src_addr >> 16) & 0xFF;
        case 0x040000D7: return (this->dma[3].io.src_addr >> 24) & 0xFF;
        case 0x040000D8: return (this->dma[3].io.dest_addr >> 0) & 0xFF;
        case 0x040000D9: return (this->dma[3].io.dest_addr >> 8) & 0xFF;
        case 0x040000DA: return (this->dma[3].io.dest_addr >> 16) & 0xFF;
        case 0x040000DB: return (this->dma[3].io.dest_addr >> 24) & 0xFF;
        case 0x040000DC: return (this->dma[3].io.word_count >> 0) & 0xFF;
        case 0x040000DD: return (this->dma[3].io.word_count >> 8) & 0xFF;

        case 0x04000100: return (this->timer[0].counter.val >> 0) & 0xFF;
        case 0x04000101: return (this->timer[0].counter.val >> 8) & 0xFF;
        case 0x04000104: return (this->timer[1].counter.val >> 0) & 0xFF;
        case 0x04000105: return (this->timer[1].counter.val >> 8) & 0xFF;
        case 0x04000108: return (this->timer[2].counter.val >> 0) & 0xFF;
        case 0x04000109: return (this->timer[2].counter.val >> 8) & 0xFF;
        case 0x0400010C: return (this->timer[3].counter.val >> 0) & 0xFF;
        case 0x0400010D: return (this->timer[3].counter.val >> 8) & 0xFF;

        case 0x04000103: // TIMERCNT upper, not used.
        case 0x04000107:
        case 0x0400010B:
        case 0x0400010F:
            return 0;

        case 0x04000102:
        case 0x04000106:
        case 0x0400010A:
        case 0x0400010E: {
            u32 tn = (addr >> 2) & 3;
            u32 v = this->timer[tn].divider.io;
            v |= this->timer[tn].cascade << 2;
            v |= this->timer[tn].irq_on_overflow << 6;
            v |= this->timer[tn].enable << 7;
            return v;
        }

    }
    return busrd_invalid(this, addr, sz, access, has_effect);
}



void GBA_eval_irqs(struct GBA *this)
{
    u32 old_line = this->cpu.regs.IRQ_line;
    this->cpu.regs.IRQ_line = (!!(this->io.IE & this->io.IF & 0x3FFF)) & this->io.IME;
    if (old_line != this->cpu.regs.IRQ_line) {
        printf("\nLINE SET TO %d cyc:%lld", this->cpu.regs.IRQ_line, this->clock.master_cycle_count);
    }
    /*

  Bit   Expl.
  0     LCD V-Blank                    (0=Disable)
  1     LCD H-Blank                    (etc.)
  2     LCD V-Counter Match            (etc.)
  3     Timer 0 Overflow               (etc.)
  4     Timer 1 Overflow               (etc.)
  5     Timer 2 Overflow               (etc.)
  6     Timer 3 Overflow               (etc.)
  7     Serial Communication           (etc.)
  8     DMA 0                          (etc.)
  9     DMA 1                          (etc.)
  10    DMA 2                          (etc.)
  11    DMA 3                          (etc.)
  12    Keypad                         (etc.)
  13    Game Pak (external IRQ source) (etc.)
  14-15 Not used */
}

static u32 DMA_CH_NUM(u32 addr)
{
    addr &= 0xFF;
    if (addr < 0xBC) return 0;
    if (addr < 0xC8) return 1;
    if (addr < 0xD4) return 2;
    return 3;
}

static void buswr_IO(struct GBA *this, u32 addr, u32 sz, u32 access, u32 val) {
    if (sz >= 2) {
        buswr_IO(this, addr, 1, access, (val >> 0) & 0xFF);
        buswr_IO(this, addr+1, 1, access, (val >> 8) & 0xFF);
    }
    if (sz == 4) {
        buswr_IO(this, addr+2, 1, access, (val >> 16) & 0xFF);
        buswr_IO(this, addr+3, 1, access, (val >> 24) & 0xFF);
    }
    if (sz != 1) return;
    val &= 0xFF;
    if (addr < 0x04000060) return GBA_PPU_mainbus_write_IO(this, addr, sz, access, val);
    if ((addr >= 0x04000060) && (addr < 0x040000B0)) return GBA_APU_write_IO(this, addr, sz, access, val);
    u32 mask = 0xFF;
    switch(addr) {
        case 0x04000200: // IE lo-byte
            this->io.IE = (this->io.IE & 0xFF00) | ((this->io.IE & ~mask) | (val & mask));
            GBA_eval_irqs(this);
            return;
        case 0x04000201: // IE hi-byte
            mask <<= 8;
            val <<= 8;
            this->io.IE = (this->io.IE & 0xFF) | ((this->io.IE & ~mask) | (val & mask));
            GBA_eval_irqs(this);
            return;
        case 0x04000202: // IF lo-byte
            this->io.IF &= ~val;
            GBA_eval_irqs(this);
            return;
        case 0x04000203: // IF hi-byte
            this->io.IF &= ~(val << 8);
            GBA_eval_irqs(this);
            return;
        case 0x04000204: // WAITcnt, ignore for now
            this->io.W8 = (this->io.W8 & 0xFF00) | val;
            return;
        case 0x04000205: // WAITcnt, ignore for now
            this->io.W8 = (this->io.W8 & 0xFF) | (val << 8);
            return;
        case 0x04000206: // empty, ignore
        case 0x04000207: // empty, ignore
            return;
        case 0x04000208: // IME lo
            this->io.IME = val & 1;
            GBA_eval_irqs(this);
            return;
        case 0x04000209: // IME hi
            return;

        case 0x0400020A:
        case 0x0400020B: // not used
            return;

        case 0x040000B0: this->dma[0].io.src_addr = (this->dma[0].io.src_addr & 0xFFFFFF00) | (val << 0); return; // DMA source address ch0
        case 0x040000B1: this->dma[0].io.src_addr = (this->dma[0].io.src_addr & 0xFFFF00FF) | (val << 8); return; // DMA source address ch0
        case 0x040000B2: this->dma[0].io.src_addr = (this->dma[0].io.src_addr & 0xFF00FFFF) | (val << 16); return; // DMA source address ch0
        case 0x040000B3: this->dma[0].io.src_addr = (this->dma[0].io.src_addr & 0x00FFFFFF) | ((val & 0x0F) << 24); return; // DMA source address ch0
        case 0x040000B4: this->dma[0].io.dest_addr = (this->dma[0].io.dest_addr & 0xFFFFFF00) | (val << 0); return; // DMA source address ch0
        case 0x040000B5: this->dma[0].io.dest_addr = (this->dma[0].io.dest_addr & 0xFFFF00FF) | (val << 8); return; // DMA source address ch0
        case 0x040000B6: this->dma[0].io.dest_addr = (this->dma[0].io.dest_addr & 0xFF00FFFF) | (val << 16); return; // DMA source address ch0
        case 0x040000B7: this->dma[0].io.dest_addr = (this->dma[0].io.dest_addr & 0x00FFFFFF) | ((val & 0x0F) << 24); return; // DMA source address ch0

        case 0x040000BC: this->dma[1].io.src_addr = (this->dma[1].io.src_addr & 0xFFFFFF00) | (val << 0); return; // DMA source address ch0
        case 0x040000BD: this->dma[1].io.src_addr = (this->dma[1].io.src_addr & 0xFFFF00FF) | (val << 8); return; // DMA source address ch0
        case 0x040000BE: this->dma[1].io.src_addr = (this->dma[1].io.src_addr & 0xFF00FFFF) | (val << 16); return; // DMA source address ch0
        case 0x040000BF: this->dma[1].io.src_addr = (this->dma[1].io.src_addr & 0x00FFFFFF) | ((val & 0x0F) << 24); return; // DMA source address ch0
        case 0x040000C0: this->dma[1].io.dest_addr = (this->dma[1].io.dest_addr & 0xFFFFFF00) | (val << 0); return; // DMA source address ch0
        case 0x040000C1: this->dma[1].io.dest_addr = (this->dma[1].io.dest_addr & 0xFFFF00FF) | (val << 8); return; // DMA source address ch0
        case 0x040000C2: this->dma[1].io.dest_addr = (this->dma[1].io.dest_addr & 0xFF00FFFF) | (val << 16); return; // DMA source address ch0
        case 0x040000C3: this->dma[1].io.dest_addr = (this->dma[1].io.dest_addr & 0x00FFFFFF) | ((val & 0x0F) << 24); return; // DMA source address ch0

        case 0x040000C8: this->dma[2].io.src_addr = (this->dma[2].io.src_addr & 0xFFFFFF00) | (val << 0); return; // DMA source address ch0
        case 0x040000C9: this->dma[2].io.src_addr = (this->dma[2].io.src_addr & 0xFFFF00FF) | (val << 8); return; // DMA source address ch0
        case 0x040000CA: this->dma[2].io.src_addr = (this->dma[2].io.src_addr & 0xFF00FFFF) | (val << 16); return; // DMA source address ch0
        case 0x040000CB: this->dma[2].io.src_addr = (this->dma[2].io.src_addr & 0x00FFFFFF) | ((val & 0x0F) << 24); return; // DMA source address ch0
        case 0x040000CC: this->dma[2].io.dest_addr = (this->dma[2].io.dest_addr & 0xFFFFFF00) | (val << 0); return; // DMA source address ch0
        case 0x040000CD: this->dma[2].io.dest_addr = (this->dma[2].io.dest_addr & 0xFFFF00FF) | (val << 8); return; // DMA source address ch0
        case 0x040000CE: this->dma[2].io.dest_addr = (this->dma[2].io.dest_addr & 0xFF00FFFF) | (val << 16); return; // DMA source address ch0
        case 0x040000CF: this->dma[2].io.dest_addr = (this->dma[2].io.dest_addr & 0x00FFFFFF) | ((val & 0x0F) << 24); return; // DMA source address ch0

        case 0x040000D4: this->dma[3].io.src_addr = (this->dma[3].io.src_addr & 0xFFFFFF00) | (val << 0); return; // DMA source address ch0
        case 0x040000D5: this->dma[3].io.src_addr = (this->dma[3].io.src_addr & 0xFFFF00FF) | (val << 8); return; // DMA source address ch0
        case 0x040000D6: this->dma[3].io.src_addr = (this->dma[3].io.src_addr & 0xFF00FFFF) | (val << 16); return; // DMA source address ch0
        case 0x040000D7: this->dma[3].io.src_addr = (this->dma[3].io.src_addr & 0x00FFFFFF) | ((val & 0x0F) << 24); return; // DMA source address ch0
        case 0x040000D8: this->dma[3].io.dest_addr = (this->dma[3].io.dest_addr & 0xFFFFFF00) | (val << 0); return; // DMA source address ch0
        case 0x040000D9: this->dma[3].io.dest_addr = (this->dma[3].io.dest_addr & 0xFFFF00FF) | (val << 8); return; // DMA source address ch0
        case 0x040000DA: this->dma[3].io.dest_addr = (this->dma[3].io.dest_addr & 0xFF00FFFF) | (val << 16); return; // DMA source address ch0
        case 0x040000DB: this->dma[3].io.dest_addr = (this->dma[3].io.dest_addr & 0x00FFFFFF) | ((val & 0x0F) << 24); return; // DMA source address ch0

        case 0x040000B8: this->dma[0].io.word_count = (this->dma[0].io.word_count & 0x3F00) | (val << 0); return;
        case 0x040000B9: this->dma[0].io.word_count = (this->dma[0].io.word_count & 0xFF) | ((val & 0x3F) << 8); return;
        case 0x040000C4: this->dma[1].io.word_count = (this->dma[1].io.word_count & 0x3F00) | (val << 0); return;
        case 0x040000C5: this->dma[1].io.word_count = (this->dma[1].io.word_count & 0xFF) | ((val & 0x3F) << 8); return;
        case 0x040000D0: this->dma[2].io.word_count = (this->dma[2].io.word_count & 0x3F00) | (val << 0); return;
        case 0x040000D1: this->dma[2].io.word_count = (this->dma[2].io.word_count & 0xFF) | ((val & 0x3F) << 8); return;
        case 0x040000DC: this->dma[3].io.word_count = (this->dma[3].io.word_count & 0xFF00) | (val << 0); return;
        case 0x040000DD: this->dma[3].io.word_count = (this->dma[3].io.word_count & 0xFF) | ((val & 0xFF) << 8); return;

        case 0x04000103: // TMRxCNT upper half unused
        case 0x04000107:
        case 0x0400010B:
        case 0x0400010F:
            return;

        case 0x04000102:
        case 0x04000106:
        case 0x0400010A:
        case 0x0400010E: {
            u32 tn = (addr >> 2) & 3;
            this->timer[tn].divider.io = val & 3;
            switch(val & 3) {
                case 0: this->timer[tn].divider.mask = 0; break;
                case 1: this->timer[tn].divider.mask = 63; break;
                case 2: this->timer[tn].divider.mask = 255; break;
                case 3: this->timer[tn].divider.mask = 1023; break;
            }
            this->timer[tn].cascade = (val >> 2) & 1;
            this->timer[tn].irq_on_overflow = (val >> 6) & 1;
            u32 old_enable = this->timer[tn].enable;
            this->timer[tn].enable = (val >> 7) & 1;
            if ((!old_enable) && (this->timer[tn].enable = 1)) {
                this->timer[tn].divider.counter = 0;
                this->timer[tn].counter.val = this->timer[tn].counter.reload;
            }
            printf("\nTIMER:%d ENABLE:%d CASCADE:%d IRQ:%d DMASK:%d", tn, this->timer[tn].enable, this->timer[tn].cascade, this->timer[tn].irq_on_overflow, this->timer[tn].divider.mask);
            return; }

        case 0x04000100: this->timer[0].counter.reload = (this->timer[0].counter.reload & 0xFF00) | val; return;
        case 0x04000104: this->timer[1].counter.reload = (this->timer[1].counter.reload & 0xFF00) | val; return;
        case 0x04000108: this->timer[2].counter.reload = (this->timer[2].counter.reload & 0xFF00) | val; return;
        case 0x0400010C: this->timer[3].counter.reload = (this->timer[3].counter.reload & 0xFF00) | val; return;

        case 0x04000101: this->timer[0].counter.reload = (this->timer[0].counter.reload & 0xFF) | (val << 8); return;
        case 0x04000105: this->timer[1].counter.reload = (this->timer[1].counter.reload & 0xFF) | (val << 8); return;
        case 0x04000109: this->timer[2].counter.reload = (this->timer[2].counter.reload & 0xFF) | (val << 8); return;
        case 0x0400010D: this->timer[3].counter.reload = (this->timer[3].counter.reload & 0xFF) | (val << 8); return;


        case 0x04000301: { printf("\nHALT! %d", val); this->io.halted = 1; break; }

        // DMA enable 0->1 while start timing = 0 will start it
        case 0x040000BA:
        case 0x040000C6:
        case 0x040000D2:
        case 0x040000DE: {
            struct GBA_DMA_ch *ch = &this->dma[DMA_CH_NUM(addr)];
            ch->io.dest_addr_ctrl = (val >> 5) & 3;
            ch->io.src_addr_ctrl = (ch->io.src_addr_ctrl & 2) | ((val >> 7) & 1);
            return;}
        case 0x040000BB:
        case 0x040000C7:
        case 0x040000D3:
        case 0x040000DF: {
            u32 chnum = DMA_CH_NUM(addr);
            struct GBA_DMA_ch *ch = &this->dma[chnum];
            ch->io.src_addr_ctrl = (ch->io.src_addr_ctrl & 1) | ((val & 1) << 1);
            ch->io.repeat = (val >> 1) & 1;
            ch->io.transfer_size = (val >> 2) & 1;
            if (chnum == 3) ch->io.game_pak_drq = (val >> 3) & 1;
            ch->io.start_timing = (val >> 4) & 3;
            ch->io.irq_on_end = (val >> 6) & 1;
            u32 old_enable = ch->io.enable;
            ch->io.enable = (val >> 7) & 1;
            if ((ch->io.enable == 1) && (old_enable == 0)) {
                ch->op.first_run = 1;
                if (ch->io.start_timing == 0) {
                    //printf("\nDMA START VIA ENABLE %d", chnum);
                    dma_start(ch, chnum);
                }
            }
            return;}

            case 0x04000120:
        case 0x04000121:
        case 0x04000122:
        case 0x04000123:
        case 0x04000124:
        case 0x04000125:
        case 0x04000126:
        case 0x04000127:
        case 0x04000128:
        case 0x04000129:
        case 0x0400012A:
        case 0x0400012B:
            // TODO: support all this serial jazz!?
            return;

        case 0x04000130:
        case 0x04000131:
            // write to keypad? why?
            return;

        case 0x04000132:
            this->io.button_irq.a = val & 1;
            this->io.button_irq.b = (val >> 1) & 1;
            this->io.button_irq.select = (val >> 2) & 1;
            this->io.button_irq.start = (val >> 3) & 1;
            this->io.button_irq.right = (val >> 4) & 1;
            this->io.button_irq.left = (val >> 5) & 1;
            this->io.button_irq.up = (val >> 6) & 1;
            this->io.button_irq.down = (val >> 7) & 1;
            return;
        case 0x04000133:
            this->io.button_irq.r = val & 1;
            this->io.button_irq.l = (val >> 1) & 1;
            u32 old_enable = this->io.button_irq.enable;
            this->io.button_irq.enable = (val >> 6) & 1;
            if ((old_enable == 0) && this->io.button_irq.enable) {
                printf("\nWARNING BUTTON IRQ ENABLED...");
            }
            this->io.button_irq.condition = (val >> 7) & 1;
            return;
    }
    buswr_invalid(this, addr, sz, access, val);
}

void GBA_bus_init(struct GBA *this)
{
    for (u32 i = 0; i < 16; i++) {
        this->mem.read[i] = &busrd_invalid;
        this->mem.write[i] = &buswr_invalid;
    }
    this->mem.read[0x0] = &busrd_bios;
    this->mem.read[0x2] = &busrd_WRAM_slow;
    this->mem.read[0x3] = &busrd_WRAM_fast;
    this->mem.read[0x4] = &busrd_IO;
    this->mem.read[0x5] = &GBA_PPU_mainbus_read_palette;
    this->mem.read[0x6] = &GBA_PPU_mainbus_read_VRAM;
    this->mem.read[0x7] = &GBA_PPU_mainbus_read_OAM;
    this->mem.read[0x8] = &GBA_cart_read_wait0;
    this->mem.read[0x9] = &GBA_cart_read_wait0;
    this->mem.read[0xA] = &GBA_cart_read_wait1;
    this->mem.read[0xB] = &GBA_cart_read_wait1;
    this->mem.read[0xC] = &GBA_cart_read_wait2;
    this->mem.read[0xD] = &GBA_cart_read_wait2;
    this->mem.read[0xE] = &GBA_cart_read_sram;

    this->mem.write[0x0] = &buswr_bios;
    this->mem.write[0x2] = &buswr_WRAM_slow;
    this->mem.write[0x3] = &buswr_WRAM_fast;
    this->mem.write[0x4] = &buswr_IO;
    this->mem.write[0x5] = &GBA_PPU_mainbus_write_palette;
    this->mem.write[0x6] = &GBA_PPU_mainbus_write_VRAM;
    this->mem.write[0x7] = &GBA_PPU_mainbus_write_OAM;
    this->mem.write[0xE] = &GBA_cart_write_sram;
}

static u32 GBA_mainbus_IO_read(struct GBA *this, u32 addr, u32 sz, u32 has_effect)
{
    printf("\nUnknown IO read addr:%08x sz:%d", addr, sz);
    return 0;
}

static void GBA_mainbus_IO_write(struct GBA *this, u32 addr, u32 sz, u32 val)
{
    printf("\nUnknown IO write addr:%08x sz:%d val:%08x", addr, sz, val);
}

static void trace_read(struct GBA *this, u32 addr, u32 sz, u32 val)
{
    struct trace_view *tv = this->cpu.dbg.tvptr;
    if (!tv) return;
    trace_view_startline(tv, 2);
    trace_view_printf(tv, 0, "BUSrd");
    trace_view_printf(tv, 1, "%lld", this->clock.master_cycle_count);
    trace_view_printf(tv, 2, "%08x", addr);
    trace_view_printf(tv, 3, "%08x", val);
    trace_view_endline(tv);
}

u32 GBA_mainbus_read(void *ptr, u32 addr, u32 sz, u32 access, u32 has_effect)
{
    if (sz == 4) addr &= ~3;
    if (sz == 2) addr &= ~1;
    struct GBA *this = (struct GBA *)ptr;

    u32 v;

    if (addr < 0x10000000) v = this->mem.read[(addr >> 24) & 15](this, addr, sz, access, has_effect);

    else v = busrd_invalid(this, addr, sz, access, has_effect);
    this->io.open_bus = v;
    if (dbg.trace_on && dbg.traces.ram) trace_read(this, addr, sz, v);
    return v;
}

u32 GBA_mainbus_fetchins(void *ptr, u32 addr, u32 sz, u32 access)
{
    struct GBA *this = (struct GBA*)ptr;
    return GBA_mainbus_read(ptr, addr, sz, access, 1);
}

void GBA_mainbus_write(void *ptr, u32 addr, u32 sz, u32 access, u32 val)
{
    if (sz == 4) addr &= ~3;
    if (sz == 2) addr &= ~1;
    struct GBA *this = (struct GBA *)ptr;

    if (addr < 0x10000000) {
        //printf("\nWRITE addr:%08x sz:%d val:%08x", addr, sz, val);
        return this->mem.write[(addr >> 24) & 15](this, addr, sz, access, val);
    }

    buswr_invalid(this, addr, sz, access, val);
}

void GBA_check_dma_at_hblank(struct GBA *this)
{
    // Check if any DMA channels are at enabled=1, started=0, time=hblank
    for (u32 i = 0; i < 4; i++) {
        struct GBA_DMA_ch *ch = &this->dma[i];
        if ((ch->io.enable) && (!ch->op.started) && ((ch->io.start_timing == 2) || ((i == 3) && (ch->io.start_timing == 3)))) {
            if (ch->io.start_timing == 3) { // SPECIAL VIDEO MODE CH3 only goes 2-162 weirdly...
                if ((this->clock.ppu.y < 2) || (this->clock.ppu.y > 162)) continue;
            }
            else { // no hblank IRQs in vblank
                if (this->clock.ppu.y >= 160) continue;
            }
            //printf("\nDMA HBLANK START %d", i);
            dma_start(ch, i);
        }
    }
    // And if it's channel 3 and "special", if we're in the correct lines.
}

void GBA_check_dma_at_vblank(struct GBA *this)
{
    // Check if any DMA channels are at enabled=1, started=0, time=hblank
    for (u32 i = 0; i < 4; i++) {
        struct GBA_DMA_ch *ch = &this->dma[i];
        if ((ch->io.enable) && (!ch->op.started) && (ch->io.start_timing == 1)) {
            //printf("\nDMA VBLANK START %d", i);
            dma_start(ch, i);
        }
    }
}

//
// Created by . on 12/4/24.
//
#include <string.h>

#include "gba_apu.h"
#include "gba_bus.h"
#include "gba_timers.h"
#include "helpers/multisize_memaccess.c"

static u32 busrd_invalid(struct GBA *this, u32 addr, u32 sz, u32 access, u32 has_effect) {
    printf("\nREAD UNKNOWN ADDR:%08x sz:%d", addr, sz);
    this->waitstates.current_transaction++;
    //dbg.var++;
    //if (dbg.var > 15) dbg_break("too many bad reads", this->clock.master_cycle_count);
    u32 v = GBA_open_bus_byte(this, addr);
    if (sz >= 2) {
        v |= GBA_open_bus_byte(this, addr+1) << 8;
    }
    if (sz == 4) {
        v |= GBA_open_bus_byte(this, addr+2) << 16;
        v |= GBA_open_bus_byte(this, addr+3) << 24;
    }
    return v;
}

static void buswr_invalid(struct GBA *this, u32 addr, u32 sz, u32 access, u32 val) {
    printf("\nWRITE UNKNOWN ADDR:%08x sz:%d DATA:%08x", addr, sz, val);
    this->waitstates.current_transaction++;
    dbg.var++;
    //if (dbg.var > 15) dbg_break("too many bad writes", this->clock.master_cycle_count);
}

#define WAITCNT 0x04000204

static u32 busrd_bios(struct GBA *this, u32 addr, u32 sz, u32 access, u32 has_effect) {
    this->waitstates.current_transaction++;
    if (addr < 0x40000) {
        if (this->cpu.regs.R[15] < 0x40000) {
            u32 v = cR[sz](this->BIOS.data, addr);
            this->io.bios_open_bus = v;
            return v;
        }
        else {
            return this->io.bios_open_bus;
        }
    }

    return GBA_open_bus(this, addr, sz);
}

static void buswr_bios(struct GBA *this, u32 addr, u32 sz, u32 access, u32 val) {
    this->waitstates.current_transaction++;
    //printf("\nWarning write to BIOS...");
}


static u32 busrd_WRAM_slow(struct GBA *this, u32 addr, u32 sz, u32 access, u32 has_effect) {
    this->waitstates.current_transaction += 3;
    if (sz == 4) {
        addr &= ~3;
        this->waitstates.current_transaction += 3;
    }
    if (sz == 2) addr &= ~1;
    return cR[sz](this->WRAM_slow, addr & 0x3FFFF);
}

static u32 busrd_WRAM_fast(struct GBA *this, u32 addr, u32 sz, u32 access, u32 has_effect) {
    this->waitstates.current_transaction++;
    if (sz == 4) addr &= ~3;
    if (sz == 2) addr &= ~1;
    return cR[sz](this->WRAM_fast, addr & 0x7FFF);
}

static void buswr_WRAM_slow(struct GBA *this, u32 addr, u32 sz, u32 access, u32 val) {
    this->waitstates.current_transaction += 3;
    if (sz == 4) {
        addr &= ~3;
        this->waitstates.current_transaction += 3;
    }
    if (sz == 4) addr &= ~3;
    if (sz == 2) addr &= ~1;
    cW[sz](this->WRAM_slow, addr & 0x3FFFF, val);
}

void GBA_dma_start(struct GBA_DMA_ch *ch, u32 i, u32 is_sound)
{
    ch->op.started = 1;
    u32 mask = ch->io.transfer_size ? ~3 : ~1;
    mask &= 0x0FFFFFFF;
    //u32 mask = 0x0FFFFFFF;
    if (ch->op.first_run) {
        ch->op.dest_addr = ch->io.dest_addr & mask;
        ch->op.src_addr = ch->io.src_addr & mask;
    }
    else if (ch->io.dest_addr_ctrl == 3) {
        ch->op.dest_addr = ch->io.dest_addr & mask;
    }
    ch->op.word_count = ch->io.word_count;
    ch->op.sz = ch->io.transfer_size ? 4 : 2;
    ch->op.word_mask = i == 3 ? 0xFFFF : 0x3FFF;
    ch->op.dest_access = ARM7P_nonsequential | ARM7P_dma;
    ch->op.src_access = ARM7P_nonsequential | ARM7P_dma;
    ch->op.is_sound = is_sound;
    if (is_sound) {
        ch->op.sz = 4;
        ch->io.dest_addr_ctrl = 2;
        ch->op.word_count = 4;
    }
}

static void set_waitstates(struct GBA *this) {

// 8 and 8+1  are set to...based on the thing....
#define DOV4(n, a, b, c, d) switch(this->waitstates.io. n) { case 0: this->waitstates. n = a; break; case 1: this->waitstates. n = b; break; case 2: this->waitstates. n = c; break; case 3: this->waitstates. n = d; break; }
#define DOV2(n, a, b) switch(this->waitstates.io. n) { case 0: this->waitstates. n = a; break; case 1: this->waitstates. n = b; break; }
    DOV4(sram, 5, 4, 3, 9);
    
    // OK we need to fill up 3 sections based on Nonsequential and Sequential
    // Nonsequentiual gets 4 possible values, sequential gets different values
    static const u32 nonseq[4] = {5, 4, 3, 9};
    static const u32 seq0[2] = {3, 2};
    static const u32 seq1[2] = {5, 2};
    static const u32 seq2[2] = {9, 2};
    
#define t16 this->waitstates.timing16
#define t32 this->waitstates.timing32
#define WS0 (0x8+i)
#define WS1 (0xA+i)
#define WS2 (0xC+i)
    for (u32 i = 0; i < 1; i++) {
        t16[0][WS0] = nonseq[this->waitstates.io.ws0_n];
        t16[0][WS1] = nonseq[this->waitstates.io.ws1_n];
        t16[0][WS2] = nonseq[this->waitstates.io.ws2_n];
        t16[1][WS0] = seq0[this->waitstates.io.ws0_s];
        t16[1][WS1] = seq1[this->waitstates.io.ws1_s];
        t16[1][WS2] = seq2[this->waitstates.io.ws2_s];
        
        t32[0][WS0] = t16[0][WS0] + t16[1][WS0];
        t32[0][WS1] = t16[0][WS1] + t16[1][WS1];
        t32[0][WS2] = t16[0][WS2] + t16[1][WS2];

        t32[1][WS0] = t16[1][WS0] << 1;
        t32[1][WS1] = t16[1][WS1] << 1;
        t32[1][WS2] = t16[1][WS2] << 1;
    }
#undef t16
#undef t32
#undef WS0
#undef WS1
#undef WS2
    /*
    DOV4(ws0_n, 8, 5, 4, 3, 9)
    DOV4(ws0_n, 5, 4, 3, 9);
    DOV2(ws0_s, 3, 2);
    DOV4(ws1_n, 5, 4, 3, 9);
    DOV2(ws1_s, 5, 2);
    DOV4(ws2_n, 5, 4, 3, 9);
    DOV2(ws2_s, 9, 2);*/
#undef DOV4
#undef DOV2
    //printf("\nWaitstates!");
#define d(x) this->waitstates. x
    //printf("\n0n:%d 0s:%d 1n:%d 1s:%d 2n:%d 2s:%d", d(ws0_n), d(ws0_s), d(ws1_n), d(ws1_s), d(ws2_n), d(ws2_s));
#undef d
}


static void buswr_WRAM_fast(struct GBA *this, u32 addr, u32 sz, u32 access, u32 val) {
    this->waitstates.current_transaction++;
    if (sz == 4) addr &= ~3;
    if (sz == 2) addr &= ~1;
    cW[sz](this->WRAM_fast, addr & 0x7FFF, val);
}

static u32 DMA_CH_NUM(u32 addr)
{
    addr &= 0xFF;
    if (addr < 0xBC) return 0;
    if (addr < 0xC8) return 1;
    if (addr < 0xD4) return 2;
    return 3;
}

static u32 busrd_IO8(struct GBA *this, u32 addr, u32 sz, u32 access, u32 has_effect) {
    if (addr < 0x4000060) return GBA_PPU_mainbus_read_IO(this, addr, sz, access, has_effect);
    if ((addr >= 0x04000060) && (addr < 0x040000B0)) return GBA_APU_read_IO(this, addr, sz, access, has_effect);

    switch(addr) {
        case 0x04000130: // buttons!!!
            return GBA_get_controller_state(this->controller.pio) & 0xFF;
        case 0x04000131: // buttons!!!
            return GBA_get_controller_state(this->controller.pio) >> 8;

        // 40000BA DMA0cnt
        // 40000C6 DMA1cnt
        // 40000D2 DMA2cnt
        // 40000DE DMA3cnt
        case 0x040000BA:
        case 0x040000C6:
        case 0x040000D2:
        case 0x040000DE: {
            struct GBA_DMA_ch *ch = &this->dma[DMA_CH_NUM(addr)];
            u32 v = ch->io.dest_addr_ctrl << 5;
            v |= ch->io.src_addr_ctrl << 7;
            return v;}

        case 0x040000B8:
        case 0x040000B9:
        case 0x040000C4:
        case 0x040000C5:
        case 0x040000D0:
        case 0x040000D1:
        case 0x040000DC:
        case 0x040000DD: return 0;

        case 0x040000BB:
        case 0x040000C7:
        case 0x040000D3:
        case 0x040000DF: {
            u32 chnum = DMA_CH_NUM(addr);
            struct GBA_DMA_ch *ch = &this->dma[chnum];
            u32 v = (ch->io.src_addr_ctrl >> 1) & 1;
            v |= ch->io.repeat << 1;
            v |= ch->io.transfer_size << 2;
            if (chnum == 3) v |= ch->io.game_pak_drq << 3;
            v |= ch->io.start_timing << 4;
            v |= ch->io.irq_on_end << 6;
            v |= ch->io.enable << 7;
            return v;}

        case 0x04000100: return (GBA_read_timer(this, 0) >> 0) & 0xFF;
        case 0x04000101: return (GBA_read_timer(this, 0) >> 8) & 0xFF;
        case 0x04000104: return (GBA_read_timer(this, 1) >> 0) & 0xFF;
        case 0x04000105: return (GBA_read_timer(this, 1) >> 8) & 0xFF;
        case 0x04000108: return (GBA_read_timer(this, 2) >> 0) & 0xFF;
        case 0x04000109: return (GBA_read_timer(this, 2) >> 8) & 0xFF;
        case 0x0400010C: return (GBA_read_timer(this, 3) >> 0) & 0xFF;
        case 0x0400010D: return (GBA_read_timer(this, 3) >> 8) & 0xFF;

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
            v |= GBA_timer_enabled(this, tn) << 7;
            return v;
        }

        case 0x04000136:
        case 0x04000137:
        case 0x04000142:
        case 0x04000143:
        case 0x0400015A:
        case 0x0400015B:
        case 0x04000206:
        case 0x04000207:
        case 0x04000302:
        case 0x04000303: return 0;

        case 0x04000300: return this->io.POSTFLG & 0xFF;

        // Unsupported stubs...
        case 0x0400012A: return this->io.SIO.send & 0xFF;
        case 0x0400012B: return this->io.SIO.send >> 8;
        case 0x04000120: return this->io.SIO.multi[0] & 0xFF;
        case 0x04000121: return this->io.SIO.multi[0] >> 8;
        case 0x04000122: return this->io.SIO.multi[1] & 0xFF;
        case 0x04000123: return this->io.SIO.multi[1] >> 8;
        case 0x04000124: return this->io.SIO.multi[2] & 0xFF;
        case 0x04000125: return this->io.SIO.multi[2] >> 8;
        case 0x04000126: return this->io.SIO.multi[3] & 0xFF;
        case 0x04000127: return this->io.SIO.multi[3] >> 8;
        case 0x04000134: return this->io.SIO.general_purpose_data & 0xFF;
        case 0x04000135: return this->io.SIO.general_purpose_data >> 8;
        case 0x04000128: return this->io.SIO.control & 0xFF;
        case 0x04000129: return this->io.SIO.control >> 8;

        case 0x04000200: return this->io.IE & 0xFF;
        case 0x04000201: return this->io.IE >> 8;
        case 0x04000202: return this->io.IF & 0xFF;
        case 0x04000203: return this->io.IF >> 8;
        case WAITCNT: {
            u32 v = this->waitstates.io.sram;
            v |= this->waitstates.io.ws0_n << 2;
            v |= this->waitstates.io.ws0_s << 4;
            v |= this->waitstates.io.ws1_n << 5;
            v |= this->waitstates.io.ws1_s << 7;

            return v;}
        case WAITCNT+1: {
            u32 v = this->waitstates.io.ws2_n;
            v |= this->waitstates.io.ws2_s << 2;
            v |= this->waitstates.io.phi_term << 3;
            v |= this->waitstates.io.empty_bit << 5;
            v |= this->cart.prefetch.enable << 6;
            return v; }
        case 0x04000208: return this->io.IME;
        case 0x04000209: return 0;

        // Unsupproted altogether...
        case 0x04000150:
        case 0x04000151:
        case 0x04000152:
        case 0x04000153:
        case 0x0400020a:
        case 0x0400020b:
            return 0;

        case 0x04076a68: // Read by Duke3D. ?
        case 0x04076a69:
            return this->io.cpu.open_bus_data & 0xFF;

        case 0x04FFFA00: // no$gba identifier
        case 0x04FFFA01:
        case 0x04FFFA02:
        case 0x04FFFA03:
            return 0;

        case 0x04fff780: // mgba debug identifier
        case 0x04fff781:
            return this->io.cpu.open_bus_data & 0xFF;
    }
    return GBA_open_bus_byte(this, addr);
}

static u32 busrd_IO(struct GBA *this, u32 addr, u32 sz, u32 access, u32 has_effect) {
    this->waitstates.current_transaction++;
    u32 r = busrd_IO8(this, addr, 1, access, has_effect) << 0;
    if (sz >= 2) {
        r |= busrd_IO8(this, addr + 1, 1, access, has_effect) << 8;
    }
    if (sz == 4) {
        r |= busrd_IO8(this, addr + 2, 1, access, has_effect) << 16;
        r |= busrd_IO8(this, addr + 3, 1, access, has_effect) << 24;
    }
    return r;
}


void GBA_eval_irqs(struct GBA *this)
{
    u32 old_line = this->cpu.regs.IRQ_line;
    this->cpu.regs.IRQ_line = (!!(this->io.IE & this->io.IF & 0x3FFF)) & this->io.IME;
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


static void enable_prefetch(struct GBA *this)
{
    u32 page = this->cpu.regs.R[15] >> 28;
    if ((page < 8) || (page >= 0xE)) { // Prefetch is enabled but not great...
        this->cart.prefetch.last_access = 0xFFFFFFFFFFFFFFFF;
    }
    else {
        this->cart.prefetch.last_access = GBA_clock_current(this);
    }
    this->cart.prefetch.cycles_banked = 0;
    this->cart.prefetch.next_addr = this->cpu.regs.R[15];
    this->cart.prefetch.duty_cycle = this->waitstates.timing16[0][page];
}

static void buswr_IO8(struct GBA *this, u32 addr, u32 sz, u32 access, u32 val) {
    val &= 0xFF;
    if (addr < 0x04000060) return GBA_PPU_mainbus_write_IO(this, addr, sz, access, val);
    u32 mask = 0xFF;
    if ((addr >= 0x4FFF600) && (addr < 0x4FFF700)) {
        this->dbg_info.mgba.str[addr - 0x4FFF600] = val & 0xFF;
        return;
    }
    switch(addr) {
        case 0x04000200: // IE lo-byte
            this->io.IE = (this->io.IE & 0xFF00) | (val & 0xFF);
            GBA_eval_irqs(this);
            return;
        case 0x04000201: // IE hi-byte
            mask <<= 8;
            val <<= 8;
            this->io.IE = (this->io.IE & 0xFF) | (val << 8);
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
        case WAITCNT:
            this->waitstates.io.sram = val & 3;
            this->waitstates.io.ws0_n = (val >> 2) & 3; // 2-3
            this->waitstates.io.ws0_s = (val >> 4) & 1; // 4
            this->waitstates.io.ws1_n = (val >> 5) & 3; // 5-6
            this->waitstates.io.ws1_s = (val >> 7) & 1; // 7
            set_waitstates(this);
            return;
        case WAITCNT+1: {
            this->waitstates.io.ws2_n = val & 3;
            this->waitstates.io.ws2_s = (val >> 2) & 1;
            this->waitstates.io.phi_term = (val >> 3) & 3;
            this->waitstates.io.empty_bit = (val >> 5) & 1;
            u32 old_enable = this->cart.prefetch.enable;
            this->cart.prefetch.enable = (val >> 6) & 1;
            set_waitstates(this);
            if (old_enable && !this->cart.prefetch.enable) {
                this->cart.prefetch.was_disabled = 1;
                //this->cart.prefetch.active = 0;
            }
            if (!old_enable && this->cart.prefetch.enable) {
                enable_prefetch(this);
            }
            return; }
        case 0x04000208: // IME lo
            this->io.IME = val & 1;
            GBA_eval_irqs(this);
            return;
        case 0x04000209: // IME hi
            return;
        case 0x04000206:
        case 0x04000207:
        case 0x0400020A:
        case 0x0400020B: // not used
        case 0x0400020C: // not used
        case 0x0400020D: // not used
        case 0x0400020E: // not used
        case 0x0400020F: // not used
        case 0x04000210: // not used
        case 0x04000211: // not used
        case 0x04000212: // not used
        case 0x04000213: // not used
        case 0x04000214: // not used
        case 0x04000215: // not used
        case 0x04000216: // not used
        case 0x04000217: // not used
        case 0x04000218: // not used
        case 0x04000219: // not used
        case 0x0400021A: // not used
        case 0x0400021B: // not used
        case 0x0400021C: // not used
        case 0x0400021D: // not used
        case 0x0400021E: // not used
        case 0x0400021F: // not used
            return;

        case 0x040000B0: this->dma[0].io.src_addr = (this->dma[0].io.src_addr & 0xFFFFFF00) | (val << 0); return; // DMA source address ch0
        case 0x040000B1: this->dma[0].io.src_addr = (this->dma[0].io.src_addr & 0xFFFF00FF) | (val << 8); return; // DMA source address ch0
        case 0x040000B2: this->dma[0].io.src_addr = (this->dma[0].io.src_addr & 0xFF00FFFF) | (val << 16); return; // DMA source address ch0
        case 0x040000B3: this->dma[0].io.src_addr = (this->dma[0].io.src_addr & 0x00FFFFFF) | ((val & 0x07) << 24); return; // DMA source address ch0
        case 0x040000B4: this->dma[0].io.dest_addr = (this->dma[0].io.dest_addr & 0xFFFFFF00) | (val << 0); return; // DMA source address ch0
        case 0x040000B5: this->dma[0].io.dest_addr = (this->dma[0].io.dest_addr & 0xFFFF00FF) | (val << 8); return; // DMA source address ch0
        case 0x040000B6: this->dma[0].io.dest_addr = (this->dma[0].io.dest_addr & 0xFF00FFFF) | (val << 16); return; // DMA source address ch0
        case 0x040000B7: this->dma[0].io.dest_addr = (this->dma[0].io.dest_addr & 0x00FFFFFF) | ((val & 0x07) << 24); return; // DMA source address ch0

        case 0x040000BC: this->dma[1].io.src_addr = (this->dma[1].io.src_addr & 0xFFFFFF00) | (val << 0); return; // DMA source address ch0
        case 0x040000BD: this->dma[1].io.src_addr = (this->dma[1].io.src_addr & 0xFFFF00FF) | (val << 8); return; // DMA source address ch0
        case 0x040000BE: this->dma[1].io.src_addr = (this->dma[1].io.src_addr & 0xFF00FFFF) | (val << 16); return; // DMA source address ch0
        case 0x040000BF: this->dma[1].io.src_addr = (this->dma[1].io.src_addr & 0x00FFFFFF) | ((val & 0x0F) << 24); return; // DMA source address ch0
        case 0x040000C0: this->dma[1].io.dest_addr = (this->dma[1].io.dest_addr & 0xFFFFFF00) | (val << 0); return; // DMA source address ch0
        case 0x040000C1: this->dma[1].io.dest_addr = (this->dma[1].io.dest_addr & 0xFFFF00FF) | (val << 8); return; // DMA source address ch0
        case 0x040000C2: this->dma[1].io.dest_addr = (this->dma[1].io.dest_addr & 0xFF00FFFF) | (val << 16); return; // DMA source address ch0
        case 0x040000C3: this->dma[1].io.dest_addr = (this->dma[1].io.dest_addr & 0x00FFFFFF) | ((val & 0x07) << 24); return; // DMA source address ch0

        case 0x040000C8: this->dma[2].io.src_addr = (this->dma[2].io.src_addr & 0xFFFFFF00) | (val << 0); return; // DMA source address ch0
        case 0x040000C9: this->dma[2].io.src_addr = (this->dma[2].io.src_addr & 0xFFFF00FF) | (val << 8); return; // DMA source address ch0
        case 0x040000CA: this->dma[2].io.src_addr = (this->dma[2].io.src_addr & 0xFF00FFFF) | (val << 16); return; // DMA source address ch0
        case 0x040000CB: this->dma[2].io.src_addr = (this->dma[2].io.src_addr & 0x00FFFFFF) | ((val & 0x0F) << 24); return; // DMA source address ch0
        case 0x040000CC: this->dma[2].io.dest_addr = (this->dma[2].io.dest_addr & 0xFFFFFF00) | (val << 0); return; // DMA source address ch0
        case 0x040000CD: this->dma[2].io.dest_addr = (this->dma[2].io.dest_addr & 0xFFFF00FF) | (val << 8); return; // DMA source address ch0
        case 0x040000CE: this->dma[2].io.dest_addr = (this->dma[2].io.dest_addr & 0xFF00FFFF) | (val << 16); return; // DMA source address ch0
        case 0x040000CF: this->dma[2].io.dest_addr = (this->dma[2].io.dest_addr & 0x00FFFFFF) | ((val & 0x07) << 24); return; // DMA source address ch0

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
            GBA_timer_write_cnt(this, (tn << 24) | val, GBA_clock_current(this), 0);
            return; }
        case 0x04000100: this->timer[0].reload = (this->timer[0].reload & 0xFF00) | val; return;
        case 0x04000104: this->timer[1].reload = (this->timer[1].reload & 0xFF00) | val; return;
        case 0x04000108: this->timer[2].reload = (this->timer[2].reload & 0xFF00) | val; return;
        case 0x0400010C: this->timer[3].reload = (this->timer[3].reload & 0xFF00) | val; return;

        case 0x04000101: this->timer[0].reload = (this->timer[0].reload & 0xFF) | (val << 8); return;
        case 0x04000105: this->timer[1].reload = (this->timer[1].reload & 0xFF) | (val << 8); return;
        case 0x04000109: this->timer[2].reload = (this->timer[2].reload & 0xFF) | (val << 8); return;
        case 0x0400010D: this->timer[3].reload = (this->timer[3].reload & 0xFF) | (val << 8); return;


        case 0x04000300: { this->io.POSTFLG = val; return; }
        case 0x04000301: { this->io.halted = 1; return; }

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
                    GBA_dma_start(ch, chnum, 0);
                }
            }
            return;}

        case 0x04000120: this->io.SIO.multi[0] = (this->io.SIO.multi[0] & 0xFF00) | val; return;
        case 0x04000121: this->io.SIO.multi[0] = (this->io.SIO.multi[0] & 0xFF) | (val << 8); return;
        case 0x04000122: this->io.SIO.multi[1] = (this->io.SIO.multi[1] & 0xFF00) | val; return;
        case 0x04000123: this->io.SIO.multi[1] = (this->io.SIO.multi[1] & 0xFF) | (val << 8); return;
        case 0x04000124: this->io.SIO.multi[2] = (this->io.SIO.multi[2] & 0xFF00) | val; return;
        case 0x04000125: this->io.SIO.multi[2] = (this->io.SIO.multi[2] & 0xFF) | (val << 8); return;
        case 0x04000126: this->io.SIO.multi[3] = (this->io.SIO.multi[3] & 0xFF00) | val; return;
        case 0x04000127: this->io.SIO.multi[3] = (this->io.SIO.multi[3] & 0xFF) | (val << 8); return;

        case 0x0400012A:
            this->io.SIO.send = (this->io.SIO.send & 0xFF00) | val; return;
        case 0x0400012B:
            this->io.SIO.send = (this->io.SIO.send & 0xFF) | (val << 8); return;
            return;

        case 0x04000130:
        case 0x04000131:
            // write to keypad? why?
            return;

        case 0x04000132:
            this->io.button_irq.buttons = (this->io.button_irq.buttons & 0b1100000000) | val;
            printf("\nWRITE VAL TO 0:%d", val);
            return;
        case 0x04000133: {
            this->io.button_irq.buttons = (this->io.button_irq.buttons & 0xFF) | ((val & 0b11) << 8);
            u32 old_enable = this->io.button_irq.enable;
            this->io.button_irq.enable = (val >> 6) & 1;
            printf("\nWRITE VAL TO 1:%d", val);
            if ((old_enable == 0) && this->io.button_irq.enable) {
                printf("\nWARNING BUTTON IRQ ENABLED...");
            }
            this->io.button_irq.condition = (val >> 7) & 1;
            return; }

        case 0x04000128: // TODO: Link cable BS
            this->io.SIO.control = (this->io.SIO.control & 0xFF00) | val;
            return;
        case 0x04000129: // TODO: Link cable BS
            this->io.SIO.control = (this->io.SIO.control & 0xFF) | val << 8;
            return;
        case 0x04000134: // TODO: Link cable BS
            this->io.SIO.general_purpose_data = (this->io.SIO.general_purpose_data & 0xFF00) | val;
            return;
        case 0x04000135:
            this->io.SIO.general_purpose_data = (this->io.SIO.general_purpose_data & 0xFF) | (val << 8);
            return;


        case 0x04000140: // TODO: link cable BS
        case 0x04000141:
        case 0x04000150:
        case 0x04000151:
        case 0x04000152:
        case 0x04000153:
        case 0x04000154:
        case 0x04000155:
        case 0x04000156:
        case 0x04000157:
        case 0x04000158:
        case 0x04000159:
            return;


        case 0x04000410: // not used
        case 0x04000411:
        case 0x040000E0:
        case 0x040000E1:
        case 0x040000E2:
        case 0x040000E3:
        case 0x040000E4:
        case 0x040000E5:
        case 0x040000E6:
        case 0x040000E7:
        case 0x040000E8:
        case 0x040000E9:
        case 0x040000EA:
        case 0x040000EB:
        case 0x040000EC:
        case 0x040000ED:
        case 0x040000EE:
        case 0x040000EF:
        case 0x040000F0:
        case 0x040000F1:
        case 0x040000F2:
        case 0x040000F3:
        case 0x040000F4:
        case 0x040000F5:
        case 0x040000F6:
        case 0x040000F7:
        case 0x040000F8:
        case 0x040000F9:
        case 0x040000FA:
        case 0x040000FB:
        case 0x040000FC:
        case 0x040000FD:
        case 0x040000FE:
        case 0x040000FF:
        case 0x04000110:
        case 0x04000111:
        case 0x04000112:
        case 0x04000113:
        case 0x04000114:
        case 0x04000115:
        case 0x04000116:
        case 0x04000117:
        case 0x04000118:
        case 0x04000119:
        case 0x0400011A:
        case 0x0400011B:
        case 0x0400011C:
        case 0x0400011D:
        case 0x0400011E:
        case 0x0400011F:
        case 0x0400012C:
        case 0x0400012D:
        case 0x0400012E:
        case 0x0400012F:

        case 0x04000142:
        case 0x04000143:
        case 0x04000144:
        case 0x04000145:
        case 0x04000146:
        case 0x04000147:
        case 0x04000148:
        case 0x04000149:
        case 0x0400014A:
        case 0x0400014B:
        case 0x0400014C:
        case 0x0400014D:
        case 0x0400014E:
        case 0x0400014F:
        case 0x0400015A:
        case 0x0400015B:
        case 0x0400015C:
        case 0x0400015D:
        case 0x0400015E:
        case 0x0400015F:

            return;
    }
    //buswr_invalid(this, addr, sz, access, val);
}

static void buswr_IO(struct GBA *this, u32 addr, u32 sz, u32 access, u32 val) {
    this->waitstates.current_transaction++;
    if ((addr >= 0x04000060) && (addr < 0x040000B0)) return GBA_APU_write_IO(this, addr, sz, access, val);

    if (addr == 0x04fff780) {
        assert(sz == 2);
        if (val == 0xc0de) this->dbg_info.mgba.enable = 1;
        return;
    }
    if (this->dbg_info.mgba.enable) {
        if (addr == 0x04fff700) {
            //if (val & 0x100) {
            if (this->dbg_info.mgba.str[0] != 0) {
                //printf("\n%s", this->dbg_info.mgba.str);
                memset(this->dbg_info.mgba.str, 0, sizeof(this->dbg_info.mgba.str));
            }
            return;
        }
    }
    buswr_IO8(this, addr, 1, access, (val >> 0) & 0xFF);
    if (sz >= 2) {
        buswr_IO8(this, addr + 1, 1, access, (val >> 8) & 0xFF);
    }
    if (sz == 4) {
        buswr_IO8(this, addr + 2, 1, access, (val >> 16) & 0xFF);
        buswr_IO8(this, addr + 3, 1, access, (val >> 24) & 0xFF);
    }
}

void GBA_bus_init(struct GBA *this)
{
    for (u32 i = 0; i < 4; i++) {
        struct GBA_TIMER *t = &this->timer[i];
        t->overflow_at = 0xFFFFFFFFFFFFFFFF;
        t->enable_at = 0xFFFFFFFFFFFFFFFF;
    }
    for (u32 i = 0; i < 16; i++) {
        this->mem.read[i] = &busrd_invalid;
        this->mem.write[i] = &buswr_invalid;
    }
    memset(this->dbg_info.mgba.str, 0, sizeof(this->dbg_info.mgba.str));
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
    this->mem.read[0xF] = &GBA_cart_read_sram;

    this->mem.write[0x0] = &buswr_bios;
    this->mem.write[0x2] = &buswr_WRAM_slow;
    this->mem.write[0x3] = &buswr_WRAM_fast;
    this->mem.write[0x4] = &buswr_IO;
    this->mem.write[0x5] = &GBA_PPU_mainbus_write_palette;
    this->mem.write[0x6] = &GBA_PPU_mainbus_write_VRAM;
    this->mem.write[0x7] = &GBA_PPU_mainbus_write_OAM;
    this->mem.write[0x8] = &GBA_cart_write;
    this->mem.write[0x9] = &GBA_cart_write;
    this->mem.write[0xA] = &GBA_cart_write;
    this->mem.write[0xB] = &GBA_cart_write;
    this->mem.write[0xC] = &GBA_cart_write;
    this->mem.write[0xD] = &GBA_cart_write;
    this->mem.write[0xE] = &GBA_cart_write_sram;
    this->mem.write[0xF] = &GBA_cart_write_sram;

    set_waitstates(this);
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
    trace_view_printf(tv, 1, "%lld", this->clock.master_cycle_count + this->waitstates.current_transaction);
    trace_view_printf(tv, 2, "%08x", addr);
    trace_view_printf(tv, 3, "%08x", val);
    trace_view_endline(tv);
}

static void trace_write(struct GBA *this, u32 addr, u32 sz, u32 val)
{
    struct trace_view *tv = this->cpu.dbg.tvptr;
    if (!tv) return;
    trace_view_startline(tv, 2);
    trace_view_printf(tv, 0, "BUSwr");
    trace_view_printf(tv, 1, "%lld", this->clock.master_cycle_count + this->waitstates.current_transaction);
    trace_view_printf(tv, 2, "%08x", addr);
    trace_view_printf(tv, 3, "%08x", val);
    trace_view_endline(tv);
}


u32 GBA_mainbus_read(void *ptr, u32 addr, u32 sz, u32 access, u32 has_effect)
{
    struct GBA *this = (struct GBA *)ptr;
    u32 v;

    if (addr < 0x10000000) v = this->mem.read[(addr >> 24) & 15](this, addr, sz, access, has_effect);
    else v = busrd_invalid(this, addr, sz, access, has_effect);
    if (dbg.trace_on) trace_read(this, addr, sz, v);
    return v;
}

u32 GBA_mainbus_fetchins(void *ptr, u32 addr, u32 sz, u32 access)
{
    struct GBA *this = (struct GBA*)ptr;
    u32 v = GBA_mainbus_read(ptr, addr, sz, access, 1);
    switch(sz) {
        case 4:
            this->io.cpu.open_bus_data = v;
            break;
        case 2:
            this->io.cpu.open_bus_data = (v << 16) | v;
            break;
    }
    return v;
}

void GBA_mainbus_write(void *ptr, u32 addr, u32 sz, u32 access, u32 val)
{
    struct GBA *this = (struct GBA *)ptr;
    if (dbg.trace_on) trace_write(this, addr, sz, val);
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
            GBA_dma_start(ch, i, 0);
        }
    }
    // And if it's channel 3 and "special", if we're in the correct lines.
}

u32 GBA_open_bus_byte(struct GBA *this, u32 addr)
{
    switch(addr & 3) {
        case 0:
            return this->io.cpu.open_bus_data & 0xFF;
        case 1:
            return (this->io.cpu.open_bus_data >> 8) & 0xFF;
        case 2:
            return (this->io.cpu.open_bus_data >> 16) & 0xFF;
        case 3:
            return (this->io.cpu.open_bus_data >> 24) & 0xFF;
    }
    return 0;
}

u32 GBA_open_bus(struct GBA *this, u32 addr, u32 sz)
{
    u32 v = GBA_open_bus_byte(this, addr);
    if (sz >= 2) {
        v |= GBA_open_bus_byte(this, addr+1) << 8;
    }
    if (sz == 4) {
        v |= GBA_open_bus_byte(this, addr+2) << 16;
        v |= GBA_open_bus_byte(this, addr+3) << 24;
    }
    return v;
}


void GBA_check_dma_at_vblank(struct GBA *this)
{
    // Check if any DMA channels are at enabled=1, started=0, time=hblank
    for (u32 i = 0; i < 4; i++) {
        struct GBA_DMA_ch *ch = &this->dma[i];
        if ((ch->io.enable) && (!ch->op.started) && (ch->io.start_timing == 1)) {
            GBA_dma_start(ch, i, 0);
        }
    }
}

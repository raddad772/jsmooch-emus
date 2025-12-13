//
// Created by . on 12/4/24.
//
#include <cstring>
#include <cassert>

#include "gba_apu.h"
#include "gba_bus.h"
#include "gba_timers.h"
#include "gba_dma.h"
#include "helpers/multisize_memaccess.cpp"

namespace GBA {
static u32 read_trace_cpu(void *ptr, u32 addr, u8 sz) {
    auto *th = static_cast<GBA::core *>(ptr);
    return core::mainbus_read(ptr, addr, sz, th->io.cpu.open_bus_data, false);
}

core::core() :
    cpu{&clock.master_cycle_count, &waitstates.current_transaction, &this->scheduler},
    cart{this},
    ppu{this},
    apu{this},
    scheduler{&clock.master_cycle_count},
    dma{this},
    timer{TIMER(this, 0), TIMER(this, 1), TIMER(this, 2), TIMER(this, 3)}
{   has.load_BIOS = true;
    has.save_state = false;
    has.set_audiobuf = true;

    memset(dbg_info.mgba.str, 0, sizeof(dbg_info.mgba.str));
    mem.read[0x0] = &busrd_bios;
    mem.read[0x2] = &core::busrd_WRAM_slow;
    mem.read[0x3] = &core::busrd_WRAM_fast;
    mem.read[0x4] = &core::busrd_IO;
    mem.read[0x5] = &PPU::core::mainbus_read_palette;
    mem.read[0x6] = &PPU::core::mainbus_read_VRAM;
    mem.read[0x7] = &PPU::core::mainbus_read_OAM;
    mem.read[0x8] = &cart::core::read_wait0;
    mem.read[0x9] = &cart::core::read_wait0;
    mem.read[0xA] = &cart::core::read_wait1;
    mem.read[0xB] = &cart::core::read_wait1;
    mem.read[0xC] = &cart::core::read_wait2;
    mem.read[0xD] = &cart::core::read_wait2;
    mem.read[0xE] = &cart::core::read_sram;
    mem.read[0xF] = &cart::core::read_sram;

    mem.write[0x0] = &core::buswr_bios;
    mem.write[0x2] = &core::buswr_WRAM_slow;
    mem.write[0x3] = &core::buswr_WRAM_fast;
    mem.write[0x4] = &core::buswr_IO;
    mem.write[0x5] = &PPU::core::mainbus_write_palette;
    mem.write[0x6] = &PPU::core::mainbus_write_VRAM;
    mem.write[0x7] = &PPU::core::mainbus_write_OAM;
    mem.write[0x8] = &cart::core::write;
    mem.write[0x9] = &cart::core::write;
    mem.write[0xA] = &cart::core::write;
    mem.write[0xB] = &cart::core::write;
    mem.write[0xC] = &cart::core::write;
    mem.write[0xD] = &cart::core::write;
    mem.write[0xE] = &cart::core::write_sram;
    mem.write[0xF] = &cart::core::write_sram;

    set_waitstates();
    scheduler.max_block_size = 8;
    scheduler.run.func = &block_step_cpu;
    scheduler.run.ptr = this;
    cpu.read_data_ptr = this;
    cpu.write_data_ptr = this;
    cpu.read_data = &mainbus_read;
    cpu.write_data = &mainbus_write;
    cpu.read_ins_ptr = this;
    cpu.read_ins = &mainbus_fetchins;
    snprintf(label, sizeof(label), "GameBoy Advance");
    jsm_debug_read_trace dt;
    dt.read_trace_arm = &read_trace_cpu;
    dt.ptr = this;
    cpu.setup_tracing(&dt, &clock.master_cycle_count, 1);

    jsm.described_inputs = false;
    jsm.cycles_left = 0;
    ppu.dbg.events.view = dbg.events.view;
}

static constexpr u32 maskalign[5] = {0, 0xFFFFFFFF, 0xFFFFFFFE, 0, 0xFFFFFFFC};

u32 core::busrd_invalid(core *th, u32 addr, u8 sz, u8 access, bool has_effect) {
    printf("\nREAD UNKNOWN ADDR:%08x sz:%d", addr, sz);
    th->waitstates.current_transaction++;
    if (addr == 0x83001888) {
        int a = 4;
        a++;
    }
    //dbg.var++;
    //if (dbg.var > 15) dbg_break("too many bad reads", clock.master_cycle_count);
    u32 v = th->open_bus_byte(addr);
    if (sz >= 2) {
        v |= th->open_bus_byte(addr+1) << 8;
    }
    if (sz == 4) {
        v |= th->open_bus_byte(addr+2) << 16;
        v |= th->open_bus_byte(addr+3) << 24;
    }
    return v;
}

void core::buswr_invalid(core *th, u32 addr, u8 sz, u8 access, u32 val) {
    printf("\nWRITE UNKNOWN ADDR:%08x sz:%d DATA:%08x", addr, sz, val);
    th->waitstates.current_transaction++;
    ::dbg.var++;
    //if (dbg.var > 15) dbg_break("too many bad writes", clock.master_cycle_count);
}

#define WAITCNT 0x04000204

u32 core::busrd_bios(core *th, u32 addr, u8 sz, u8 access, bool has_effect) {
    addr &= maskalign[sz];
    th->waitstates.current_transaction++;
    if (addr < 0x4000) {
        if (th->cpu.regs.R[15] < 0x4000) {
            const u32 v = cR[sz](th->BIOS.data, addr);
            th->io.bios_open_bus = v;
            return v;
        }
        else {
            return th->io.bios_open_bus;
        }
    }

    return th->open_bus(addr, sz);
}

void core::buswr_bios(core *th, u32 addr, u8 sz, u8 access, u32 val) {
    th->waitstates.current_transaction++;
    //printf("\nWarning write to BIOS...");
}

u32 core::busrd_WRAM_slow(core *th, u32 addr, u8 sz, u8 access, bool has_effect) {
    addr &= maskalign[sz];
    th->waitstates.current_transaction += 3;
    if (sz == 4) {
        addr &= ~3;
        th->waitstates.current_transaction += 3;
    }
    if (sz == 2) addr &= ~1;
    return cR[sz](th->WRAM_slow, addr & 0x3FFFF);
}

u32 core::busrd_WRAM_fast(core *th, u32 addr, u8 sz, u8 access, bool has_effect) {
    addr &= maskalign[sz];
    th->waitstates.current_transaction++;
    if (sz == 4) addr &= ~3;
    if (sz == 2) addr &= ~1;
    return cR[sz](th->WRAM_fast, addr & 0x7FFF);
}

void core::buswr_WRAM_slow(core *th, u32 addr, u8 sz, u8 access, u32 val) {
    addr &= maskalign[sz];
    th->waitstates.current_transaction += 3;
    if (sz == 4) {
        addr &= ~3;
        th->waitstates.current_transaction += 3;
    }
    if (sz == 4) addr &= ~3;
    if (sz == 2) addr &= ~1;
    cW[sz](th->WRAM_slow, addr & 0x3FFFF, val);
}

void core::set_waitstates() {

// 8 and 8+1  are set to...based on the thing....
#define DOV4(n, a, b, c, d) switch(waitstates.io. n) { case 0: waitstates. n = a; break; case 1: waitstates. n = b; break; case 2: waitstates. n = c; break; case 3: waitstates. n = d; break; }
#define DOV2(n, a, b) switch(waitstates.io. n) { case 0: waitstates. n = a; break; case 1: waitstates. n = b; break; }
    DOV4(sram, 5, 4, 3, 9);
    
    // OK we need to fill up 3 sections based on Nonsequential and Sequential
    // Nonsequentiual gets 4 possible values, sequential gets different values
    static constexpr u32 nonseq[4] = {5, 4, 3, 9};
    static constexpr u32 seq0[2] = {3, 2};
    static constexpr u32 seq1[2] = {5, 2};
    static constexpr u32 seq2[2] = {9, 2};
    
#define t16 waitstates.timing16
#define t32 waitstates.timing32
#define WS0 (0x8+i)
#define WS1 (0xA+i)
#define WS2 (0xC+i)
    for (u32 i = 0; i < 1; i++) {
        t16[0][WS0] = nonseq[waitstates.io.ws0_n];
        t16[0][WS1] = nonseq[waitstates.io.ws1_n];
        t16[0][WS2] = nonseq[waitstates.io.ws2_n];
        t16[1][WS0] = seq0[waitstates.io.ws0_s];
        t16[1][WS1] = seq1[waitstates.io.ws1_s];
        t16[1][WS2] = seq2[waitstates.io.ws2_s];
        
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
#define d(x) waitstates. x
    //printf("\n0n:%d 0s:%d 1n:%d 1s:%d 2n:%d 2s:%d", d(ws0_n), d(ws0_s), d(ws1_n), d(ws1_s), d(ws2_n), d(ws2_s));
#undef d
}


void core::buswr_WRAM_fast(core *th, u32 addr, u8 sz, u8 access, u32 val) {
    addr &= maskalign[sz];
    th->waitstates.current_transaction++;
    if (sz == 4) addr &= ~3;
    if (sz == 2) addr &= ~1;
    cW[sz](th->WRAM_fast, addr & 0x7FFF, val);
}

static inline u32 DMA_CH_NUM(u32 addr)
{
    addr &= 0xFF;
    if (addr < 0xBC) return 0;
    if (addr < 0xC8) return 1;
    if (addr < 0xD4) return 2;
    return 3;
}

u32 core::busrd_IO8(u32 addr, u8 sz, u8 access, bool has_effect) {
    if (addr < 0x4000060) return PPU::core::mainbus_read_IO(this, addr, sz, access, has_effect);
    if ((addr >= 0x04000060) && (addr < 0x040000B0)) return apu.read_IO(addr, sz, access, has_effect);

    switch(addr) {
        case 0x04000130: // buttons!!!
            return controller.get_state() & 0xFF;
        case 0x04000131: // buttons!!!
            return controller.get_state() >> 8;

        // 40000BA DMA0cnt
        // 40000C6 DMA1cnt
        // 40000D2 DMA2cnt
        // 40000DE DMA3cnt
        case 0x040000BA:
        case 0x040000C6:
        case 0x040000D2:
        case 0x040000DE: {
            const auto &ch = dma.channel[DMA_CH_NUM(addr)];
            u32 v = ch.io.dest_addr_ctrl << 5;
            v |= ch.io.src_addr_ctrl << 7;
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
            auto &ch = dma.channel[chnum];
            u32 v = (ch.io.src_addr_ctrl >> 1) & 1;
            v |= ch.io.repeat << 1;
            v |= ch.io.transfer_size << 2;
            if (chnum == 3) v |= ch.io.game_pak_drq << 3;
            v |= ch.io.start_timing << 4;
            v |= ch.io.irq_on_end << 6;
            v |= ch.io.enable << 7;
            return v;}

        case 0x04000100: return (timer[0].read() >> 0) & 0xFF;
        case 0x04000101: return (timer[0].read() >> 8) & 0xFF;
        case 0x04000104: return (timer[1].read() >> 0) & 0xFF;
        case 0x04000105: return (timer[1].read() >> 8) & 0xFF;
        case 0x04000108: return (timer[2].read() >> 0) & 0xFF;
        case 0x04000109: return (timer[2].read() >> 8) & 0xFF;
        case 0x0400010C: return (timer[3].read() >> 0) & 0xFF;
        case 0x0400010D: return (timer[3].read() >> 8) & 0xFF;

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
            u32 v = timer[tn].divider.io;
            v |= timer[tn].cascade << 2;
            v |= timer[tn].irq_on_overflow << 6;
            v |= timer[tn].enabled() << 7;
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

        case 0x04000300: return io.POSTFLG & 0xFF;

        // Unsupported stubs...
        case 0x0400012A: return io.SIO.send & 0xFF;
        case 0x0400012B: return io.SIO.send >> 8;
        case 0x04000120: return io.SIO.multi[0] & 0xFF;
        case 0x04000121: return io.SIO.multi[0] >> 8;
        case 0x04000122: return io.SIO.multi[1] & 0xFF;
        case 0x04000123: return io.SIO.multi[1] >> 8;
        case 0x04000124: return io.SIO.multi[2] & 0xFF;
        case 0x04000125: return io.SIO.multi[2] >> 8;
        case 0x04000126: return io.SIO.multi[3] & 0xFF;
        case 0x04000127: return io.SIO.multi[3] >> 8;
        case 0x04000134: return io.SIO.general_purpose_data & 0xFF;
        case 0x04000135: return io.SIO.general_purpose_data >> 8;
        case 0x04000128: return io.SIO.control & 0xFF;
        case 0x04000129: return io.SIO.control >> 8;

        case 0x04000200: return io.IE & 0xFF;
        case 0x04000201: return io.IE >> 8;
        case 0x04000202: return io.IF & 0xFF;
        case 0x04000203: return io.IF >> 8;
        case WAITCNT: {
            u32 v = waitstates.io.sram;
            v |= waitstates.io.ws0_n << 2;
            v |= waitstates.io.ws0_s << 4;
            v |= waitstates.io.ws1_n << 5;
            v |= waitstates.io.ws1_s << 7;

            return v;}
        case WAITCNT+1: {
            u32 v = waitstates.io.ws2_n;
            v |= waitstates.io.ws2_s << 2;
            v |= waitstates.io.phi_term << 3;
            v |= waitstates.io.empty_bit << 5;
            v |= cart.prefetch.enable << 6;
            return v; }
        case 0x04000208: return io.IME;
        case 0x04000209:

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
            return io.cpu.open_bus_data & 0xFF;

        case 0x04FFFA00: // no$gba identifier
        case 0x04FFFA01:
        case 0x04FFFA02:
        case 0x04FFFA03:
            return 0;

        case 0x04fff780: // mgba debug identifier
        case 0x04fff781:
            return io.cpu.open_bus_data & 0xFF;
        default:
    }
    return open_bus_byte(addr);
}

u32 core::busrd_IO(core *th, u32 addr, u8 sz, u8 access, bool has_effect) {
    addr &= maskalign[sz];
    th->waitstates.current_transaction++;
    u32 r = th->busrd_IO8(addr, 1, access, has_effect) << 0;
    if (sz >= 2) {
        r |= th->busrd_IO8(addr + 1, 1, access, has_effect) << 8;
    }
    if (sz == 4) {
        r |= th->busrd_IO8(addr + 2, 1, access, has_effect) << 16;
        r |= th->busrd_IO8(addr + 3, 1, access, has_effect) << 24;
    }
    return r;
}


void core::eval_irqs()
{
    u32 old_line = cpu.regs.IRQ_line;
    cpu.regs.IRQ_line = (!!(io.IE & io.IF & 0x3FFF)) & io.IME;
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


void core::enable_prefetch()
{
    u32 page = cpu.regs.R[15] >> 28;
    if ((page < 8) || (page >= 0xE)) { // Prefetch is enabled but not great...
        cart.prefetch.last_access = 0xFFFFFFFFFFFFFFFF;
    }
    else {
        cart.prefetch.last_access = clock_current();
    }
    cart.prefetch.cycles_banked = 0;
    cart.prefetch.next_addr = cpu.regs.R[15];
    cart.prefetch.duty_cycle = waitstates.timing16[0][page];
}

static void block_step_halted(void *ptr, u64 key, u64 clock, u32 jitter)
{
    auto *th = static_cast<core *>(ptr);
    th->io.halted &= ((!!(th->io.IF & th->io.IE)) ^ 1);
    if (!th->io.halted) {
        th->waitstates.current_transaction = 1;
        th->scheduler.run.func = &block_step_cpu;
    }
    else {
        th->clock.master_cycle_count = th->scheduler.first_event->timecode;
        th->waitstates.current_transaction = 0;
    }
}

void core::buswr_IO8(u32 addr, u8 sz, u8 access, u32 val) {
    val &= 0xFF;
    if (addr < 0x04000060) return PPU::core::mainbus_write_IO(this, addr, sz, access, val);
    //u32 mask = 0xFF;
    if ((addr >= 0x4FFF600) && (addr < 0x4FFF700)) {
        dbg_info.mgba.str[addr - 0x4FFF600] = static_cast<char>(val & 0xFF);
        return;
    }
    switch(addr) {
        case 0x04000200: // IE lo-byte
            io.IE = (io.IE & 0xFF00) | val;
            eval_irqs();
            return;
        case 0x04000201: // IE hi-byte
            io.IE = (io.IE & 0xFF) | (val << 8);
            eval_irqs();
            return;
        case 0x04000202: // IF lo-byte
            io.IF &= ~val;
            eval_irqs();
            return;
        case 0x04000203: // IF hi-byte
            io.IF &= ~(val << 8);
            eval_irqs();
            return;
        case WAITCNT:
            waitstates.io.sram = val & 3;
            waitstates.io.ws0_n = (val >> 2) & 3; // 2-3
            waitstates.io.ws0_s = (val >> 4) & 1; // 4
            waitstates.io.ws1_n = (val >> 5) & 3; // 5-6
            waitstates.io.ws1_s = (val >> 7) & 1; // 7
            set_waitstates();
            return;
        case WAITCNT+1: {
            waitstates.io.ws2_n = val & 3;
            waitstates.io.ws2_s = (val >> 2) & 1;
            waitstates.io.phi_term = (val >> 3) & 3;
            waitstates.io.empty_bit = (val >> 5) & 1;
            u32 old_enable = cart.prefetch.enable;
            cart.prefetch.enable = (val >> 6) & 1;
            set_waitstates();
            if (old_enable && !cart.prefetch.enable) {
                cart.prefetch.was_disabled = true;
                //cart.prefetch.active = 0;
            }
            if (!old_enable && cart.prefetch.enable) {
                enable_prefetch();
            }
            return; }
        case 0x04000208: // IME lo
            io.IME = val & 1;
            eval_irqs();
            return;
        case 0x04000209: // IME hi
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

        case 0x040000B0: dma.channel[0].io.src_addr = (dma.channel[0].io.src_addr & 0xFFFFFF00) | (val << 0); return; // DMA source address ch0
        case 0x040000B1: dma.channel[0].io.src_addr = (dma.channel[0].io.src_addr & 0xFFFF00FF) | (val << 8); return; // DMA source address ch0
        case 0x040000B2: dma.channel[0].io.src_addr = (dma.channel[0].io.src_addr & 0xFF00FFFF) | (val << 16); return; // DMA source address ch0
        case 0x040000B3: dma.channel[0].io.src_addr = (dma.channel[0].io.src_addr & 0x00FFFFFF) | ((val & 0x07) << 24); return; // DMA source address ch0
        case 0x040000B4: dma.channel[0].io.dest_addr = (dma.channel[0].io.dest_addr & 0xFFFFFF00) | (val << 0); return; // DMA source address ch0
        case 0x040000B5: dma.channel[0].io.dest_addr = (dma.channel[0].io.dest_addr & 0xFFFF00FF) | (val << 8); return; // DMA source address ch0
        case 0x040000B6: dma.channel[0].io.dest_addr = (dma.channel[0].io.dest_addr & 0xFF00FFFF) | (val << 16); return; // DMA source address ch0
        case 0x040000B7: dma.channel[0].io.dest_addr = (dma.channel[0].io.dest_addr & 0x00FFFFFF) | ((val & 0x07) << 24); return; // DMA source address ch0

        case 0x040000BC: dma.channel[1].io.src_addr = (dma.channel[1].io.src_addr & 0xFFFFFF00) | (val << 0); return; // DMA source address ch0
        case 0x040000BD: dma.channel[1].io.src_addr = (dma.channel[1].io.src_addr & 0xFFFF00FF) | (val << 8); return; // DMA source address ch0
        case 0x040000BE: dma.channel[1].io.src_addr = (dma.channel[1].io.src_addr & 0xFF00FFFF) | (val << 16); return; // DMA source address ch0
        case 0x040000BF: dma.channel[1].io.src_addr = (dma.channel[1].io.src_addr & 0x00FFFFFF) | ((val & 0x0F) << 24); return; // DMA source address ch0
        case 0x040000C0: dma.channel[1].io.dest_addr = (dma.channel[1].io.dest_addr & 0xFFFFFF00) | (val << 0); return; // DMA source address ch0
        case 0x040000C1: dma.channel[1].io.dest_addr = (dma.channel[1].io.dest_addr & 0xFFFF00FF) | (val << 8); return; // DMA source address ch0
        case 0x040000C2: dma.channel[1].io.dest_addr = (dma.channel[1].io.dest_addr & 0xFF00FFFF) | (val << 16); return; // DMA source address ch0
        case 0x040000C3: dma.channel[1].io.dest_addr = (dma.channel[1].io.dest_addr & 0x00FFFFFF) | ((val & 0x07) << 24); return; // DMA source address ch0

        case 0x040000C8: dma.channel[2].io.src_addr = (dma.channel[2].io.src_addr & 0xFFFFFF00) | (val << 0); return; // DMA source address ch0
        case 0x040000C9: dma.channel[2].io.src_addr = (dma.channel[2].io.src_addr & 0xFFFF00FF) | (val << 8); return; // DMA source address ch0
        case 0x040000CA: dma.channel[2].io.src_addr = (dma.channel[2].io.src_addr & 0xFF00FFFF) | (val << 16); return; // DMA source address ch0
        case 0x040000CB: dma.channel[2].io.src_addr = (dma.channel[2].io.src_addr & 0x00FFFFFF) | ((val & 0x0F) << 24); return; // DMA source address ch0
        case 0x040000CC: dma.channel[2].io.dest_addr = (dma.channel[2].io.dest_addr & 0xFFFFFF00) | (val << 0); return; // DMA source address ch0
        case 0x040000CD: dma.channel[2].io.dest_addr = (dma.channel[2].io.dest_addr & 0xFFFF00FF) | (val << 8); return; // DMA source address ch0
        case 0x040000CE: dma.channel[2].io.dest_addr = (dma.channel[2].io.dest_addr & 0xFF00FFFF) | (val << 16); return; // DMA source address ch0
        case 0x040000CF: dma.channel[2].io.dest_addr = (dma.channel[2].io.dest_addr & 0x00FFFFFF) | ((val & 0x07) << 24); return; // DMA source address ch0

        case 0x040000D4: dma.channel[3].io.src_addr = (dma.channel[3].io.src_addr & 0xFFFFFF00) | (val << 0); return; // DMA source address ch0
        case 0x040000D5: dma.channel[3].io.src_addr = (dma.channel[3].io.src_addr & 0xFFFF00FF) | (val << 8); return; // DMA source address ch0
        case 0x040000D6: dma.channel[3].io.src_addr = (dma.channel[3].io.src_addr & 0xFF00FFFF) | (val << 16); return; // DMA source address ch0
        case 0x040000D7: dma.channel[3].io.src_addr = (dma.channel[3].io.src_addr & 0x00FFFFFF) | ((val & 0x0F) << 24); return; // DMA source address ch0
        case 0x040000D8: dma.channel[3].io.dest_addr = (dma.channel[3].io.dest_addr & 0xFFFFFF00) | (val << 0); return; // DMA source address ch0
        case 0x040000D9: dma.channel[3].io.dest_addr = (dma.channel[3].io.dest_addr & 0xFFFF00FF) | (val << 8); return; // DMA source address ch0
        case 0x040000DA: dma.channel[3].io.dest_addr = (dma.channel[3].io.dest_addr & 0xFF00FFFF) | (val << 16); return; // DMA source address ch0
        case 0x040000DB: dma.channel[3].io.dest_addr = (dma.channel[3].io.dest_addr & 0x00FFFFFF) | ((val & 0x0F) << 24); return; // DMA source address ch0

        case 0x040000B8: dma.channel[0].io.word_count = (dma.channel[0].io.word_count & 0x3F00) | (val << 0); return;
        case 0x040000B9: dma.channel[0].io.word_count = (dma.channel[0].io.word_count & 0xFF) | ((val & 0x3F) << 8); return;
        case 0x040000C4: dma.channel[1].io.word_count = (dma.channel[1].io.word_count & 0x3F00) | (val << 0); return;
        case 0x040000C5: dma.channel[1].io.word_count = (dma.channel[1].io.word_count & 0xFF) | ((val & 0x3F) << 8); return;
        case 0x040000D0: dma.channel[2].io.word_count = (dma.channel[2].io.word_count & 0x3F00) | (val << 0); return;
        case 0x040000D1: dma.channel[2].io.word_count = (dma.channel[2].io.word_count & 0xFF) | ((val & 0x3F) << 8); return;
        case 0x040000DC: dma.channel[3].io.word_count = (dma.channel[3].io.word_count & 0xFF00) | (val << 0); return;
        case 0x040000DD: dma.channel[3].io.word_count = (dma.channel[3].io.word_count & 0xFF) | ((val & 0xFF) << 8); return;

        case 0x04000103: // TMRxCNT upper half unused
        case 0x04000107:
        case 0x0400010B:
        case 0x0400010F:
            return;

        case 0x04000102:
        case 0x04000106:
        case 0x0400010A:
        case 0x0400010E: {
            const u32 tn = (addr >> 2) & 3;
            timer[tn].write_cnt(val, clock_current(), 0);
            return; }
        case 0x04000100: timer[0].reload = (timer[0].reload & 0xFF00) | val; return;
        case 0x04000104: timer[1].reload = (timer[1].reload & 0xFF00) | val; return;
        case 0x04000108: timer[2].reload = (timer[2].reload & 0xFF00) | val; return;
        case 0x0400010C: timer[3].reload = (timer[3].reload & 0xFF00) | val; return;

        case 0x04000101: timer[0].reload = (timer[0].reload & 0xFF) | (val << 8); return;
        case 0x04000105: timer[1].reload = (timer[1].reload & 0xFF) | (val << 8); return;
        case 0x04000109: timer[2].reload = (timer[2].reload & 0xFF) | (val << 8); return;
        case 0x0400010D: timer[3].reload = (timer[3].reload & 0xFF) | (val << 8); return;


        case 0x04000300: { io.POSTFLG = val; return; }
        case 0x04000301: { io.halted = 1; scheduler.run.func = &block_step_halted;  return; }

        // DMA enable 0->1 while start timing = 0 will start it
        case 0x040000BA:
        case 0x040000C6:
        case 0x040000D2:
        case 0x040000DE: {
            u32 chn = DMA_CH_NUM(addr);
            auto &ch = dma.channel[chn];
            ch.io.dest_addr_ctrl = (val >> 5) & 3;
            ch.io.src_addr_ctrl = (ch.io.src_addr_ctrl & 2) | ((val >> 7) & 1);
            ch.on_modify_write();
            return;}
        case 0x040000BB:
        case 0x040000C7:
        case 0x040000D3:
        case 0x040000DF: {
            const u32 chnum = DMA_CH_NUM(addr);
            auto &ch = dma.channel[chnum];
            ch.io.src_addr_ctrl = (ch.io.src_addr_ctrl & 1) | ((val & 1) << 1);
            ch.io.repeat = (val >> 1) & 1;
            ch.io.transfer_size = (val >> 2) & 1;
            if (chnum == 3) ch.io.game_pak_drq = (val >> 3) & 1;
            ch.io.start_timing = (val >> 4) & 3;
            ch.io.irq_on_end = (val >> 6) & 1;
            u32 old_enable = ch.io.enable;
            ch.io.enable = (val >> 7) & 1;
            ch.cnt_written(old_enable);
            ch.on_modify_write();
            return;}

        case 0x04000120: io.SIO.multi[0] = (io.SIO.multi[0] & 0xFF00) | val; return;
        case 0x04000121: io.SIO.multi[0] = (io.SIO.multi[0] & 0xFF) | (val << 8); return;
        case 0x04000122: io.SIO.multi[1] = (io.SIO.multi[1] & 0xFF00) | val; return;
        case 0x04000123: io.SIO.multi[1] = (io.SIO.multi[1] & 0xFF) | (val << 8); return;
        case 0x04000124: io.SIO.multi[2] = (io.SIO.multi[2] & 0xFF00) | val; return;
        case 0x04000125: io.SIO.multi[2] = (io.SIO.multi[2] & 0xFF) | (val << 8); return;
        case 0x04000126: io.SIO.multi[3] = (io.SIO.multi[3] & 0xFF00) | val; return;
        case 0x04000127: io.SIO.multi[3] = (io.SIO.multi[3] & 0xFF) | (val << 8); return;

        case 0x0400012A:
            io.SIO.send = (io.SIO.send & 0xFF00) | val; return;
        case 0x0400012B:
            io.SIO.send = (io.SIO.send & 0xFF) | (val << 8); return;

        case 0x04000130:
        case 0x04000131:
            // write to keypad? why?
            return;

        case 0x04000132:
            io.button_irq.buttons = (io.button_irq.buttons & 0b1100000000) | val;

            return;
        case 0x04000133: {
            io.button_irq.buttons = (io.button_irq.buttons & 0xFF) | ((val & 0b11) << 8);
            u32 old_enable = io.button_irq.enable;
            io.button_irq.enable = (val >> 6) & 1;
            if ((old_enable == 0) && io.button_irq.enable) {
                printf("\nWARNING BUTTON IRQ ENABLED...");
            }
            io.button_irq.condition = (val >> 7) & 1;
            return; }

        case 0x04000128: // TODO: Link cable BS
            io.SIO.control = (io.SIO.control & 0xFF00) | val;
            return;
        case 0x04000129: // TODO: Link cable BS
            io.SIO.control = (io.SIO.control & 0xFF) | val << 8;
            return;
        case 0x04000134: // TODO: Link cable BS
            io.SIO.general_purpose_data = (io.SIO.general_purpose_data & 0xFF00) | val;
            return;
        case 0x04000135:
            io.SIO.general_purpose_data = (io.SIO.general_purpose_data & 0xFF) | (val << 8);
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
        default:
            return;
    }
}

void core::buswr_IO(core *th, u32 addr, u8 sz, u8 access, u32 val) {
    addr &= maskalign[sz];
    th->waitstates.current_transaction++;
    if ((addr >= 0x04000060) && (addr < 0x040000B0)) return th->apu.write_IO(addr, sz, val);

    if (addr == 0x04fff780) {
        assert(sz == 2);
        if (val == 0xc0de) th->dbg_info.mgba.enable = 1;
        return;
    }
    if (th->dbg_info.mgba.enable) {
        if (addr == 0x04fff700) {
            //if (val & 0x100) {
            if (th->dbg_info.mgba.str[0] != 0) {
                //printf("\n%s", dbg_info.mgba.str);
                memset(th->dbg_info.mgba.str, 0, sizeof(dbg_info.mgba.str));
            }
            return;
        }
    }
    th->buswr_IO8(addr, 1, access, (val >> 0) & 0xFF);
    if (sz >= 2) {
        th->buswr_IO8(addr + 1, 1, access, (val >> 8) & 0xFF);
    }
    if (sz == 4) {
        th->buswr_IO8(addr + 2, 1, access, (val >> 16) & 0xFF);
        th->buswr_IO8(addr + 3, 1, access, (val >> 24) & 0xFF);
    }
}

void core::trace_read(u32 addr, u8 sz, u32 val) const
{
    trace_view *tv = cpu.dbg.tvptr;
    if (!tv) return;
    tv->startline(2);
    tv->printf(0, "BUSrd");
    tv->printf(1, "%lld", clock.master_cycle_count + waitstates.current_transaction);
    tv->printf(2, "%08x", addr);
    tv->printf(3, "%08x", val);
    tv->endline();
}

void core::trace_write(u32 addr, u8 sz, u32 val) const
{
    trace_view *tv = cpu.dbg.tvptr;
    if (!tv) return;
    tv->startline(2);
    tv->printf(0, "BUSwr");
    tv->printf(1, "%lld", clock.master_cycle_count + waitstates.current_transaction);
    tv->printf(2, "%08x", addr);
    tv->printf(3, "%08x", val);
    tv->endline();
}


u32 core::mainbus_read(void *ptr, u32 addr, u8 sz, u8 access, bool has_effect)
{
    auto *th = static_cast<core *>(ptr);
    u32 v;

    if (addr < 0x10000000) v = th->mem.read[(addr >> 24) & 15](th, addr, sz, access, has_effect);
    else v = busrd_invalid(th, addr, sz, access, has_effect);
    if (::dbg.trace_on) th->trace_read(addr, sz, v);
    return v;
}

u32 core::mainbus_fetchins(void *ptr, u32 addr, u8 sz, u8 access)
{
    auto *th = static_cast<core *>(ptr);
    u32 v = mainbus_read(th, addr, sz, access, true);
    switch(sz) {
        case 4:
            th->io.cpu.open_bus_data = v;
            break;
        case 2:
            th->io.cpu.open_bus_data = (v << 16) | v;
            break;
        default:
    }
    return v;
}

void core::mainbus_write(void *ptr, u32 addr, u8 sz, u8 access, u32 val)
{
    auto *th = static_cast<core *>(ptr);
    if (addr < 0x10000000) {
        //printf("\nWRITE addr:%08x sz:%d val:%08x", addr, sz, val);
        return th->mem.write[(addr >> 24) & 15](th, addr, sz, access, val);
    }

    buswr_invalid(th, addr, sz, access, val);
}

void core::check_dma_at_hblank()
{
    // Check if any DMA channels are at enabled=1, started=0, time=hblank
    for (u32 i = 0; i < 4; i++) {
        auto &ch = dma.channel[i];
        if ((ch.io.enable) && (!ch.latch.started) && ((ch.io.start_timing == 2) || ((i == 3) && (ch.io.start_timing == 3)))) {
            if (ch.io.start_timing == 3) { // SPECIAL VIDEO MODE CH3 only goes 2-162 weirdly...
                if ((clock.ppu.y < 2) || (clock.ppu.y > 162)) continue;
            }
            else { // no hblank IRQs in vblank
                if (clock.ppu.y >= 160) continue;
            }
            //printf("\nDMA HBLANK START %d", i);
            ch.start();
        }
    }
    // And if it's channel 3 and "special", if we're in the correct lines.
}

u32 core::open_bus_byte(u32 addr) const
{
    switch(addr & 3) {
        case 0:
            return io.cpu.open_bus_data & 0xFF;
        case 1:
            return (io.cpu.open_bus_data >> 8) & 0xFF;
        case 2:
            return (io.cpu.open_bus_data >> 16) & 0xFF;
        case 3:
            return (io.cpu.open_bus_data >> 24) & 0xFF;
        default: NOGOHERE;
    }
    return 0;
}

u32 core::open_bus(const u32 addr, const u32 sz) const
{
    u32 v = open_bus_byte(addr);
    if (sz >= 2) {
        v |= open_bus_byte(addr+1) << 8;
    }
    if (sz == 4) {
        v |= open_bus_byte(addr+2) << 16;
        v |= open_bus_byte(addr+3) << 24;
    }
    return v;
}

void core::check_dma_at_vblank()
{
    // Check if any DMA channels are at enabled=1, started=0, time=hblank
    for (auto &ch : dma.channel) {
        if ((ch.io.enable) && (!ch.latch.started) && (ch.io.start_timing == 1)) {
            ch.start();
        }
    }
}

void core::process_button_IRQ()
{
    if (io.button_irq.enable) {
        u32 bits = controller.get_state();
        u32 ior = 0;
        u32 iand = 1;
        for (u32 i = 0; i < 10; i++) {
            u32 bit = 1 << i;
            if ((io.button_irq.buttons & bit) && (bits & bit)) {
                ior = 1;
            }
            else {
                iand = 0;
            }
        }
        const u32 care_about = io.button_irq.condition ? iand : ior;
        const u32 old_IF = io.IF;
        if (care_about) io.IF |= (1 << 12);
        else io.IF &= ~(1 << 12);
        //printf("\nCARE_ABOUT? %d BITS:%d BUTTONS:%d CONDITION:%d", care_about, bits, io.button_irq.buttons, io.button_irq.condition);
        if (old_IF != io.IF) {
            if (io.IF) printf("\nTRIGGERING THE INTERRUPT...");
            eval_irqs();
        }
    }
}
}
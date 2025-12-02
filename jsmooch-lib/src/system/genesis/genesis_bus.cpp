//
// Created by . on 6/1/24.
//

#include <cassert>
#include <cstdio>

#include "genesis_debugger.h"
#include "genesis_bus.h"
#include "genesis_vdp.h"

namespace genesis {

#define RETUL16or8(thing) ((UDS && LDS) ? *(u16 *)(thing) : RETUL8(thing))
#define RETUL8(thing) *(((u8 *)(thing)) + (UDS ? 1 : 0))

#define SETUL16or8(thing, tw)   if (UDS && LDS) *(u16 *)(thing) = tw; else SETUL8(thing, tw)
#define SETUL8(thing, tw) *(((u8 *)(thing)) + (UDS ? 1 : 0)) = tw

static u32 UDS_mask[4] = { 0, 0xFF, 0xFF00, 0xFFFF };
#define UDSMASK UDS_mask[((UDS) << 1) | (LDS)]

void core::test_dbg_break(const char *where)
{
    clock.mem_break++;
    if (clock.mem_break > 5) {
        dbg_break(where, clock.master_cycle_count);
        printf("\nBREAK AT CYCLE %lld", clock.master_cycle_count);
    }
}

void core::z80_reset_line(bool enabled)
{
    if ((!io.z80.reset_line) && enabled) {
        // 0->1 transition means count restart
        io.z80.reset_line_count = 0;
    }
    if ((io.z80.reset_line) && (!enabled) && (io.z80.reset_line_count >= 3)) {
        // 1->0 transition means actually reset it. only after >= 3 cycles
        z80.reset();
        ym2612.reset();
    }
    io.z80.reset_line = enabled;
    //printf("\nZ80 RESET LINE SET TO %d cyc:%lld", io.z80.reset_line, clock.master_cycle_count);
    //if (!io.z80.reset_line) dbg_break();
}

u16 core::read_version_register(u32 mask) const
{
    // bit 7 0 = japanese, 1 = other
    // bit 6 0 = NTSC, 1 = PAL
    // bit 5 0 = expansion like 32x/CD, 1 = no expansion like 32x/CD
    // bit 1-3 version, must be 0
    u32 v = 0b00100000;
    v |= ((v_opts.JP ^ 1) << 7);
    v |= PAL << 6;
    return (v << 8) | v;
}

//      A11200
// Read A10000-A1FFFF
void core::mainbus_write_a1k(u32 addr, u16 val, u16 mask)
{
    switch(addr) {
        case 0xA10002:
            io.controller_port1.write_data(val);
            return;
        case 0xA10004:
            io.controller_port2.write_data(val);
            return;
        case 0xA10006: // ext port data
            // TODO: this
            return;
        case 0xA10008:
            io.controller_port1.write_control(val);
            return;
        case 0xA1000A:
            io.controller_port2.write_control(val);
            return;
        case 0xA1000C: // ext port control
            // TODO: this
            return;
        case 0xA11100: // Z80 BUSREQ
            io.z80.bus_request = ((val >> 8) & 1);
            //if (io.z80.bus_request && io.z80.reset_line) io.z80.bus_ack = 1;
            //printf("\nZ80 BUSREQ:%d cycle:%lld", io.z80.bus_request, clock.master_cycle_count);
            return;
        case 0xA11200: // Z80 reset line
            // 1 = no reset. 0 = reset. so invert it
            z80_reset_line(((val >> 8) & 1) ^ 1);
            return;
        case 0xA14000: // VDP lock
            printf("\nVDP LOCK WRITE: %04x", val);
            return;
        case 0xA14101: // TMSS select
            printf("\nTMSS ENABLE? %d", (val & 1) ^ 1);
            return;
        default:
    }
    //gen_test_dbg_break("write_a1k");
    printf("\nWrote unknown A1K address %06x val %04x", addr, val);
}

// Write A10000-A1FFFF
u16 core::mainbus_read_a1k(u32 addr, u16 old, u16 mask, bool has_effect)
{
    switch(addr) {
        case 0xA10000: // Version register
            return read_version_register(mask);
        case 0xA10002:
            return io.controller_port1.read_data();
        case 0xA10004:
            return io.controller_port2.read_data();
        case 0xA10008:
            return io.controller_port1.read_control();
        case 0xA1000A:
            return io.controller_port2.read_control();
        case 0xA1000C:
            return 0;
        case 0xA11100: // Z80 BUSREQ
        /*
         * TT
         * bus_ack    reset   result
         *   1         1       1
         *   0         1       1
         *   0         0       1
         *   1         0       0
         */
            //
            return ((io.z80.bus_ack ^ 1) << 8) | (io.m68k.open_bus_data & 0b1111111011111111);
        default:
    }

    test_dbg_break("mainbus_read_a1k");
    printf("\nRead unknown A1K address %06x cyc:%lld", addr, clock.master_cycle_count);
    return old;
}

u16 core::mainbus_read(u32 addr, u8 UDS, u8 LDS, u16 old, bool has_effect)
{
    u32 mask = UDSMASK;
    u16 v = 0;
    if (addr < 0x400000)
        return cart.read(addr, mask, has_effect, io.SRAM_enabled);
    // $A00000	$A0FFFF, audio RAM
    if ((addr >= 0xA00000) && (addr < 0xA10000)) {
        if (UDS) {
            v = z80_bus_read(addr & 0x7FFE, old >> 8, has_effect) << 8;
            if (LDS) v |= (v >> 8);
        }
        else
            v = z80_bus_read((addr & 0x7FFE) | 1, old & 0xFF, has_effect);
        return v;
    }
    if ((addr >= 0xA10000) && (addr < 0xA12000)) {
        return mainbus_read_a1k(addr, old, mask, has_effect);
    }
    if ((addr >= 0xC00000) && (addr < 0xFF0000)) {
        return vdp.mainbus_read(addr, old, mask, has_effect);
    }
    if (addr >= 0xFF0000)
        return RAM[(addr & 0xFFFF)>>1] & mask;

    //printf("\nWARNING BAD MAIN READ AT %06x %d%d cycle:%lld", addr, UDS, LDS, clock.master_cycle_count);
    //gen_test_dbg_break();
    return old;
}

void core::mainbus_write(u32 addr, u8 UDS, u8 LDS, u16 val) {
    u32 mask = UDSMASK;
    if (addr < 0x400000) {
        cart.write(addr, mask, val, io.SRAM_enabled);
        return;
    } //     // A06000

    if ((addr >= 0xA00000) && (addr < 0xA10000)) {
        if (UDS) z80_bus_write(addr & 0x7FFE, val >> 8, true);
        else z80_bus_write((addr & 0x7FFE) | 1, val & 0xFF, true);
        return;
    }

    if ((addr >= 0xA10000) && (addr < 0xA12000)) {
        mainbus_write_a1k(addr, val, mask);
        return;
    }
    if ((addr == 0xA130F0) && (cart.ROM.size > 0x200000)) {
        io.SRAM_enabled = val & 1;
    }

    if (cart.kind == sc_ssf) {
        switch(addr) {
            case 0xA130F2:
                cart.bank_offset[1] = (val & 0xFF) << 19;
                break;
            case 0xA130F4:
                cart.bank_offset[2] = (val & 0xFF) << 19;
                break;
            case 0xA130F6:
                cart.bank_offset[3] = (val & 0xFF) << 19;
                break;
            case 0xA130F8:
                cart.bank_offset[4] = (val & 0xFF) << 19;
                break;
            case 0xA130FA:
                cart.bank_offset[5] = (val & 0xFF) << 19;
                break;
            case 0xA130FC:
                cart.bank_offset[6] = (val & 0xFF) << 19;
                break;
            case 0xA130FE:
                cart.bank_offset[7] = (val & 0xFF) << 19;
                break;
            case 0xA130F1:
            case 0xA130F3:
            case 0xA130F5:
            case 0xA130F7:
            case 0xA130F9:
            case 0xA130FB:
            case 0xA130FD:
                printf("\nWARN ACCESS SSF2 REGS ODD!");
                break;
            default:
        }
    }
    if ((addr >= 0xC00000) && (addr < 0xFF0000)) {
        vdp.mainbus_write(addr, val, mask);
        return;
    }
    if (addr >= 0xFF0000) {
        addr = (addr & 0xFFFF) >> 1;
        RAM[addr] = (RAM[addr] & ~mask) | (val & mask);
        return;
    }
    printf("\nWARNING BAD MAIN WRITE1 AT %06x: %04x %d%d cycle:%lld", addr, val, UDS, LDS, clock.master_cycle_count);
    test_dbg_break("mainbus_write");
}

void core::z80_mainbus_write(u32 addr, u8 val)
{
    printf("\nZ80 attempted write to mainbus at %06x on cycle %lld BAR:%08x", addr, clock.master_cycle_count, io.z80.bank_address_register);
}

u8 core::z80_mainbus_read(u32 addr, u8 old, bool has_effect)
{
    addr = (addr & 0x7FFF) | (io.z80.bank_address_register << 15);
    u8 data = addr & 1 ? mainbus_read(addr & ~1, 0, 1, 0, has_effect) & 0xFF : mainbus_read(addr & ~1, 1, 0, 0, has_effect) >> 8;
    // was TRUE

    //return genesis_mainbus_read(((u32)(addr & 0xFFFE) | io.z80.bank_address_register), (addr & 1) ^ 1, addr & 1, 0, 1) >> (((addr & 1) ^ 1) * 8);
    return data;
}

u8 core::z80_ym2612_read(u32 addr, u8 old, bool has_effect) {
    return ym2612.read(addr & 3, old, has_effect);
}

u8 core::z80_bus_read(u16 addr, u8 old, bool has_effect)
{
    if (addr < 0x2000)
        return ARAM[addr];
    if (addr < 0x4000) // Reserved
        return old;
    if (addr < 0x6000) return z80_ym2612_read(addr, old, has_effect);
    if (addr < 0x60FF) return 0xFF; // bank address register reads return 0xFF
    if ((addr >= 0x7000) && (addr <= 0x7FFF)) return vdp.z80_read(addr, old, has_effect);
    if (addr >= 0x8000) {
        return z80_mainbus_read(addr, old, has_effect);
    }
    printf("\nUnhandled Z80 read to %04x", addr);
    return old;
}


void core::write_z80_bank_address_register(u32 val)
{
    //state.bank = data.bit(0) << 8 | state.bank >> 1
    io.z80.bank_address_register = io.z80.bank_address_register >> 1 | ((val & 1) << 8);
    //u32 v = (io.z80.bank_address_register >> 1) & 0x7F0000;
    //io.z80.bank_address_register = v | ((val & 1) << 23);
}

void core::z80_bus_write(u16 addr, u8 val, bool is_m68k)
{
    if (addr < 0x2000) {
        ARAM[addr] = val;
        return;
    }
    if (addr < 0x4000) // Reserved
        return;
    if (addr < 0x6000) {
        ym2612.write(addr & 3, val);
        return;
    }
    if ((addr < 0x60FF) && (!is_m68k)) {
        write_z80_bank_address_register(val);
        return;
    }
    if ((addr >= 0x7000) && (addr <= 0x7FFF)) {
        vdp.z80_write(addr, val);
        return;
    }
    if (addr >= 0x8000) {
        return z80_mainbus_write(addr, val);
    }

}

SYMDO *core::get_at_addr(u32 addr)
{
    for (u32 i = 0; i < debugging.num_symbols; i++) {
        if (debugging.symbols[i].addr == addr) return &debugging.symbols[i];
    }
    return nullptr;
}

void core::cycle_m68k()
{
    static u32 PCO = 0;
    timing.m68k_cycles++;
    if (io.m68k.stuck) dbg_printf("\nSTUCK cyc %lld", *m68k.trace.cycles);
    if (vdp.io.bus_locked) return;

    m68k.cycle();
    if (m68k.pins.FC == 7) {
        // Auto-vector interrupts!
        m68k.pins.VPA = 1;
        return;
    }
    if (m68k.pins.AS && (!m68k.pins.DTACK) && (!io.m68k.stuck)) {
        if (!m68k.pins.RW) { // read
            io.m68k.open_bus_data = m68k.pins.D = mainbus_read(m68k.pins.Addr, m68k.pins.UDS, m68k.pins.LDS, io.m68k.open_bus_data, 1);
#ifdef TRACE_SONIC1
            if (PCO != m68k.regs.PC) {
                struct SYMDO *sd = get_at_addr(m68k.regs.PC);
                if (sd) {
                    printf("\n%06x: func %s    line:%d cyc:%lld", m68k.regs.PC, sd->name, clock.vdp.vcount, clock.master_cycle_count);
                }
            }
#endif
            if (::dbg.traces.cpu3) DFT("\nRD %06x(%d%d) %04x", m68k.pins.Addr, m68k.pins.UDS, m68k.pins.LDS, m68k.pins.D);

            m68k.pins.DTACK = !(io.m68k.VDP_FIFO_stall | io.m68k.VDP_prefetch_stall);
            //io.m68k.stuck = !m68k.pins.DTACK;
            if (::dbg.trace_on && ::dbg.traces.m68000.mem && ((m68k.pins.FC & 1) || ::dbg.traces.m68000.ifetch)) {
                dbg_printf(DBGC_READ "\nr.M68k(%lld)   %06x  v:%04x" DBGC_RST, clock.master_cycle_count, m68k.pins.Addr, m68k.pins.D);
            }
        }
        else { // write
            mainbus_write(m68k.pins.Addr, m68k.pins.UDS, m68k.pins.LDS, m68k.pins.D);
            if (::dbg.traces.cpu3) DFT("\nWR %06x(%d%d) %04x", m68k.pins.Addr, m68k.pins.UDS, m68k.pins.LDS, m68k.pins.D);
            m68k.pins.DTACK = !(io.m68k.VDP_FIFO_stall | io.m68k.VDP_prefetch_stall);
            //io.m68k.stuck = !m68k.pins.DTACK;
            if (::dbg.trace_on && ::dbg.traces.m68000.mem && (m68k.pins.FC & 1)) {
                dbg_printf(DBGC_WRITE "\nw.M68k(%lld)   %06x  v:%04x" DBGC_RST, clock.master_cycle_count, m68k.pins.Addr, m68k.pins.D);
            }
        }
    }
    assert(io.m68k.stuck == 0);
}

void core::update_irqs()
{
    u32 old_IPL = m68k.pins.IPL;

    u32 lvl = 0;
    if (vdp.irq.hblank.pending && vdp.irq.hblank.enable) lvl = 4;
    if (vdp.irq.vblank.pending && vdp.irq.vblank.enable) lvl = 6;
    if (lvl != old_IPL) {
        if (lvl == 4) {
            DBG_EVENT(DBG_GEN_EVENT_HBLANK_IRQ);
        }
        if (lvl == 6) {
            DBG_EVENT(DBG_GEN_EVENT_VBLANK_IRQ);
        }
        m68k.set_interrupt_level(lvl);
    }
}

void core::z80_interrupt(bool level)
{
    //if ((z80.pins.IRQ == 0) && level) printf("\nZ80 IRQ PIN SET!");
    /*if (z80.IRQ_pending != level) {
        printf("\nZ80 IRQ TO %d and EI:%d IFF1:%d IFF2:%d", level, z80.regs.EI, z80.regs.IFF1, z80.regs.IFF2);
    }*/
    //z80.pins.IRQ = level;
    z80.notify_IRQ(level);
}

void core::cycle_z80()
{
    timing.z80_cycles++;
    if (io.z80.reset_line) {
        io.z80.reset_line_count++;
        if (io.z80.reset_line_count >= 3) return; // If it's held down 3 or more, freeze!
    }
    io.z80.reset_line_count = 0;
    if (io.z80.bus_request && io.z80.bus_ack) {
        return;
    }

    z80.cycle();
    if (z80.pins.RD) {
        if (z80.pins.MRQ) {
            z80.pins.D = z80_bus_read(z80.pins.Addr, z80.pins.D, 1);
            if (::dbg.trace_on && ::dbg.traces.z80.mem) {
                dbg_printf(DBGC_READ "\nr.Z80 (%lld)   %06x  v:%02x" DBGC_RST, clock.master_cycle_count, z80.pins.Addr, z80.pins.D);
            }
        }
        else if (z80.pins.IO && (z80.pins.M1 == 0)) {
            // All Z80 IO requests return 0xFF
            z80.pins.D = 0xFF;
        }
    }
    else if (z80.pins.WR) {
        if (z80.pins.MRQ) {
            // All Z80 IO requests are ignored
            z80_bus_write(z80.pins.Addr, z80.pins.D, false);
            if (::dbg.trace_on && ::dbg.traces.z80.mem) {
                dbg_printf(DBGC_WRITE "\nw.Z80 (%lld)   %06x  v:%04x" DBGC_RST, clock.master_cycle_count, z80.pins.Addr, z80.pins.D);
            }
        }
    }
    // Bus request/ack at end of cycle
    io.z80.bus_ack = io.z80.bus_request;
}

}
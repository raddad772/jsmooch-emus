//
// Created by . on 6/1/24.
//

#include <assert.h>
#include <stdio.h>

#include "genesis_debugger.h"
#include "genesis_bus.h"
#include "genesis_vdp.h"

#define RETUL16or8(thing) ((UDS && LDS) ? *(u16 *)(thing) : RETUL8(thing))
#define RETUL8(thing) *(((u8 *)(thing)) + (UDS ? 1 : 0))

#define SETUL16or8(thing, tw)   if (UDS && LDS) *(u16 *)(thing) = tw; else SETUL8(thing, tw)
#define SETUL8(thing, tw) *(((u8 *)(thing)) + (UDS ? 1 : 0)) = tw

static u32 UDS_mask[4] = { 0, 0xFF, 0xFF00, 0xFFFF };
#define UDSMASK UDS_mask[((UDS) << 1) | (LDS)]

static void genesis_z80_bus_write(struct genesis* this, u16 addr, u8 val, u32 is_m68k);


void gen_test_dbg_break(struct genesis* this, const char *where)
{
    this->clock.mem_break++;
    if (this->clock.mem_break > 5) {
        dbg_break(where, this->clock.master_cycle_count);
        printf("\nBREAK AT CYCLE %lld", this->clock.master_cycle_count);
    }
}

void genesis_z80_reset_line(struct genesis* this, u32 enabled)
{
    if ((!this->io.z80.reset_line) && enabled) {
        // 0->1 transition means count restart
        this->io.z80.reset_line_count = 0;
    }
    if ((this->io.z80.reset_line) && (!enabled) && (this->io.z80.reset_line_count >= 3)) {
        // 1->0 transition means actually reset it. only after >= 3 cycles
        Z80_reset(&this->z80);
        ym2612_reset(&this->ym2612);
    }
    this->io.z80.reset_line = enabled;
    //printf("\nZ80 RESET LINE SET TO %d cyc:%lld", this->io.z80.reset_line, this->clock.master_cycle_count);
    //if (!this->io.z80.reset_line) dbg_break();
}

static u16 read_version_register(struct genesis* this, u32 mask)
{
    // bit 7 0 = japanese, 1 = other
    // bit 6 0 = NTSC, 1 = PAL
    // bit 5 0 = expansion like 32x/CD, 1 = no expansion like 32x/CD
    // bit 1-3 version, must be 0
    u32 v = 0b00100000;
    v |= ((this->opts.JP ^ 1) << 7);
    v |= this->PAL << 6;
    return (v << 8) | v;
}

//      A11200
// Read A10000-A1FFFF
void genesis_mainbus_write_a1k(struct genesis* this, u32 addr, u16 val, u16 mask)
{
    switch(addr) {
        case 0xA10002:
            genesis_controllerport_write_data(&this->io.controller_port1, val);
            return;
        case 0xA10004:
            genesis_controllerport_write_data(&this->io.controller_port2, val);
            return;
        case 0xA10006: // ext port data
            // TODO: this
            return;
        case 0xA10008:
            genesis_controllerport_write_control(&this->io.controller_port1, val);
            return;
        case 0xA1000A:
            genesis_controllerport_write_control(&this->io.controller_port2, val);
            return;
        case 0xA1000C: // ext port control
            // TODO: this
            return;
        case 0xA11100: // Z80 BUSREQ
            this->io.z80.bus_request = ((val >> 8) & 1);
            //if (this->io.z80.bus_request && this->io.z80.reset_line) this->io.z80.bus_ack = 1;
            //printf("\nZ80 BUSREQ:%d cycle:%lld", this->io.z80.bus_request, this->clock.master_cycle_count);
            return;
        case 0xA11200: // Z80 reset line
            // 1 = no reset. 0 = reset. so invert it
            genesis_z80_reset_line(this, ((val >> 8) & 1) ^ 1);
            return;
        case 0xA14000: // VDP lock
            printf("\nVDP LOCK WRITE: %04x", val);
            return;
        case 0xA14101: // TMSS select
            printf("\nTMSS ENABLE? %d", (val & 1) ^ 1);
            return;
    }
    //gen_test_dbg_break(this, "write_a1k");
    printf("\nWrote unknown A1K address %06x val %04x", addr, val);
}

// Write A10000-A1FFFF
u16 genesis_mainbus_read_a1k(struct genesis* this, u32 addr, u16 old, u16 mask, u32 has_effect)
{
    switch(addr) {
        case 0xA10000: // Version register
            return read_version_register(this, mask);
        case 0xA10002: {
            u16 a = genesis_controllerport_read_data(&this->io.controller_port1);
            return a;
        }
        case 0xA10004:
            return genesis_controllerport_read_data(&this->io.controller_port2);
        case 0xA10008:
            return genesis_controllerport_read_control(&this->io.controller_port1);
        case 0xA1000A:
            return genesis_controllerport_read_control(&this->io.controller_port2);
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
        return ((this->io.z80.bus_ack ^ 1) << 8) | (this->io.m68k.open_bus_data & 0b1111111011111111);
    }

    gen_test_dbg_break(this, "mainbus_read_a1k");
    printf("\nRead unknown A1K address %06x cyc:%lld", addr, this->clock.master_cycle_count);
    return old;
}

u16 genesis_mainbus_read(struct genesis* this, u32 addr, u32 UDS, u32 LDS, u16 old, u32 has_effect)
{
    u32 mask = UDSMASK;
    u16 v = 0;
    if (addr < 0x400000)
        return genesis_cart_read(&this->cart, addr, mask, has_effect, this->io.SRAM_enabled);
    // $A00000	$A0FFFF, audio RAM
    if ((addr >= 0xA00000) && (addr < 0xA10000)) {
        if (UDS) {
            v = genesis_z80_bus_read(this, addr & 0x7FFE, old >> 8, has_effect) << 8;
            if (LDS) v |= (v >> 8);
        }
        else
            v = genesis_z80_bus_read(this, (addr & 0x7FFE) | 1, old & 0xFF, has_effect);
        return v;
    }
    if ((addr >= 0xA10000) && (addr < 0xA12000)) {
        return genesis_mainbus_read_a1k(this, addr, old, mask, has_effect);
    }
    if ((addr >= 0xC00000) && (addr < 0xFF0000)) {
        return genesis_VDP_mainbus_read(this, addr, old, mask, has_effect);
    }
    if (addr >= 0xFF0000)
        return this->RAM[(addr & 0xFFFF)>>1] & mask;

    //printf("\nWARNING BAD MAIN READ AT %06x %d%d cycle:%lld", addr, UDS, LDS, this->clock.master_cycle_count);
    //gen_test_dbg_break(this);
    return old;
}

void genesis_mainbus_write(struct genesis* this, u32 addr, u32 UDS, u32 LDS, u16 val) {
    u32 mask = UDSMASK;
    if (addr < 0x400000) {
        genesis_cart_write(&this->cart, addr, mask, val, this->io.SRAM_enabled);
        return;
    } //     // A06000

    if ((addr >= 0xA00000) && (addr < 0xA10000)) {
        if (UDS) genesis_z80_bus_write(this, addr & 0x7FFE, val >> 8, 1);
        else genesis_z80_bus_write(this, (addr & 0x7FFE) | 1, val & 0xFF, 1);
        return;
    }

    if ((addr >= 0xA10000) && (addr < 0xA12000)) {
        genesis_mainbus_write_a1k(this, addr, val, mask);
        return;
    }
    if ((addr == 0xA130F0) && (this->cart.ROM.size > 0x200000)) {
        this->io.SRAM_enabled = val & 1;
    }

    if (this->cart.kind == sega_cart_ssf) {
        switch(addr) {
            case 0xA130F2:
                this->cart.bank_offset[1] = (val & 0xFF) << 19;
                break;
            case 0xA130F4:
                this->cart.bank_offset[2] = (val & 0xFF) << 19;
                break;
            case 0xA130F6:
                this->cart.bank_offset[3] = (val & 0xFF) << 19;
                break;
            case 0xA130F8:
                this->cart.bank_offset[4] = (val & 0xFF) << 19;
                break;
            case 0xA130FA:
                this->cart.bank_offset[5] = (val & 0xFF) << 19;
                break;
            case 0xA130FC:
                this->cart.bank_offset[6] = (val & 0xFF) << 19;
                break;
            case 0xA130FE:
                this->cart.bank_offset[7] = (val & 0xFF) << 19;
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
        }
    }
    if ((addr >= 0xC00000) && (addr < 0xFF0000)) {
        genesis_VDP_mainbus_write(this, addr, val, mask);
        return;
    }
    if (addr >= 0xFF0000) {
        addr = (addr & 0xFFFF) >> 1;
        this->RAM[addr] = (this->RAM[addr] & ~mask) | (val & mask);
        return;
    }
    printf("\nWARNING BAD MAIN WRITE1 AT %06x: %04x %d%d cycle:%lld", addr, val, UDS, LDS, this->clock.master_cycle_count);
    gen_test_dbg_break(this, "mainbus_write");
}

static void genesis_z80_mainbus_write(struct genesis* this, u32 addr, u8 val)
{
    printf("\nZ80 attempted write to mainbus at %06x on cycle %lld BAR:%08x", addr, this->clock.master_cycle_count, this->io.z80.bank_address_register);
}

static u8 genesis_z80_mainbus_read(struct genesis* this, u32 addr, u8 old, u32 has_effect)
{
    addr = (addr & 0x7FFF) | (this->io.z80.bank_address_register << 15);
    u8 data = 0xFF;
    if(addr & 1) {
        data = genesis_mainbus_read(this, addr & ~1, 0, 1, 0, 1) & 0xFF;
    } else {
        data = genesis_mainbus_read(this, addr & ~1, 1, 0, 0, 1) >> 8;
    }

    //return genesis_mainbus_read(this, ((u32)(addr & 0xFFFE) | this->io.z80.bank_address_register), (addr & 1) ^ 1, addr & 1, 0, 1) >> (((addr & 1) ^ 1) * 8);
    return data;
}

static u8 genesis_z80_ym2612_read(struct genesis* this, u32 addr, u8 old, u32 has_effect) {
    return ym2612_read(&this->ym2612, addr & 3, old, has_effect);
}

u8 genesis_z80_bus_read(struct genesis* this, u16 addr, u8 old, u32 has_effect)
{
    if (addr < 0x2000)
        return this->ARAM[addr];
    if (addr < 0x4000) // Reserved
        return old;
    if (addr < 0x6000) return genesis_z80_ym2612_read(this, addr, old, has_effect);
    if (addr < 0x60FF) return 0xFF; // bank address register reads return 0xFF
    if ((addr >= 0x7000) && (addr <= 0x7FFF)) return genesis_VDP_z80_read(this, addr, old, has_effect);
    if (addr >= 0x8000) {
        return genesis_z80_mainbus_read(this, addr, old, has_effect);
    }
    printf("\nUnhandled Z80 read to %04x", addr);
    return old;
}


static void write_z80_bank_address_register(struct genesis* this, u32 val)
{
    //state.bank = data.bit(0) << 8 | state.bank >> 1
    this->io.z80.bank_address_register = this->io.z80.bank_address_register >> 1 | ((val & 1) << 8);
    //u32 v = (this->io.z80.bank_address_register >> 1) & 0x7F0000;
    //this->io.z80.bank_address_register = v | ((val & 1) << 23);
}

static void genesis_z80_bus_write(struct genesis* this, u16 addr, u8 val, u32 is_m68k)
{
    if (addr < 0x2000) {
        this->ARAM[addr] = val;
        return;
    }
    if (addr < 0x4000) // Reserved
        return;
    if (addr < 0x6000) {
        ym2612_write(&this->ym2612, addr & 3, val);
        return;
    }
    if ((addr < 0x60FF) && (!is_m68k)) {
        write_z80_bank_address_register(this, val);
        return;
    }
    if ((addr >= 0x7000) && (addr <= 0x7FFF)) {
        genesis_VDP_z80_write(this, addr, val);
        return;
    }
    if (addr >= 0x8000) {
        return genesis_z80_mainbus_write(this, addr, val);
    }

}

static struct SYMDO *get_at_addr(struct genesis* this, u32 addr)
{
    for (u32 i = 0; i < this->debugging.num_symbols; i++) {
        if (this->debugging.symbols[i].addr == addr) return &this->debugging.symbols[i];
    }
    return NULL;
}

void genesis_cycle_m68k(struct genesis* this)
{
    static u32 PCO = 0;
    this->timing.m68k_cycles++;
    if (this->io.m68k.stuck) dbg_printf("\nSTUCK cyc %lld", *this->m68k.trace.cycles);
    if (this->vdp.io.bus_locked) return;

    M68k_cycle(&this->m68k);
    if (this->m68k.pins.FC == 7) {
        // Auto-vector interrupts!
        this->m68k.pins.VPA = 1;
        return;
    }
    if (this->m68k.pins.AS && (!this->m68k.pins.DTACK) && (!this->io.m68k.stuck)) {
        if (!this->m68k.pins.RW) { // read
            this->io.m68k.open_bus_data = this->m68k.pins.D = genesis_mainbus_read(this, this->m68k.pins.Addr, this->m68k.pins.UDS, this->m68k.pins.LDS, this->io.m68k.open_bus_data, 1);
#ifdef TRACE_SONIC1
            if (PCO != this->m68k.regs.PC) {
                struct SYMDO *sd = get_at_addr(this, this->m68k.regs.PC);
                if (sd) {
                    printf("\n%06x: func %s    line:%d cyc:%lld", this->m68k.regs.PC, sd->name, this->clock.vdp.vcount, this->clock.master_cycle_count);
                }
            }
#endif
            if (dbg.traces.cpu3) DFT("\nRD %06x(%d%d) %04x", this->m68k.pins.Addr, this->m68k.pins.UDS, this->m68k.pins.LDS, this->m68k.pins.D);

            this->m68k.pins.DTACK = !(this->io.m68k.VDP_FIFO_stall | this->io.m68k.VDP_prefetch_stall);
            //this->io.m68k.stuck = !this->m68k.pins.DTACK;
            if (dbg.trace_on && dbg.traces.m68000.mem && ((this->m68k.pins.FC & 1) || dbg.traces.m68000.ifetch)) {
                dbg_printf(DBGC_READ "\nr.M68k(%lld)   %06x  v:%04x" DBGC_RST, this->clock.master_cycle_count, this->m68k.pins.Addr, this->m68k.pins.D);
            }
        }
        else { // write
            genesis_mainbus_write(this, this->m68k.pins.Addr, this->m68k.pins.UDS, this->m68k.pins.LDS, this->m68k.pins.D);
            if (dbg.traces.cpu3) DFT("\nWR %06x(%d%d) %04x", this->m68k.pins.Addr, this->m68k.pins.UDS, this->m68k.pins.LDS, this->m68k.pins.D);
            this->m68k.pins.DTACK = !(this->io.m68k.VDP_FIFO_stall | this->io.m68k.VDP_prefetch_stall);
            //this->io.m68k.stuck = !this->m68k.pins.DTACK;
            if (dbg.trace_on && dbg.traces.m68000.mem && (this->m68k.pins.FC & 1)) {
                dbg_printf(DBGC_WRITE "\nw.M68k(%lld)   %06x  v:%04x" DBGC_RST, this->clock.master_cycle_count, this->m68k.pins.Addr, this->m68k.pins.D);
            }
        }
    }
    assert(this->io.m68k.stuck == 0);
}

void genesis_bus_update_irqs(struct genesis* this)
{
    u32 old_IPL = this->m68k.pins.IPL;

    u32 lvl = 0;
    if (this->vdp.irq.hblank.pending && this->vdp.irq.hblank.enable) lvl = 4;
    if (this->vdp.irq.vblank.pending && this->vdp.irq.vblank.enable) lvl = 6;
    if (lvl != old_IPL) {
        if (lvl == 4) {
            DBG_EVENT(DBG_GEN_EVENT_HBLANK_IRQ);
        }
        if (lvl == 6) {
            DBG_EVENT(DBG_GEN_EVENT_VBLANK_IRQ);
        }
        M68k_set_interrupt_level(&this->m68k, lvl);
    }
}

void genesis_z80_interrupt(struct genesis* this, u32 level)
{
    //if ((this->z80.pins.IRQ == 0) && level) printf("\nZ80 IRQ PIN SET!");
    /*if (this->z80.IRQ_pending != level) {
        printf("\nZ80 IRQ TO %d and EI:%d IFF1:%d IFF2:%d", level, this->z80.regs.EI, this->z80.regs.IFF1, this->z80.regs.IFF2);
    }*/
    //this->z80.pins.IRQ = level;
    Z80_notify_IRQ(&this->z80, level);
}

void genesis_cycle_z80(struct genesis* this)
{
    this->timing.z80_cycles++;
    if (this->io.z80.reset_line) {
        this->io.z80.reset_line_count++;
        if (this->io.z80.reset_line_count >= 3) return; // If it's held down 3 or more, freeze!
    }
    this->io.z80.reset_line_count = 0;
    if (this->io.z80.bus_request && this->io.z80.bus_ack) {
        return;
    }

    Z80_cycle(&this->z80);
    if (this->z80.pins.RD) {
        if (this->z80.pins.MRQ) {
            this->z80.pins.D = genesis_z80_bus_read(this, this->z80.pins.Addr, this->z80.pins.D, 1);
            if (dbg.trace_on && dbg.traces.z80.mem) {
                dbg_printf(DBGC_READ "\nr.Z80 (%lld)   %06x  v:%02x" DBGC_RST, this->clock.master_cycle_count, this->z80.pins.Addr, this->z80.pins.D);
            }
        }
        else if (this->z80.pins.IO && (this->z80.pins.M1 == 0)) {
            // All Z80 IO requests return 0xFF
            this->z80.pins.D = 0xFF;
        }
    }
    else if (this->z80.pins.WR) {
        if (this->z80.pins.MRQ) {
            // All Z80 IO requests are ignored
            genesis_z80_bus_write(this, this->z80.pins.Addr, this->z80.pins.D, 0);
            if (dbg.trace_on && dbg.traces.z80.mem) {
                dbg_printf(DBGC_WRITE "\nw.Z80 (%lld)   %06x  v:%04x" DBGC_RST, this->clock.master_cycle_count, this->z80.pins.Addr, this->z80.pins.D);
            }
        }
    }
    // Bus request/ack at end of cycle
    this->io.z80.bus_ack = this->io.z80.bus_request;
}

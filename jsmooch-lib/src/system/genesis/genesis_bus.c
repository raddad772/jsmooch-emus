//
// Created by . on 6/1/24.
//

#include <assert.h>
#include <stdio.h>

#include "genesis_bus.h"
#include "genesis_vdp.h"

#define RETUL16or8(thing) ((UDS && LDS) ? *(u16 *)(thing) : RETUL8(thing))
#define RETUL8(thing) *(((u8 *)(thing)) + (UDS ? 1 : 0))

#define SETUL16or8(thing, tw)   if (UDS && LDS) *(u16 *)(thing) = tw; else SETUL8(thing, tw)
#define SETUL8(thing, tw) *(((u8 *)(thing)) + (UDS ? 1 : 0)) = tw

static u32 UDS_mask[4] = { 0, 0xFF, 0xFF00, 0xFFFF };
#define UDSMASK UDS_mask[((UDS) << 1) | (LDS)]

static void genesis_z80_bus_write(struct genesis* this, u16 addr, u8 val);


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
        // 1->0 transition means reset it
        Z80_reset(&this->z80);
        ym2612_reset(&this->ym2612);
    }
    this->io.z80.reset_line = enabled;
    printf("\nZ80 RESET LINE SET TO %d cyc:%lld", this->io.z80.reset_line, this->clock.master_cycle_count);
    //if (!this->io.z80.reset_line) dbg_break();
}

static u16 read_version_register(struct genesis* this, u32 mask)
{
    // bit 7 0 = japanese, 1 = other
    // bit 6 0 = NTSC, 1 = PAL
    // bit 5 0 = no expansion like 32x, 1 = expansion like 32x
    // bit 1-3 version, must be 0
    u32 v = 0b10000000;
    return ((v << 8) | v) & mask;
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
            if (this->io.z80.bus_request) if (this->io.z80.reset_line) this->io.z80.bus_ack = 1;
            printf("\nZ80 BUSREQ:%d cycle:%lld", this->io.z80.bus_request, this->clock.master_cycle_count);
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
    gen_test_dbg_break(this, "write_a1k");
    printf("\nWrote unknown A1K address %06x val %04x", addr, val);
}

// Write A10000-A1FFFF
u16 genesis_mainbus_read_a1k(struct genesis* this, u32 addr, u16 old, u16 mask, u32 has_effect)
{
    switch(addr) {
        case 0xA10000: // Version register
            return read_version_register(this, mask);
        case 0xA10002:
            return genesis_controllerport_read_data(&this->io.controller_port1);
        case 0xA10004:
            return genesis_controllerport_read_data(&this->io.controller_port2);
        case 0xA10008:
            return genesis_controllerport_read_control(&this->io.controller_port1);
        case 0xA1000A:
            return genesis_controllerport_read_control(&this->io.controller_port2);
        case 0xA1000C:
            return old; // TODO: this
        case 0xA11100: // Z80 BUSREQ
        /*
         * TT
         * bus_ack    reset   result
         *   1         1       1
         *   0         1       1
         *   0         0       1
         *   1         0       0
         */
            return (!(this->io.z80.bus_ack && !this->io.z80.reset_line)) << 8;
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
        return genesis_cart_read(&this->cart, addr, mask, has_effect);
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

void genesis_mainbus_write(struct genesis* this, u32 addr, u32 UDS, u32 LDS, u16 val)
{
    u32 mask = UDSMASK;
    if (addr < 0x400000) {
        gen_test_dbg_break(this, "mainbus_write cart");
        dbg_printf("\nWARNING ATTEMPTED WRITE TO CART AT %06x cycle:%lld", addr, this->clock.master_cycle_count);
        return;
    } //     // A06000

    if ((addr >= 0xA00000) && (addr < 0xA10000)) {
        if (UDS) genesis_z80_bus_write(this, addr & 0x7FFE, val >> 8);
        else genesis_z80_bus_write(this, (addr & 0x7FFE) | 1, val & 0xFF);
        return;
    }

    if ((addr >= 0xA10000) && (addr < 0xA12000)) {
        genesis_mainbus_write_a1k(this, addr, val, mask);
        return;
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
    return genesis_mainbus_read(this, ((u32)(addr & 0xFFFE) | this->io.z80.bank_address_register), (addr & 1) ^ 1, addr & 1, 0, 1) >> (((addr & 1) ^ 1) * 8);
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


static void genesis_z80_ym2612_write(struct genesis* this, u32 addr, u32 val)
{
    ym2612_write(&this->ym2612, addr & 3, val);
}

static void write_z80_bank_address_register(struct genesis* this, u32 val)
{
    u32 v = (this->io.z80.bank_address_register << 1) & 0xFF0000;
    this->io.z80.bank_address_register = v | ((val & 1) << 15);
}

static void genesis_z80_bus_write(struct genesis* this, u16 addr, u8 val)
{
    if (addr < 0x2000) {
        this->ARAM[addr] = val;
        return;
    }
    if (addr < 0x4000) // Reserved
        return;
    if (addr < 0x6000) {
        genesis_z80_ym2612_write(this, addr, val);
        return;
    }
    if (addr < 0x60FF) {
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

void genesis_cycle_m68k(struct genesis* this)
{
    if (this->io.m68k.stuck) dbg_printf("\nSTUCK cyc %lld", *this->m68k.trace.cycles);

    M68k_cycle(&this->m68k);
    if (this->m68k.pins.FC == 7) {
        // Auto-vector interrupts!
        this->m68k.pins.VPA = 1;
        return;
    }
    if (this->m68k.pins.AS && (!this->m68k.pins.DTACK) && (!this->io.m68k.stuck)) {
        if (!this->m68k.pins.RW) { // read
            this->io.m68k.open_bus_data = this->m68k.pins.D = genesis_mainbus_read(this, this->m68k.pins.Addr, this->m68k.pins.UDS, this->m68k.pins.LDS, this->io.m68k.open_bus_data, 1);
            this->m68k.pins.DTACK = !(this->io.m68k.VDP_FIFO_stall | this->io.m68k.VDP_prefetch_stall);
            this->io.m68k.stuck = !this->m68k.pins.DTACK;
            if (dbg.trace_on && dbg.traces.m68000.mem && ((this->m68k.pins.FC & 1) || dbg.traces.m68000.ifetch)) {
                dbg_printf(DBGC_READ "\nr.M68k(%lld)   %06x  v:%04x" DBGC_RST, this->clock.master_cycle_count, this->m68k.pins.Addr, this->m68k.pins.D);
            }
        }
        else { // write
            genesis_mainbus_write(this, this->m68k.pins.Addr, this->m68k.pins.UDS, this->m68k.pins.LDS, this->m68k.pins.D);
            this->m68k.pins.DTACK = !(this->io.m68k.VDP_FIFO_stall | this->io.m68k.VDP_prefetch_stall);
            this->io.m68k.stuck = !this->m68k.pins.DTACK;
            if (dbg.trace_on && dbg.traces.m68000.mem && (this->m68k.pins.FC & 1)) {
                dbg_printf(DBGC_WRITE "\nw.M68k(%lld)   %06x  v:%04x" DBGC_RST, this->clock.master_cycle_count, this->m68k.pins.Addr, this->m68k.pins.D);
            }
        }
    }
    assert(this->io.m68k.stuck == 0);
}

void genesis_m68k_line_count_irq(struct genesis* this, u32 level)
{
    // TODO: multiplex/priority encode these
    if (level) printf("\nM68k line count irq! cycle:%lld line:%d", this->clock.master_cycle_count, this->clock.vdp.vcount);
    if ((this->m68k.pins.IPL == 4) || (this->m68k.pins.IPL == 0))
        M68k_set_interrupt_level(&this->m68k, 4 * level);
}

void genesis_m68k_vblank_irq(struct genesis* this, u32 level)
{
    // TODO: multiplex/priority encode these
    //if (level) printf("\nM68K vblank irq! cycle:%lld", this->clock.master_cycle_count);
    if ((this->m68k.pins.IPL == 6) || (this->m68k.pins.IPL == 0))
        M68k_set_interrupt_level(&this->m68k, 6 * level);
}

void genesis_z80_interrupt(struct genesis* this, u32 level)
{
    //if ((this->z80.pins.IRQ == 0) && level) printf("\nZ80 IRQ PIN SET!");
    this->z80.pins.IRQ = level;
}

void genesis_cycle_z80(struct genesis* this)
{
    if (this->io.z80.reset_line) {
        this->io.z80.reset_line_count++;
        if (this->io.z80.reset_line_count >= 3) return; // If it's held down 3 or more, freeze
    }
    this->io.z80.reset_line_count = 0;
    if (this->io.z80.bus_request && this->io.z80.bus_ack) return;

    Z80_cycle(&this->z80);
    if (this->z80.pins.RD) {
        if (this->z80.pins.MRQ) {
            this->z80.pins.D = genesis_z80_bus_read(this, this->z80.pins.Addr, this->z80.pins.D, 1);
            if (dbg.trace_on && dbg.traces.z80.mem) {
                dbg_printf(DBGC_READ "\nr.Z80 (%lld)   %06x  v:%02x" DBGC_RST, this->clock.master_cycle_count, this->z80.pins.Addr, this->z80.pins.D);
            }
        }
        else if (this->z80.pins.IO) {
            // All Z80 IO requests return 0xFF
            this->z80.pins.D = 0xFF;
        }
    }
    else if (this->z80.pins.WR) {
        if (this->z80.pins.MRQ) {
            // All Z80 IO requests are ignored
            genesis_z80_bus_write(this, this->z80.pins.Addr, this->z80.pins.D);
            if (dbg.trace_on && dbg.traces.z80.mem) {
                dbg_printf(DBGC_WRITE "\nw.Z80 (%lld)   %06x  v:%04x" DBGC_RST, this->clock.master_cycle_count, this->z80.pins.Addr, this->z80.pins.D);
            }
        }
    }
    // Bus request/ack at end of cycle
    this->io.z80.bus_ack = this->io.z80.bus_request;
}

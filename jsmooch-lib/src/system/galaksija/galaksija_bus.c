//
// Created by . on 2/13/25.
//

#include "galaksija_bus.h"
#include "helpers/physical_io.h"

void galaksija_bus_init(struct galaksija *this)
{

}

static u32 read_kb(struct galaksija *this, u32 addr)
{
    return 1; // no key pressed atm
}

static u32 read_IO(struct galaksija *this, u32 addr, u32 open_bus, u32 has_effect)
{
    if (addr < 0x38) return read_kb(this, addr);
    return this->io.latch;
}

static void write_IO(struct galaksija *this, u32 addr, u32 val)
{
    if (addr < 0x38) return;
    this->io.latch = val & 0b11111100; // 6 bits!
}

u32 galaksija_mainbus_read(struct galaksija *this, u32 addr, u32 open_bus, u32 has_effect)
{
    if (addr < 0x1000)
        return this->ROMA[addr];
    if ((addr < 0x2000) && (this->ROMB_present))
        return this->ROMB[addr & 0xFFF];
    // 2000-27FF is IO
    if (addr < 0x2800) return read_IO(this, addr & 0x3F, open_bus, has_effect);
    if (addr < 0x4000) {
        addr |= (this->io.latch & 0x80) ^ 0x80;
        return this->RAM[addr - 0x2800];
    }
    return open_bus;
}

void galaksija_mainbus_write(struct galaksija *this, u32 addr, u32 val)
{
    if (addr < 0x2000) // ROMA & B
        return;
    // 2000-27FF is IO
    if (addr < 0x2800) {
        write_IO(this, addr & 0x3F, val);
        return;
    }
    if (addr < 0x4000)
    {
        addr |= (this->io.latch & 0x80) ^ 0x80;
        this->RAM[addr - 0x2800] = val;
        return;
    }
}

static void new_frame(struct galaksija *this)
{
    this->crt.y = 0;
    this->crt.cur_output = ((u8 *)this->crt.display->output[this->crt.display->last_written ^ 1]);
    this->crt.display->last_written ^= 1;
    this->clock.master_frame++;
}

static void new_scanline(struct galaksija *this)
{
    this->crt.x = 0;
    this->crt.y++;
    if (this->crt.y >= 320) new_frame(this);
    if (this->crt.y == 56) Z80_notify_IRQ(&this->z80, 1);
}

static void reload_shift_register(struct galaksija *this)
{
 /*
* A10-A7 are from IO latch,
 * A6-A0 come from CPU
   */
    //u32 addr = this->z80.pins.Addr & 0x3F;
    u32 addr = this->z80.regs.R & 0x3F;
    /*if (this->z80.regs.R != this->z80.pins.Addr) {
        printf("\nMISMATCHE");
    }*/
    addr |= ((this->io.latch & 0b111100) << 5);
    this->crt.shift_register = this->ROM_char[addr];
    this->crt.shift_count = 8;
}

void galaksija_cycle(struct galaksija *this)
{
    // Draw a pixel if we're in the visible zone
    if (((this->crt.x >= 32) && (this->crt.x < 160)) &&
            ((this->crt.y >= 56) && (this->crt.y < 264))) {
        if ((this->crt.y == 56) && (this->crt.x == 30)) { // TODO: this is a guess when it goes down
            Z80_notify_IRQ(&this->z80, 0);
        }
        if (this->crt.shift_count == 0) reload_shift_register(this);
        u32 this_px = ((this->crt.shift_register >> 7) & 1) ^ 1;
        this->crt.shift_register <<= 1;
        this->crt.shift_count--;
        this->crt.cur_output[(192 * this->crt.y) + this->crt.x] = this_px;
    }

    this->crt.x++;
    if (this->crt.x >= 192) {
        new_scanline(this);
    }

    // Divide by 2 for Z80
    if (this->clock.z80_divider == 0) {
        // Cycle the Z80!
        Z80_cycle(&this->z80);
        if (this->z80.pins.RD) {
            if (this->z80.pins.MRQ) {// read ROM/RAM
                this->z80.pins.D = galaksija_mainbus_read(this, this->z80.pins.Addr, this->io.open_bus, 1);
                this->io.open_bus = this->z80.pins.D;
                printif(z80.mem, DBGC_Z80 "\nGAL(%06llu)r   %04x   $%02x         TCU:%d" DBGC_RST, *this->z80.trace.cycles, this->z80.pins.Addr, this->z80.pins.D, this->z80.regs.TCU);
            } else if (this->z80.pins.IO) { // read IO port
                printif(z80.io, DBGC_Z80"\nGAL(%06llu)in  %04x   $%02x         TCU:%d" DBGC_RST, *this->z80.trace.cycles, this->z80.pins.Addr, this->z80.pins.D, this->z80.regs.TCU);
            }
        }
        if (this->z80.pins.WR) {
            if (this->z80.pins.MRQ) {// write ROM/RAM
                if (dbg.trace_on && (this->z80.trace.last_cycle != *this->z80.trace.cycles)) {
                    this->z80.trace.last_cycle = *this->z80.trace.cycles;
                    printif(z80.mem, DBGC_Z80 "\nZ80(%06llu)wr  %04x   $%02x         TCU:%d" DBGC_RST, *this->z80.trace.cycles, this->z80.pins.Addr, this->z80.pins.D, this->z80.regs.TCU);
                }
                galaksija_mainbus_write(this, this->z80.pins.Addr, this->z80.pins.D);
            }
            else if (this->z80.pins.IO) {// write IO
                printif(z80.io, DBGC_Z80 "\nZ80(%06llu)out %04x   $%02x         TCU:%d" DBGC_RST, *this->z80.trace.cycles, this->z80.pins.Addr, this->z80.pins.D, this->z80.regs.TCU);
            }
        }
    }
    this->clock.z80_divider ^= 1;
}
//
// Created by . on 2/13/25.
//

#include "galaksija_bus.h"
#include "helpers/physical_io.h"

#define TOP_VBLANK_LINES 56
#define DRAW_LINES 208
#define BOTTOM_VBLANK_LINES 56
#define LEFT_HBLANK_PX 64
#define DRAW_PX 256
#define RIGHT_HBLANK_PX 64

#define LINE_PX (LEFT_HBLANK_PX + DRAW_PX + RIGHT_HBLANK_PX)

void galaksija_bus_init(struct galaksija *this)
{

}

static u32 read_kb(struct galaksija *this, u32 addr)
{
    if (addr == 0) return 1;
    addr--;
    struct physical_io_device *d = cvec_get(this->jsm.IOs, 0);
    struct JSM_KEYBOARD *kbd = &d->keyboard;

    if (addr < 41) { // Just directly grab from keymap
        //if (addr == 40) printf("\n40 SCANNED");
        return kbd->key_states[addr] ^ 1;
    }

    switch(addr) {
        case 41: // ;?
            return kbd->key_states[41] ^ 1;
        case 42: // :?
            return kbd->key_states[51] ^ 1;
        case 43: // ,?
            return kbd->key_states[42] ^ 1;
        case 44: // =
            return kbd->key_states[43] ^ 1;
        case 45: // .?
            return kbd->key_states[44] ^ 1;
        case 46: // /
            return kbd->key_states[45] ^ 1;
        case 47: // return
            return kbd->key_states[46] ^ 1;
        case 48: // break (esc)
            return kbd->key_states[52] ^ 1;
        case 49: // repeat (f1)
            return kbd->key_states[49] ^ 1;
        case 50: // delete
            return kbd->key_states[47] ^ 1;
        case 51: // list (f2)
            return kbd->key_states[50] ^ 1;
        case 52: // shift
            return kbd->key_states[48] ^ 1;
    }
    // starting at 41...
    // 41 :?
    // 42 ;?
    // 43 ,?
    // 44 =
    // 45 .?
    // 46 /
    // 47 return
    // 48 break (esc)
    // 49 repeat (f1)
    // 50 delete
    // 51 list (f2)
    // 52 shift



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
    /*
Video synchronization
; hardware then makes sure (by inserting WAIT states) that the first opcode
; starts to execute exactly in sync with the next horizontal sync.     */
    if (this->crt.y == 56) {
        Z80_notify_IRQ(&this->z80, 1); // draw IRQ
        this->crt.WAIT_on_M1 = 1;
    }
    this->crt.shift_count = 0;
}

static void reload_shift_register(struct galaksija *this)
{
    u32 addr = (this->z80.regs.I << 8) | this->z80.regs.R & 0x7F;
    addr = galaksija_mainbus_read(this, addr, 0, 1);

    // D6 is not used
    addr = (addr & 0b00111111) | ((addr & 0b10000000) >> 1);
    addr |= ((this->io.latch & 0b111100) << 5);

    // x=64 is first shift register load?
    if ((this->crt.x >= 64) && (this->crt.x < 81)) {
        if (this->crt.y == 56) {
            //printf("\nframe:%lld x:%d y:%d.  TCU:%d DECODE?:%d  MCLK:%lld", this->clock.master_frame, this->crt.x, this->crt.y, this->z80.regs.TCU, this->z80.regs.IR == Z80_S_DECODE, this->clock.master_cycle_count);
            //Z80_printf_trace(&this->z80);
        }
    }

    if (0) {
        // CHEAT!
        u32 col_x = (this->crt.x - LEFT_HBLANK_PX) >> 3;
        u32 col_y = (this->crt.y - TOP_VBLANK_LINES) >> 4;
        u32 rom_y = (this->crt.y - TOP_VBLANK_LINES) & 15;
        addr = (col_y * 32) + col_x;
        addr += 0x2800;
        addr = galaksija_mainbus_read(this, addr, 0, 1);
        // D6 is not used...
        addr = (addr & 0b00111111) | ((addr & 0b10000000) >> 1);
        //addr |= rom_y << 7;
        addr |= (this->io.latch & 0b111100) << 5;
    }

    this->crt.shift_register = this->ROM_char[addr] ^ 0xFF;
    this->crt.shift_count = 8;
}

static void cpu_cycle (struct galaksija *this)
{
    Z80_cycle(&this->z80);
    if ((this->crt.WAIT_on_M1) && (this->z80.pins.M1) && (this->z80.pins.Addr == 0x38)) {
        this->crt.WAIT_on_M1 = 0;
        this->crt.WAIT_down_on_hblank = 1;
        this->z80.pins.WAIT = 1;
        Z80_notify_IRQ(&this->z80, 0);
    }
    if (this->z80.pins.RD) {
        if (this->z80.pins.MRQ) {// read ROM/RAM
            this->z80.pins.D = galaksija_mainbus_read(this, this->z80.pins.Addr, this->io.open_bus, 1);
            this->io.open_bus = this->z80.pins.D;
            printif(z80.mem, DBGC_Z80 "\nGAL(%06llu)r   %04x   $%02x         TCU:%d" DBGC_RST,
                    *this->z80.trace.cycles, this->z80.pins.Addr, this->z80.pins.D, this->z80.regs.TCU);
        } else if (this->z80.pins.IO) { // read IO port
            printif(z80.io, DBGC_Z80"\nGAL(%06llu)in  %04x   $%02x         TCU:%d" DBGC_RST,
                    *this->z80.trace.cycles, this->z80.pins.Addr, this->z80.pins.D, this->z80.regs.TCU);
        }
    }
    if (this->z80.pins.WR) {
        if (this->z80.pins.MRQ) {// write ROM/RAM
            if (dbg.trace_on && (this->z80.trace.last_cycle != *this->z80.trace.cycles)) {
                this->z80.trace.last_cycle = *this->z80.trace.cycles;
                printif(z80.mem, DBGC_Z80 "\nZ80(%06llu)wr  %04x   $%02x         TCU:%d" DBGC_RST,
                        *this->z80.trace.cycles, this->z80.pins.Addr, this->z80.pins.D, this->z80.regs.TCU);
            }
            galaksija_mainbus_write(this, this->z80.pins.Addr, this->z80.pins.D);
        } else if (this->z80.pins.IO) {// write IO
            printif(z80.io, DBGC_Z80 "\nZ80(%06llu)out %04x   $%02x         TCU:%d" DBGC_RST,
                    *this->z80.trace.cycles, this->z80.pins.Addr, this->z80.pins.D, this->z80.regs.TCU);
        }
    }
}

void galaksija_cycle(struct galaksija *this)
{
    // Lower IRQ if needed
    if (this->crt.WAIT_down_on_hblank) {
        u32 cx = this->ROMB_present ? 2 : 16;
        if (this->crt.x == cx) {
            this->z80.pins.WAIT = 0;
            this->crt.WAIT_down_on_hblank = 0;
        }
    }
    // Run CPU at clock divisor of 2
    if (this->clock.z80_divider == 0) {
        cpu_cycle(this);
    }
    this->clock.z80_divider ^= 1;



    // Draw a pixel if we're in the visible zone
    if (((this->crt.x >= LEFT_HBLANK_PX) && (this->crt.x < (LEFT_HBLANK_PX+DRAW_PX))) &&
            ((this->crt.y >= TOP_VBLANK_LINES) && (this->crt.y < (TOP_VBLANK_LINES+DRAW_LINES)))) {

        if (this->crt.shift_count == 0) reload_shift_register(this);

        u32 this_px = this->crt.shift_register & 1;
        this->crt.shift_register >>= 1;
        this->crt.shift_count--;
        this->crt.cur_output[(LINE_PX * this->crt.y) + this->crt.x] = this_px;
    }

    // Increment pixel
    this->crt.x++;
    if (this->crt.x >= LINE_PX)
        new_scanline(this);

    this->clock.master_cycle_count++;
}
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

namespace galaksija {
u8 core::read_kb(u32 addr)
{
    if (addr == 0) return 1;
    addr--;
    physical_io_device *d = &IOs[0];
    JSM_KEYBOARD *kbd = &d->keyboard;

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

u8 core::read_IO(u16 addr, u8 open_bus, bool has_effect)
{
    if (addr < 0x38) return read_kb(addr);
    return io.latch;
}

void core::write_IO(u16 addr, u8 val)
{
    if (addr < 0x38) return;
    io.latch = val & 0b11111100; // 6 bits!
}

u8 core::mainbus_read(u16 addr, u8 open_bus, bool has_effect)
{
    if (addr < 0x1000)
        return ROMA[addr];
    if ((addr < 0x2000) && (ROMB_present))
        return ROMB[addr & 0xFFF];
    // 2000-27FF is IO
    if (addr < 0x2800) return read_IO(addr & 0x3F, open_bus, has_effect);
    if (addr < 0x4000) {
        addr |= (io.latch & 0x80) ^ 0x80;
        return RAM[addr - 0x2800];
    }
    return open_bus;
}

void core::mainbus_write(u16 addr, u8 val)
{
    if (addr < 0x2000) // ROMA & B
        return;
    // 2000-27FF is IO
    if (addr < 0x2800) {
        write_IO(addr & 0x3F, val);
        return;
    }
    if (addr < 0x4000)
    {
        addr |= (io.latch & 0x80) ^ 0x80;
        RAM[addr - 0x2800] = val;
        return;
    }
}

void core::new_frame()
{
    crt.y = 0;
    crt.cur_output = static_cast<u8 *>(crt.display->output[crt.display->last_written ^ 1]);
    crt.display->last_written ^= 1;
    clock.master_frame++;
}

void core::new_scanline()
{
    crt.x = 0;
    crt.y++;
    if (crt.y >= 320) new_frame();
    /*
Video synchronization
; hardware then makes sure (by inserting WAIT states) that the first opcode
; starts to execute exactly in sync with the next horizontal sync.     */

    // Schematics show it does this by setting WAIT pin to 1 when M1=1 and IO=1 during IRQ acknowledge cycle.
    // It supposedly goes down at the next line.
    if (crt.y == 56) {
        z80.notify_IRQ(true); // draw IRQ
    }
    crt.shift_count = 0;
}

void core::reload_shift_register()
{
    u32 addr = (z80.regs.I << 8) | z80.regs.R & 0x7F;
    addr = mainbus_read(addr, 0, 1);

    // D6 is not used
    addr = (addr & 0b00111111) | ((addr & 0b10000000) >> 1);
    addr |= ((io.latch & 0b111100) << 5);

    // x=64 is first shift register load?
    if ((crt.x >= 64) && (crt.x < 81)) {
        if (crt.y == 56) {
            //printf("\nframe:%lld x:%d y:%d.  TCU:%d DECODE?:%d  MCLK:%lld", clock.master_frame, crt.x, crt.y, z80.regs.TCU, z80.regs.IR == Z80_S_DECODE, clock.master_cycle_count);
            //Z80_printf_trace(&z80);
        }
    }

    if (false) {
        // CHEAT!
        u32 col_x = (crt.x - LEFT_HBLANK_PX) >> 3;
        u32 col_y = (crt.y - TOP_VBLANK_LINES) >> 4;
        u32 rom_y = (crt.y - TOP_VBLANK_LINES) & 15;
        addr = (col_y * 32) + col_x;
        addr += 0x2800;
        addr = mainbus_read(addr, 0, true);
        // D6 is not used...
        addr = (addr & 0b00111111) | ((addr & 0b10000000) >> 1);
        //addr |= rom_y << 7;
        addr |= (io.latch & 0b111100) << 5;
    }

    crt.shift_register = ROM_char[addr] ^ 0xFF;
    crt.shift_count = 8;
}

void core::cpu_cycle()
{
    z80.cycle();
    if (z80.pins.RD) {
        if (z80.pins.MRQ) {// read ROM/RAM
            z80.pins.D = mainbus_read(z80.pins.Addr, io.open_bus, 1);
            io.open_bus = z80.pins.D;
/*            printif(z80.mem, DBGC_Z80 "\nGAL(%06llu)r   %04x   $%02x         TCU:%d" DBGC_RST,
                    *z80.trace.cycles, z80.pins.Addr, z80.pins.D, z80.regs.TCU);*/
        } else if (z80.pins.IO) { // read IO port
            if (z80.pins.M1) {
                z80.pins.WAIT = 1;
                z80.notify_IRQ(false);
            } else {
                /*printif(z80.io, DBGC_Z80"\nGAL(%06llu)in  %04x   $%02x         TCU:%d" DBGC_RST,
                        *z80.trace.cycles, z80.pins.Addr, z80.pins.D, z80.regs.TCU);*/
            }
        }
    }
    if (z80.pins.WR) {
        if (z80.pins.MRQ) {// write ROM/RAM
            if (::dbg.trace_on && (z80.trace.last_cycle != *z80.trace.cycles)) {
                z80.trace.last_cycle = *z80.trace.cycles;
                /*printif(z80.mem, DBGC_Z80 "\nZ80(%06llu)wr  %04x   $%02x         TCU:%d" DBGC_RST,
                        *z80.trace.cycles, z80.pins.Addr, z80.pins.D, z80.regs.TCU);*/
            }
            mainbus_write(z80.pins.Addr, z80.pins.D);
        } else if (z80.pins.IO) {// write IO
            /*printif(z80.io, DBGC_Z80 "\nZ80(%06llu)out %04x   $%02x         TCU:%d" DBGC_RST,
                    *z80.trace.cycles, z80.pins.Addr, z80.pins.D, z80.regs.TCU);*/
        }
    }
}

void core::cycle()
{
    // Run CPU at clock divisor of 2
    if (clock.z80_divider == 0) {
        cpu_cycle();
    }
    // Lower WAIT if needed
    u32 cx = ROMB_present ? 380 : 382;
    if (crt.x == cx) {
        z80.pins.WAIT = 0;
    }
    clock.z80_divider ^= 1;



    // Draw a pixel if we're in the visible zone
    /*if (((crt.x >= LEFT_HBLANK_PX) && (crt.x < (LEFT_HBLANK_PX+DRAW_PX))) &&
            ((crt.y >= TOP_VBLANK_LINES) && (crt.y < (TOP_VBLANK_LINES+DRAW_LINES)))) {*/
    if (true) {

        if (crt.shift_count == 0) reload_shift_register();

        u32 this_px = crt.shift_register & 1;
        crt.shift_register >>= 1;
        crt.shift_count--;
        crt.cur_output[(LINE_PX * crt.y) + crt.x] = this_px;
    }

    // Increment pixel
    crt.x++;
    if (crt.x >= LINE_PX)
        new_scanline();

    clock.master_cycle_count++;
}
}
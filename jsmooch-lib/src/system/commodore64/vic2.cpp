//
// Created by . on 11/25/25.
//
#include <cassert>

#include "vic2.h"
#include "c64_bus.h"

#include "c64_vic2_color.h"

#include "helpers/enums.h"

namespace VIC2 {

core::core(C64::core *parent) : bus(parent) {
    setup_timing();
    for (u32 i = 0; i < 16; i++) {
        palette[i] = color::calculate_color(i, 1, 50.0f, 100.0f, 1.0f);
    }
}

void core::update_IRQs() {
    io.IF.IRQ = ((io.IF.u & io.IE.u) > 0) ^ 1;
    bus->cpu.pins.IRQ = !io.IF.IRQ;
}

u8 core::read_IO(u16 addr, u8 old, bool has_effect) {
    addr &= 0x3F;
    switch (addr | 0xD000) {
        case 0xD000:
        case 0xD002:
        case 0xD004:
        case 0xD006:
        case 0xD008:
        case 0xD00A:
        case 0xD00C:
        case 0xD00E: {
            sprite &sp = sprites[addr >> 1];
            return sp.x & 0xFF;
        }
        case 0xD001:
        case 0xD003:
        case 0xD005:
        case 0xD007:
        case 0xD009:
        case 0xD00B:
        case 0xD00D:
        case 0xD00F: {
            sprite &sp = sprites[addr >> 1];
            return sp.y; }
        case 0xD010: return io.D010;
        case 0xD011: return io.CR1.u;
        case 0xD012: return vpos;
        case 0xD013: return io.LPX;
        case 0xD014: return io.LPY;
        case 0xD015: return io.ME;
        case 0xD016: return io.CR2.u | 0xC0;
        case 0xD017: return io.MYE; // sprite expansion
        case 0xD018: return io.D018 | 1;
        case 0xD019: return io.IF.u | 0x70;
        case 0xD01A: return io.IE.u | 0xF0;
        case 0xD01B: return io.MDP;
        case 0xD01C: return io.MMC;
        case 0xD01D: return io.MXE;
        case 0xD01E: return io.SSx;
        case 0xD01F: return io.SDx;
        case 0xD020: return io.EC;
        case 0xD021: return io.BC[0];
        case 0xD022: return io.BC[1];
        case 0xD023: return io.BC[2];
        case 0xD024: return io.BC[3];
        case 0xD025: return io.MM[0];
        case 0xD026: return io.MM[1];
        case 0xD027: return io.MC[0];
        case 0xD028: return io.MC[1];
        case 0xD029: return io.MC[2];
        case 0xD02A: return io.MC[3];
        case 0xD02B: return io.MC[4];
        case 0xD02C: return io.MC[5];
        case 0xD02D: return io.MC[6];
        case 0xD02E: return io.MC[7];
    }
    return 0xFF;
}

void core::update_SSx(u8 val) {
    io.IF.MBC |= (io.SDx == 0) && (val > 0);
    io.SDx = val;
    io.IF.MBC &= val > 0;
    update_IRQs();
}

void core::update_SDx(u8 val) {
    io.IF.MMC |= (io.SSx == 0) && (val > 0);
    io.SSx = val;
    io.IF.MMC &= val > 0;
    update_IRQs();
}

void core::write_IO(u16 addr, u8 val) {
    addr &= 0x3F;
    switch (addr | 0xD000) {
        case 0xD000:
        case 0xD002:
        case 0xD004:
        case 0xD006:
        case 0xD008:
        case 0xD00A:
        case 0xD00C:
        case 0xD00E: {
            sprite &sp = sprites[addr >> 1];
            sp.x = (sp.x & 0x100) | val;
            return;
        }
        case 0xD001:
        case 0xD003:
        case 0xD005:
        case 0xD007:
        case 0xD009:
        case 0xD00B:
        case 0xD00D:
        case 0xD00F: {
            sprite &sp = sprites[addr >> 1];
            sp.y = val;
            return; }
        case 0xD010:
            sprites[0].x = (sprites[0].x & 0xFF) | ((val & 1) << 8);
            sprites[1].x = (sprites[1].x & 0xFF) | ((val & 2) << 7);
            sprites[2].x = (sprites[2].x & 0xFF) | ((val & 4) << 6);
            sprites[3].x = (sprites[3].x & 0xFF) | ((val & 8) << 5);
            sprites[4].x = (sprites[4].x & 0xFF) | ((val & 0x10) << 4);
            sprites[5].x = (sprites[5].x & 0xFF) | ((val & 0x20) << 3);
            sprites[6].x = (sprites[6].x & 0xFF) | ((val & 0x40) << 2);
            sprites[7].x = (sprites[7].x & 0xFF) | ((val & 0x80) << 1);
            io.D010 = val;
            return;
        case 0xD011: // control register 1
            io.CR1.u = val;
            if (io.CR1.RSEL) {
                timing.frame.first_line = 51;
                timing.frame.last_line = 250;
            }
            else {
                timing.frame.first_line = 55;
                timing.frame.last_line = 246;
            }
            update_raster_compare((io.raster_compare & 0xFF) | ((val & 0x80) << 1));
            update_IRQs();
            return;
        case 0xD012: // raster counter
            // TODO: set comparison line for raster interrupt
            update_raster_compare((io.raster_compare & 0x100) | val);
            update_IRQs();
            return;
        case 0xD013: // LPX
            printf("\nWARN WRITE LPX!? WHAT TO DO?");
            // TODO: write?
            return;
        case 0xD014: // LPY
            printf("\nWARN WRITE LPY!? WHAT TO DO?");
            return;
        case 0xD015:
            io.ME = val;
            sprites[0].enabled = (val >> 0) & 1;
            sprites[1].enabled = (val >> 1) & 1;
            sprites[2].enabled = (val >> 2) & 1;
            sprites[3].enabled = (val >> 3) & 1;
            sprites[4].enabled = (val >> 4) & 1;
            sprites[5].enabled = (val >> 5) & 1;
            sprites[6].enabled = (val >> 6) & 1;
            sprites[7].enabled = (val >> 7) & 1;
            return;
        case 0xD016:
            io.CR2.u = val;
            if (io.CR2.CSEL) {
                timing.line.first_col = 24;
                timing.line.last_col = 343;
            }
            else {
                timing.line.first_col = 31;
                timing.line.last_col = 334;
            }
            return;
        case 0xD017:
            io.MYE = val;
            sprites[0].y_expand = (val >> 0) & 1;
            sprites[1].y_expand = (val >> 1) & 1;
            sprites[2].y_expand = (val >> 2) & 1;
            sprites[3].y_expand = (val >> 3) & 1;
            sprites[4].y_expand = (val >> 4) & 1;
            sprites[5].y_expand = (val >> 5) & 1;
            sprites[6].y_expand = (val >> 6) & 1;
            sprites[7].y_expand = (val >> 7) & 1;
            return;
        case 0xD018:
            io.D018 = val;
            io.ptr.VM = (val & 0xF0) << 6;
            io.ptr.CB = (val & 0x0E) << 10;
            return;
        case 0xD019:
            // Only clear on write of 1!?
            io.IF.u = val & 0x0b10001111;
            update_IRQs();
            return;
        case 0xD01A:
            io.IE.u = val & 0x0b00001111;
            update_IRQs();
            return;
        case 0xD01B:
            io.MDP = val;
            return;
        case 0xD01C:
            io.MMC = val;
            return;
        case 0xD01D:
            io.MXE = val;
            sprites[0].x_expand = (val >> 0) & 1;
            sprites[1].x_expand = (val >> 0) & 1;
            sprites[2].x_expand = (val >> 0) & 1;
            sprites[3].x_expand = (val >> 0) & 1;
            sprites[4].x_expand = (val >> 0) & 1;
            sprites[5].x_expand = (val >> 0) & 1;
            sprites[6].x_expand = (val >> 0) & 1;
            sprites[7].x_expand = (val >> 0) & 1;
            return;
        case 0xD01E:
            update_SSx(0);
            update_IRQs();
            return;
        case 0xD01F:
            update_SDx(0);
            return;
        case 0xD020: io.EC = val & 15; return;
        case 0xD021: io.BC[0] = val & 15; return;
        case 0xD022: io.BC[1] = val & 15; return;
        case 0xD023: io.BC[2] = val & 15; return;
        case 0xD024: io.BC[3] = val & 15; return;
        case 0xD025: io.MM[0] = val & 15; return;
        case 0xD026: io.MM[1] = val & 15; return;
        case 0xD027: io.MC[0] = val & 15; return;
        case 0xD028: io.MC[1] = val & 15; return;
        case 0xD029: io.MC[2] = val & 15; return;
        case 0xD02A: io.MC[3] = val & 15; return;
        case 0xD02B: io.MC[4] = val & 15; return;
        case 0xD02C: io.MC[5] = val & 15; return;
        case 0xD02D: io.MC[6] = val & 15; return;
        case 0xD02E: io.MC[7] = val & 15; return;
    }

}

void core::write_color_ram(u16 addr, u8 val) {
    if (addr < 1000) COLOR[addr] = val & 15;
}

u8 core::read_color_ram(u16 addr, u8 old, bool has_effect) {
    addr &= 0x3FF;
    u8 val = open_bus;
    if (addr < 1000) {
        val = (val & 0xF0) | COLOR[addr];
    }
    else {
        printf("\nILLEGAL CRAM READ AT %04x!", addr | 0xDB00);
    }
    return val;
}

void core::reset() {
    hpos = vpos = 0;
}

void core::output_BG() { // output 8 pixels of background!
    //assert(hpos<timing.line.pixels_per);
    if (hpos>timing.line.pixels_per) {
        printf("\nUH OH NO TOO MANY PIXELS %d", hpos);
    }
    line_output[hpos++] = io.EC;
}

void core::mem_access() {
    // If nothing else, we do an idle access
    read_mem(read_mem_ptr, 0x3FFF);
    read_mem(read_mem_ptr, 0x3FFF);
}

void core::output_px() {
    if (hpos == timing.line.first_col) hborder_on = false;
    if (hpos == timing.line.last_col) hborder_on = true;

/*
 *0-63
 *56 idle
 *57 idle
 *58 idle
 *59 idle
 *60 0s
 *61 ss
 *62 1s
 *63 ss
 *64 2s
 *65 ss
 *1 3s
 *2 ss
 *3 4s
 *4 ss
 *5 5s
 *6 ss
 *7 6s
 *8 ss
 *9 7s
 *10 ss
 *11 r_
 *12 r_
 *13 r_  x=000,
 *14 r_
 *15
 *16
 *17
 *18
 *19
 *20
 */
    mem_access(); // Do our first memory access/DRAM refresh/etc.

    if (vborder_on || hborder_on) {
        output_BG();
        return;
    }

}

void core::cycle() {
    // 65-5 read accesses go to DRAM refresh
    // = 60

    // either in display state or not
    // regardless of display state, chargen/bitmap reads happen
    // only display state do badlinematrix/color RAM reads happen
    //

   for (u32 i = 0; i < 8; i++) {
        output_px();
    }
    // Generate border


    /* so we have 4 data...
     * register, RAM, screen matrix, color RAM
    1 cycle = 8 pixel
    1 RAM read per active cycle,
    or with AEC set to 1, we get our normal RAM read + a second to refill screen matrix
    */
    // Of course where IS active area? waht about IRQ etc.?

    // So during active 65 cycles,
    // BA=1 3 cycles before AEC=1
    // BA=0 1 cycle before AEC=0

}

static void schedule_scanline(void *ptr, u64 key, u64 clock, u32 jitter) {
    auto *v = static_cast<VIC2::core *>(ptr);
    clock -= jitter;
    v->new_scanline();

    v->bus->scheduler.only_add_abs_w_tag(clock + v->timing.frame.cycles_per, 0, v, &schedule_scanline, nullptr, 2);
}

void new_frame(void *ptr, u64 key, u64 cur_clock, u32 jitter) {
    cur_clock -= jitter;

    auto *v = static_cast<VIC2::core *>(ptr);
    v->master_frame++;

    if (v->bus->dbg.events.view.vec) {
        debugger_report_frame(v->bus->dbg.interface);
    }
    v->cur_output = static_cast<u8 *>(v->display->output[v->display->last_written ^ 1]);
    v->line_output = v->cur_output;
    memset(v->cur_output, 0, v->timing.frame.num_pixels);
    v->display->last_written ^= 1;

    v->field ^= 1;
    v->vpos = -1;

    //vblank(v, 0, cur_clock, 0);

    v->bus->scheduler.only_add_abs_w_tag(cur_clock + v->timing.line.cycles_per, 0, v, &new_frame, nullptr, 2);
}

void core::update_raster_compare(u16 val) {
    io.raster_compare = val;
    do_raster_compare();
}

void core::update_vpos(u16 val) {
    vpos = val;
    do_raster_compare();
    line_output = cur_output + (timing.line.pixels_per * vpos);
}

void core::do_raster_compare() {
    if (!io.raster_compare_protect && !io.IF.RST && io.raster_compare == vpos) {
        io.IF.RST = 1;
        io.raster_compare_protect = true;
    }
}

void core::new_scanline() {
    //timing.scanline_start = cur_clock;
    io.raster_compare_protect = io.IF.RST;
    if ((vpos == timing.frame.first_line) && io.CR1.DEN) vborder_on = false;
    if (vpos == timing.frame.last_line) vborder_on = true;
    update_vpos(vpos+1);
    if (bus->dbg.events.view.vec) {
        debugger_report_line(bus->dbg.interface, vpos);
    }
    /*
    A Bad Line Condition is given at any arbitrary clock cycle if, at the
    negative edge of ϕ0 at the beginning of the cycle, RASTER >= $30 and RASTER
    <= $f7 and the lower three bits of RASTER are equal to YSCROLL, and if the
    DEN bit was set during an arbitrary cycle of raster line $30.
    */
    if (vpos == 0x30) {
        io.latch.DEN = io.CR1.DEN;
    }
    bad_line = ((vpos >= 30) && (vpos <= 0xF7) && ((vpos & 7) == io.CR1.YSCROLL));
}

void core::schedule_first() {
    schedule_scanline(this, 0, 0, 0);
    bus->scheduler.only_add_abs_w_tag(timing.frame.cycles_per, 0, this, &new_frame, nullptr, 2);
    // TODO: schedule drawing...

}

void core::setup_timing() {
    printf("\nDISPLAY STANDARD %d", bus->display_standard);
    switch (bus->display_standard) {
        case jsm::NTSC: {
            timing.line.cycles_per = 65;
            timing.line.pixels_per = 520;
            timing.frame.lines_per = 263;
            timing.frame.cycles_per = timing.line.cycles_per * timing.frame.lines_per;
            timing.vblank.first_line = 13;
            timing.vblank.last_line = 40;
            timing.second.frames_per = 60;
            //  pixels are 412-489-396;
            break;
        }
        case jsm::PAL: {
            timing.line.cycles_per = 63;
            timing.line.pixels_per = 504;
            timing.frame.lines_per = 312;
            timing.frame.cycles_per = timing.line.cycles_per * timing.frame.lines_per;
            timing.vblank.first_line = 300;
            timing.vblank.last_line = 15;
            timing.second.frames_per = 50;
            // pixels are 404-480-380
            break;
        }
        default:
            assert(1==2);
            return;
    }
    timing.frame.num_pixels = timing.frame.lines_per * timing.line.pixels_per;
    timing.frame.cycles_per = timing.line.cycles_per * timing.line.pixels_per;
    timing.second.cycles_per = timing.frame.cycles_per * timing.second.frames_per;
}

}

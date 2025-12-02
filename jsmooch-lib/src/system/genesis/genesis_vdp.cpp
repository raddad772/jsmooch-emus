//
// Created by . on 6/1/24.
//

/*
 * NOTE: parts of the DMA subsystem are almost directly from Ares.
 * I wanted to get it right and write a good article about it. Finding accurate
 *  info was VERY difficult, so I kept very close to the best source of
 *  info I could find - Ares
 */

#include <cstdlib>
#include <cassert>
#include <cstdio>
#include <cstring>

#include "helpers/debug.h"
#include "genesis_bus.h"
#include "genesis_vdp.h"
#include "genesis_debugger.h"

namespace genesis::VDP {

#define PLANE_A 0
#define PLANE_B 1
#define PLANE_WINDOW 2

#define SC_ARRAY_H32      0
#define SC_ARRAY_H40      1
#define SC_ARRAY_DISABLED 2 // H32 timing
#define SC_ARRAY_H40_VBLANK 3


//    UDS<<2 + LDS           neither   just LDS   just UDS    UDS+LDS
static constexpr u32 write_maskmem[4] = { 0,     0xFF00,    0x00FF,      0 };

struct slice {
    u8 pattern[16];
    u32 v, h;
    u32 plane;
};

u16 core::VRAM_read(u32 addr, int report) const {
    u16 a = VRAM[addr & 0x7FFF];
    //if (dbg.traces.vdp4 && report) DFT("\nVRAM READ %04x: %04x", addr, a);
    return a;
}


void core::set_m68k_dtack()
{
    printf("\nYAR");
    //this->m68k.pins.DTACK = !(this->io.m68k.VDP_FIFO_stall | this->io.m68k.VDP_prefetch_stall);
    //this->io.m68k.stuck = !this->m68k.pins.DTACK;
}

void core::VRAM_write(u16 addr, u16 val)
{
    /*assert(io.vram_mode == 0);
    if (dbg.traces.vdp) printf("\nVRAM write %04x: %04x, cyc:%lld", addr & 0x7FFF, val, bus->clock.master_cycle_count);
    if (dbg.traces.vdp4) DFT("\nVRAM write %04x: %04x", addr & 0x7FFF, val);*/
    DBG_EVENT(DBG_GEN_EVENT_WRITE_VRAM);
    VRAM[addr & 0x7FFF] = val;
}

#define WSWAP     if (addr & 1) v = (v & 0xFF00) | (val & 0xFF);\
                  else v = (v & 0xFF) | (val & 0xFF00);

u16 core::VRAM_read_byte(u16 addr) const {
    u16 val = VRAM_read(addr, 1);
    u16 v = 0;
    WSWAP;
    return v;
}

void core::VRAM_write_byte(u16 addr, u16 val)
{
    u16 v = VRAM_read(addr >> 1, 1);
    //if (dbg.traces.vdp4) DFT("\nVRAM writebyte_r %04x: %04x", addr, v);
    WSWAP;
    //if (dbg.traces.vdp4) DFT("\nVRAM writebyte %04x: %04x", addr, v);
    VRAM_write(addr >> 1, v);
}

u16 core::VSRAM_read(const u16 addr) const {
    return (addr > 39) ? 0 : VSRAM[addr];
}

void core::VSRAM_write(u16 addr, u16 val)
{
    DBG_EVENT(DBG_GEN_EVENT_WRITE_VSRAM);
    if (addr < 40) VSRAM[addr] = val & 0x3FF;
}

void core::VSRAM_write_byte(const u16 addr, const u16 val)
{
    u16 v = VSRAM_read(addr >> 1);
    WSWAP;
    VSRAM_write(addr >> 1, v);
}

u16 core::CRAM_read(const u16 addr) const {
    return CRAM[addr & 0x3F];
}

void core::CRAM_write(const u16 addr, const u16 val)
{
    //if (dbg.traces.vdp4) DFT("\nCRAM WRITE %04x: %04x", addr & 0x3F, val & 0x1FF);
    DBG_EVENT(DBG_GEN_EVENT_WRITE_CRAM);
    CRAM[addr & 0x3F] = val & 0x1FF;
}

void core::CRAM_write_byte(u16 addr, const u16 val)
{
    addr = (addr & 0x7F);
    u16 v = CRAM_read(addr >> 1);
    WSWAP;
    CRAM_write(addr >> 1, v);
}

u16 core::read_counter() const
{
    if (io.counter_latch) return io.counter_latch_value;
    u16 vcnt = bus->clock.vdp.vcount;
    /*if (io.interlace_mode & 1) {
        if (io.interlace_mode & 2) vcnt <<= 1;
        vcnt = (vcnt & 0xFFFE) | ((vcnt >> 8) ^ 1);
    }*/
    if(vcnt >= 0xEB) vcnt += 0xFA;
    return vcnt << 8 | bus->clock.vdp.hcount;
}

void core::init_slot_tables()
{
    // scanline h32
    // !HSYNC low
    //7X Sprite Tile Row (previous line)
    //68K Access Slot
    //Horizontal Scroll Data
    //4X Sprite Tile Row (previous line)
    //!HSYNC high
    // the hsync goes low in same place here...

#define SR(start,end,value) for (u32 i = (start); i < (end); i++) { s[i] = value; }
    enum slot_kinds *s = &slot_array[SC_ARRAY_H32][0];
    // still slow-clock here
    s[0] = slot_hscroll_data;
    SR(1,5,slot_sprite_pattern);
    // HSYNC HI here, and fast-clock
    s[5] = slot_layer_a_mapping;
    s[6] = slot_sprite_pattern;
    SR(7,9,slot_layer_a_pattern);
    s[9] = slot_layer_b_mapping;
    s[10] = slot_sprite_pattern;
    SR(12,14,slot_layer_b_pattern);

    u32 sn = 13;
    u32 first = 1;
    for (u32 j = 0; j < 5; j++) { // repeat 5 times
         for (u32 x = 0; x < 4; x++) { // repeat 4 times with minor change
#define S(x) s[sn] = x; sn++
             S(slot_layer_a_mapping);
             if (x == 3) { S(slot_external_access); }
             else { S(slot_refresh_cycle); }
             S(slot_layer_a_pattern);
             S(slot_layer_a_pattern);
             S(slot_layer_b_mapping);
             S(slot_sprite_mapping);
             S(slot_layer_b_pattern);
             if (first) {
                 first = 0;
                 S(slot_layer_b_pattern_first_trigger);
             }
             else {
                 S(slot_layer_b_pattern_trigger);
             }
        }
    }
    SR(141, 143, slot_external_access);
    SR(143, 156, slot_sprite_pattern);
    s[156] = slot_external_access;
    SR(157, 159, slot_sprite_pattern);
    SR(159, 164, slot_sprite_pattern);
    // HSYNC LOW
    SR(164, 170, slot_sprite_pattern);
    s[170] = slot_external_access;

    // Now do h40
    s = &slot_array[SC_ARRAY_H40][0];
    // slow-clock first 6 clocks
    s[0] = slot_hscroll_data;
    SR(1,5,slot_sprite_pattern);
    // now fast-clock
    s[5] = slot_layer_a_mapping;
    s[6] = slot_sprite_pattern;
    s[7] = s[8] = slot_layer_a_pattern;
    s[9] = slot_layer_b_mapping;
    s[10] = slot_sprite_pattern;
    s[11] = s[12] = slot_layer_b_pattern;
    // -1 fetch finish @ 12
    // then every 8 cycles @0 20 times
    // -1=12,
    // 0=20
    // 1=28
    // 2=36
    // 3=44
    // 4=52
    // 12 + (n+1)*8
    // so for n=19, sc 172 should be the last draw time
    // according to this we are 8 early


    // measured:
    // COL:0 MCLOCK:215 SC:13 H40:1
    // COL:19 MCLOCK:2648 SC:165 H40:1
    // HSYNC ON M:2635 SC:164 H40:1
    sn = 13;
    first = 1;
    for (u32 j = 0; j < 5; j++) {
        for (u32 x = 0; x < 4; x++) {
            S(slot_layer_a_mapping);
            if (x == 3) { S(slot_refresh_cycle); }
            else { S(slot_external_access); }
            S(slot_layer_a_pattern);
            S(slot_layer_a_pattern);
            S(slot_layer_b_mapping);
            S(slot_sprite_mapping);
            S(slot_layer_b_pattern);
            if (first) {
                S(slot_layer_b_pattern_first_trigger);
                first = 0;
            }
            else {
                S(slot_layer_b_pattern_trigger);
            }
        }
    }
    SR(173,175, slot_external_access);
    SR(175,197, slot_sprite_pattern); // 22
    // SLO CLOCK here
    s[197] = slot_sprite_pattern; // +1
    s[198] = slot_external_access;
    SR(199, 210, slot_sprite_pattern); // 199...209 = 11 slots

    // first 6 clocks slow.
    // next 191 fast
    // then starting at 197? slow again

    // Now do rendering disabled
    s = &slot_array[SC_ARRAY_DISABLED][0];
    //TODO: fill this
    // FOR NOW just put in 1 refersh slot every 16
    sn = 0;
    for (u32 i = 0; i < 171; i++) {
        if ((i & 7) == 0) { S(slot_refresh_cycle); }
        else { S(slot_external_access); }
    }

    // Now do vblank H40
    s = slot_array[SC_ARRAY_DISABLED+SC_ARRAY_H40];
    sn = 0;
    for (u32 i = 0; i < 211; i++) {
        if ((i & 7) == 0) { S(slot_refresh_cycle); }
        else { S(slot_external_access); }
    }
}
#undef S
#undef SR

void core::handle_iack(void* ptr)
{
    auto th = static_cast<core*>(ptr);
    if (th->irq.vblank.enable && th->irq.vblank.pending) th->irq.vblank.pending = 0;
    else if (th->irq.hblank.enable && th->irq.hblank.pending) th->irq.hblank.pending = 0;
    th->bus->update_irqs();
}

core::core(genesis::core *bus_in) : bus(bus_in)
{
    term_out_ptr = &term_out[0];
    init_slot_tables();
    bus->m68k.register_iack_handler(static_cast<void *>(this), &core::handle_iack);
}

void core::set_clock_divisor() const
{
    // first 200 SC slots in H40 are /4
    // the rest are /5
    bus->clock.vdp.clock_divisor = latch.h40 && (sc_slot <= 199) ? 16 : 20;
}

void core::latch_hcounter()
{
    irq.hblank.counter = irq.hblank.reload; // TODO: load this from register
}

void core::do_vcounter()
{
    if ((bus->clock.vdp.vcount >= 0) && (bus->clock.vdp.vcount <= (1+bus->clock.vdp.bottom_rendered_line))) {
        // Do down counter if we're not in line 0 or 224
        if (irq.hblank.counter == 0) {
            irq.hblank.pending = 1;
            latch_hcounter();
            bus->update_irqs();
        } else irq.hblank.counter--;
    }
}

void core::hblank(u32 new_value)
{
    bus->clock.vdp.hblank_active = new_value;
    set_clock_divisor();
}

void core::set_sc_array()
{
    if (((bus->clock.vdp.vblank_active) && (bus->clock.vdp.vcount != (bus->clock.timing.frame.scanlines_per-1))) || (!io.enable_display)) {
        sc_array = SC_ARRAY_DISABLED + latch.h40;
    }
    else sc_array = latch.h40;
}

void core::vblank(bool new_value)
{
    //bool old_value = bus->clock.vdp.vblank_active;
    bus->clock.vdp.vblank_active = new_value;  // Our value that indicates vblank yes or no
    io.vblank = new_value;     // Our IO vblank value, which can be reset

    if (new_value) {
        set_sc_array();
        irq.vblank.pending = 1;
        bus->update_irqs();
    }

    set_clock_divisor();
}

void core::new_frame(void* ptr, u64 key, u64 cur_clock, u32 jitter)
{
    u64 cur = cur_clock - jitter;
    auto *bus = static_cast<genesis::core *>(ptr);

    bus->clock.vdp.frame_start = cur;
    debugger_report_frame(bus->dbg.interface);
    bus->vdp.cur_output = static_cast<u16 *>(bus->vdp.display->output[bus->vdp.display->last_written ^ 1]);
    bus->clock.master_frame++;

    bus->clock.vdp.field ^= 1;
    bus->clock.vdp.vcount = -1;
    bus->clock.vdp.vblank_active = 0;
    bus->vdp.cur_output = static_cast<u16 *>(bus->vdp.display->output[bus->vdp.display->last_written]);
    memset(bus->vdp.cur_output, 0, 1280*240*2);
    bus->vdp.display->last_written ^= 1;
    bus->clock.current_back_buffer ^= 1;
    bus->clock.current_front_buffer ^= 1;
    for (u32 i = 0; i < 2; i++) {
        memset(bus->vdp.debug.px_displayed[bus->clock.current_back_buffer][i], 0, sizeof(debug.px_displayed[bus->clock.current_back_buffer][i]));
    }

    bus->vdp.display->scan_y = 0;
    bus->vdp.display->scan_x = 0;
    //if (bus->clock.vdp.vcount == 0) {
    //printf("\nSCHEDULE NEXT FRAME FOR %lld", cur + bus->clock.timing.frame.cycles_per);
        bus->scheduler.only_add_abs_w_tag(cur + bus->clock.timing.frame.cycles_per, 0, bus, &new_frame, nullptr, 2);
    //}

    bus->vdp.set_clock_divisor();
}


void core::print_scroll_info()
{
    printf("\nSCROLL AT LINE %d. HMODE:%d VMODE:%d", bus->clock.vdp.vcount, io.hscroll_mode, io.vscroll_mode);
    printf("\nHSCROLL A:%d VSCROLL A:%d", fetcher.hscroll[0], fetcher.vscroll_latch[0]);
    printf("\nFG WIDTH:%d  HEIGHT:%d", io.foreground_width, io.foreground_height);
}

void core::gen_z80_interrupt_off(void *ptr, u64 key, u64 clock, u32 jitter)
{
    auto *bus = static_cast<genesis::core *>(ptr);
    bus->z80.notify_IRQ(false);
}

void core::new_scanline(u64 cur_clock)
{
    // We use info from last line to render this one, before we reset it!
    render_sprites();
    bus->clock.vdp.vcount++;
    //printf("\nNEW SCANLINE %d clk:%lld", bus->clock.vdp.vcount, bus->clock.master_cycle_count);
    sc_slot = 0;
    set_clock_divisor();

    line.sprite_mappings = 0;
    line.sprite_patterns = 0;
    latch.h40 = io.h40;
    if (this->dbg.events.view.vec) {
        debugger_report_line(bus->dbg.interface, bus->clock.vdp.vcount);
    }
    sprite_collision = sprite_overflow = 0;
    bus->clock.vdp.hcount = 0;
    display->scan_y++;
    display->scan_x = 0;
    cur_pixel = bus->clock.vdp.vcount * 1280;
    assert(cur_pixel < (1280*312));
    // 281, 282
    assert(bus->clock.vdp.vcount < 312);
    if (bus->clock.vdp.vcount < bus->clock.vdp.bottom_rendered_line) {
        line.screen_x = 0;

        // Set up the current row's fetcher...
        fetcher.column = 0;
        fetcher.hscroll[2] = 0; // Reset window
        memset(&ringbuf, 0, sizeof(ringbuf));

        u32 planes[2];
        get_line_hscroll(bus->clock.vdp.vcount, planes);

        u32 plane_wrap = (8 * io.foreground_width) - 1;

        for (u32 p = 0; p < 2; p++) {
            // Calculate fine X...
            fetcher.fine_x[p] = planes[p] & 15;

            // Unflip the hscroll value...
            fetcher.hscroll[p] = (0 - static_cast<i32>(planes[p])) & plane_wrap;
        }

        // DEBUG STUFF NOW
        auto *r = &debug.output_rows[bus->clock.vdp.vcount];
        r->h40 = io.h40;
        r->hscroll_mode = io.hscroll_mode;
        r->vscroll_mode = io.vscroll_mode;
        r->video_mode = io.mode5 ? 5 : 4;
        r->hscroll[0] = fetcher.hscroll[0];
        r->hscroll[1] = fetcher.hscroll[1];

        if (bus->clock.vdp.vcount == 0) {
            i32 col = -1;
            for (u32 i = 0; i < 21; i++) {
                u32 vscrolls[2];
                get_vscrolls(col++, vscrolls);
                r->vscroll[0][i] = vscrolls[0];
                r->vscroll[1][i] = vscrolls[1];
            }
        }

        // So, fetches happen in 16-pixel (2-word) groups.
        // There's an extra 16-pixel fetch off the left side of the screen, for fine hscroll &15
    }

    line.screen_x = 0;
    line.screen_y = bus->clock.vdp.vcount;

    sc_slot = 0;
    fetcher.vscroll_latch[0] = VSRAM[0];
    fetcher.vscroll_latch[1] = VSRAM[1];

    if (bus->clock.vdp.vcount == 0) {
        vblank(false);
    }
    else if (bus->clock.vdp.vcount == bus->clock.vdp.vblank_on_line) {
        vblank(true);
        bus->z80_interrupt(1); // Z80 asserts vblank interrupt for 2573 mclocks
        bus->scheduler.only_add_abs(bus->clock.master_cycle_count + 2573, 0, bus, &gen_z80_interrupt_off, nullptr);
    }
    //if (bus->clock.vdp.vcount == 0) latch_hcounter();
    else if (bus->clock.vdp.vcount >= (bus->clock.vdp.bottom_rendered_line+2)) latch_hcounter(); // in vblank, continually reload. TODO: make dependent on vblank
    else do_vcounter();
    set_sc_array();
}


void core::schedule_scanline(void *ptr, u64 key, u64 clock, u32 jitter)
{
    auto *th = static_cast<core *>(ptr);
    u64 cur = clock - jitter;
    u64 next_scanline = cur + th->bus->clock.timing.scanline.cycles_per;
    th->bus->scheduler.only_add_abs_w_tag(next_scanline, 0, th, &schedule_scanline, nullptr, 1);
    th->new_scanline(cur);
}


void core::schedule_first()
{
    schedule_scanline(this, 0, 0, 0);
    printf("\nFIRST NF FOR %d", bus->clock.timing.frame.cycles_per);
    bus->scheduler.only_add_abs_w_tag(bus->clock.timing.frame.cycles_per, 0, bus, &new_frame, nullptr, 2);
}


bool core::fifo_empty() const
{
    return fifo.len == 0;
}

bool core::fifo_overfull() const
{
    return fifo.len > 4;
}

bool core::fifo_full() const
{
    return fifo.len > 3;
}

void core::recalc_window()
{
    if (io.window_RIGHT) {
        window.left_col = io.window_h_pos;
        window.right_col = latch.h40 ? 40 : 32;
    }
    else {
        window.left_col = 0;
        window.right_col = io.window_h_pos;
    }
    if (io.window_DOWN) {
        window.top_row = io.window_v_pos;
        window.bottom_row = 30;
    }
    else {
        window.top_row = 0;
        window.bottom_row = io.window_v_pos;
    }
    window.nt_base = io.window_table_addr;
    if (latch.h40)
        window.nt_base &= 0b111110; // Ignore lowest bit in h40
    window.nt_base <<= 10;
    //printf("\nWINDOW LEFT:%d RIGHT:%d TOP:%d BOTTOM:%d", window.left_col, window.right_col, window.top_row, window.bottom_row);
}

void core::write_vdp_reg(u16 rn, u16 val)
{
    val &= 0xFF;
    switch(rn) {
        case 0: // Mode Register 1
            io.blank_left_8_pixels = (val >> 5) & 1;
            irq.hblank.enable = (val >> 4) & 1;
            if ((!io.counter_latch) && (val & 2))
                io.counter_latch_value = read_counter();
            io.counter_latch = (val >> 1) & 1;
            io.enable_overlay = (val & 1) ^ 1;
            bus->update_irqs();
            // TODO: handle more bits
            return;
        case 1: // Mode regiser 2
            io.vram_mode = (val >> 7) & 1;
            io.enable_display = (val >> 6) & 1;
            irq.vblank.enable = (val >> 5) & 1;
            //printf("\nVBLANK ENABLE?%d", irq.vblank.enable);
            //if (irq.vblank.enable) printf("\nENABLE PC: %06x", this->m68k.regs.PC);
            dma.enable = (val >> 4) & 1;
            io.cell30 = (val >> 3) & 1;
            // TODO: handle more bits
            bus->clock.vdp.bottom_rendered_line = io.cell30 ? 239 : 223;
            bus->clock.vdp.vblank_on_line = bus->clock.vdp.bottom_rendered_line + 1;
            io.mode5 = (val >> 2) & 1;
            if (!io.mode5) printf("\nWARNING MODE5 DISABLED");

            bus->update_irqs();
            dma_run_if_ready();
            return;
        case 2: // Plane A table location
            io.plane_a_table_addr = (val & 0b01111000) << 9;
            return;
        case 3: // Window nametable location
            io.window_table_addr = (val & 0b111110) >> 1; // lowest bit ignored in H40 << 9
            recalc_window();
            return;
        case 4: // Plane B location
            io.plane_b_table_addr = (val & 15) << 12;
            return;
        case 5: // Sprite table location
            io.sprite_table_addr = (val & 0xFF) << 8;
            return;
        case 6: // um
            //io.sprite_generator_addr = (io.sprite_generator_addr & 0x7FFF) | (((val >> 5) & 1) << 15);
            return;
        case 7:
            io.bg_color = val & 63;
            return;
        case 8: // master system hscroll
            //printf("\nwrite SMS hscroll %d", val & 0xFF);
            return;
        case 9: // master system vscroll
            //printf("\nwrite SMS vscroll %d", val & 0xFF);
            return;
        case 10: // horizontal interrupt counter
            irq.hblank.reload = val;
            return;
        case 11: // Mode register 3
            io.enable_th_interrupts = (val >> 3) & 1;
            io.vscroll_mode = static_cast<vertical_scroll_modes>((val >> 2) & 1);
            io.hscroll_mode = static_cast<horizontal_scroll_modes>(val & 3);
            // TODO: do all of these
            return;
        case 12: // Mode register 4
            // TODO: handle more of these
            io.h40 = (val & 1) != 0;
            set_sc_array();
            set_clock_divisor();
            recalc_window();
            io.enable_shadow_highlight = (val >> 3) & 1;
            io.interlace_mode = static_cast<interlace_modes>((val >> 1) & 3);
            return;
        case 13: // horizontal scroll data addr
            io.hscroll_addr = (val & 0x7F) << 9;
            return;
        case 14: // mostly unused
            return;
        case 15:
            command.increment = val & 0xFF;
            return;
        case 16: {
            u32 va = (val &3);
            switch(va) {
                case 0:
                    io.foreground_width = 32;
                    break;
                case 1:
                    io.foreground_width = 64;
                    break;
                case 2:
                    printf("\nWARNING BAD FOREGROUND WIDTH");
                    break;
                case 3:
                    io.foreground_width = 128;
                    break;
            }
            va = (val >> 4) & 3;
            switch(va) {
                case 0:
                    io.foreground_height = 32;
                    break;
                case 1:
                    io.foreground_height = 64;
                    break;
                case 2:
                    printf("\nWARNING BAD FOREGROUND HEIGHT");
                    break;
                case 3:
                    io.foreground_width = 128;
                    break;
            }
            io.foreground_width_mask = io.foreground_width - 1;
            io.foreground_height_mask = io.foreground_height - 1;
            io.foreground_width_mask_pixels = (io.foreground_width * 8) - 1;
            io.foreground_height_mask_pixels = (io.foreground_height * 8) - 1;

            // 64x128, 128x64, and 128x128 are invalid
            u32 total = io.foreground_width * io.foreground_height;
            if (total > 4096) {
                printf("\nWARNING INVALID FOREGROUND HEIGHT AND WIDTH COMBO");
            }
            return; }
        case 17: // window plane horizontal position
            io.window_h_pos = val & 0x1F;
            io.window_RIGHT = (val >> 7) & 1;
            recalc_window();
            return;
        case 18: // window plane vertical postions
            io.window_v_pos = (val & 0x1F);
            io.window_DOWN = (val >> 7) & 1;
            recalc_window();
            return;
        case 19:
            dma.len = (dma.len & 0xFF00) | (val & 0xFF);
            //printf("\nWR DMALEN(13) %04x %02x", dma.len, val);
            return;
        case 20:
            dma.len = (dma.len & 0xFF) | ((val << 8) & 0xFF00);
            //printf("\nWR DMALEN(14) %04x %02x", dma.len, val);
            return;
        case 21:
            // source be 22 bits
            dma.source_address = (dma.source_address & 0xFFFF00) | (val & 0xFF);
            //if (dbg.traces.vdp2) DFT("\nWR DMA ADDR(15) %06x", dma.source_address);
            return;
        case 22:
            dma.source_address = (dma.source_address & 0xFF00FF) | ((val << 8) & 0xFF00);
            //if (dbg.traces.vdp2) DFT("\nWR DMA ADDR(16) %06x", dma.source_address);
            return;
        case 23:
            dma.source_address = (dma.source_address & 0xFFFF) | (((val & 0x3F) << 16) & 0x7F0000);
            dma.mode = (val >> 6) & 3;
            dma.fill_pending = (val >> 7) & 1;
            //if (dbg.traces.vdp) printf("\nMODE! %d FILL PENDING! %d", dma.mode, dma.fill_pending);
            //if (dbg.traces.vdp2) DFT("\nWR DMA ADDR(17) %06x MODE:%d PEND:%d WAIT:%d", dma.source_address, dma.mode, command.pending, dma.fill_pending);
            dma_run_if_ready();
            return;
        case 29: // KGEN debug register, enable debugger
            dbg_break("KGEN debugger", bus->clock.master_cycle_count);
            break;

        case 30: // character for terminal out
            if (val == 0) {
                *term_out_ptr = 0;
                term_out_ptr = &term_out[0];
                printf("[GEN CONSOLE] %s", term_out);
                term_out[0] = 0;
            }
            else {
                u64 sz = (term_out_ptr - (&term_out[0]));
                if (sz > 16382) {
                    printf("\nGEN TERM OVERFLOW! FLUSHING!");
                    *term_out_ptr = 0;
                    term_out_ptr = &term_out[0];
                    printf("[GEN CONSOLE] %s", term_out);
                    term_out[0] = 0;
                }
                *term_out_ptr = val;
                term_out_ptr++;
            }
            break;
        default:
            printf("\nUNHANDLED VDP REG WRITE %02x: %02x", rn, val);
            break;
    }
}

void core::dma_source_inc()
{
    dma.source_address = (dma.source_address & 0xFFFF0000) | ((dma.source_address + 1) & 0xFFFF);
}

void core::dma_load()
{
    io.bus_locked = 1;
    u32 addr = ((dma.mode & 1) << 23) | (dma.source_address << 1);
    u16 data = bus->mainbus_read(addr & 0xFFFFFF, 1, 1, 0, true);
    //printf("\nVDP DMA LOAD ADDR:%04x DATA:%04x LEN:%04x VBLANK:%d DIS_ENABLE:%d LINE:%d cyc:%lld", (u32)addr, (u32)data, (u32)dma.len, bus->clock.vdp.vblank_active, io.enable_display, bus->clock.vdp.vcount, bus->clock.master_cycle_count);
    write_data_port(data, false);
    DBG_EVENT(DBG_GEN_EVENT_DMA_LOAD);

    dma_source_inc();
    if (--dma.len == 0) {
        command.pending = 0;
        io.bus_locked = 0;
    }
    sc_skip = 1; // every-other
}

void core::dma_fill()
{
    DBG_EVENT(DBG_GEN_EVENT_DMA_FILL);
    dma.active = 1;
    switch(command.target) {
        case 1:
            //printf("\nVDP DMA FILL VRAM ADDR:%04x DATA:%02x VBLANK:%d cyc:%lld", command.address, dma.fill_value, bus->clock.vdp.vblank_active, bus->clock.master_cycle_count);
            VRAM_write_byte(command.address, dma.fill_value);
            break;
        case 3:
            //if (dbg.traces.vdp2) DFT("\nVDP DMA FILL CRAM ADDR:%04x DATA:%02x", command.address, dma.fill_value);
            CRAM_write_byte(command.address, dma.fill_value);
            break;
        case 5:
            VSRAM_write_byte(command.address, dma.fill_value);
            break;
        default:
            //if (dbg.traces.vdp) printf("\nDMA FILL UNKNOWN!? target:%d value:%02x addr:%04x cyc:%lld", command.target, dma.fill_value, command.target, bus->clock.master_cycle_count);
            break;
    }
    //tracer.dma->notify({"fill(", hex(target, 1L), ":", hex(address, 5L), ", ", hex(data, 4L), ")"});

    dma_source_inc();
    command.address = (command.address + command.increment) & 0x1FFFF;
    if(--dma.len == 0) {
        command.pending = 0;
        dma.active = 0;
    }
}

void core::dma_copy()
{
    dma.active = 1;
    DBG_EVENT(DBG_GEN_EVENT_DMA_COPY);

    u32 data = VRAM_read_byte(dma.source_address);
    //if (dbg.traces.vdp4) DFT("\nVDP DMA COPY VRAM SRC:%04x DEST:%04x DATA:%04x", (u32)dma.source_address, (u32)command.address, (u32)data);

    VRAM_write_byte(command.address, data);
    //tracer.dma->notify({"copy(", hex(source, 6L), ", ", hex(target, 1L), ":", hex(address, 5L), ", ", hex(data, 4L), ")"});

    dma_source_inc();
    command.address = (command.address + command.increment) & 0x1FFFF;
    if(--dma.len == 0) {
        command.pending = 0;
        dma.active = 0;
    }

}

void core::dma_run_if_ready()
{
    if (!dma.locked) {
        dma.locked = 1;
        if (dma.enable) {
            if (command.pending && !dma.fill_pending) { // Run once YO!
            //while(command.pending && !dma.fill_pending) {
                switch(dma.mode) {
                    case 0: case 1: dma_load(); break;
                    case 2: dma_fill(); break;
                    case 3: dma_copy(); break;
                }
            }
        }
        dma.locked = 0;
    }
}

void core::write_control_port(u16 val, u16 mask)
{
    if (command.latch == 1) { // Finish latching a DMA transfer command
        command.latch = 0;
        command.address = (command.address & 0b11111111111111) | ((val & 7) << 14);
        command.target = (command.target & 3) | ((val >> 4) & 3) << 2;
        //command.ready = ((val >> 6) & 1) | (command.target & 1);
        command.pending = ((val >> 7) & 1) & dma.enable;

        dma.fill_pending = dma.mode == 2; // Wait for fill

        if (command.pending && !dma.fill_pending) {
            switch(dma.mode) {
                case 0:
                case 1:
                    DBG_EVENT(DBG_GEN_EVENT_DMA_LOAD);
                    break;
                case 2:
                    break;
                case 3:
                    DBG_EVENT(DBG_GEN_EVENT_DMA_COPY);
                    break;
            }
        }

        //if (dbg.traces.vdp2) DFT("\nSECOND WRITE. VAL:%04x ADDR:%06x DMA_ENABLE:%d TARGET:%d PENDING:%d WAIT:%d", val, command.address, dma.enable, command.target, command.pending, dma.fill_pending);
        dma_run_if_ready();
        return;
    }

    // command.address.bit(0,13) = data.bit(0,13);
    command.address = (command.address & 0b111100000000000000) | (val & 0b11111111111111);
    //   command.target.bit(0,1)   = data.bit(14,15);
    command.target = (command.target & 0b1100) | ((val >> 14) & 3);
    //printf("   SETTING TARGET TO %d", command.target);

    //printf("\nFIRST WRITE. VAL:%04x ADDR:%06x TARGET:%d PENDING:%d", val, command.address, command.target, command.pending);
    //  VAL:c000 ADDR:00c000
    /*if ((val == 0xc000) && (command.address == 0xc000) && (command.target == 3)) {
        printf("\nDID IT HERE AT CYCLE:%lld", bus->clock.master_cycle_count);
    }*/

    if (((val >> 14) & 3) != 2) { // It'll equal 2 if we're writing a register
        command.latch = 1;
        return;
    }

    write_vdp_reg((val >> 8) & 0x1F, val & 0xFF);
}

void core::schedule_frame(void *ptr, u64 key, u64 clock, u32 jitter)
{
    auto *th = static_cast<core *>(ptr);
    const u64 cur = clock - jitter;
    const u64 next_frame = cur + th->bus->clock.timing.frame.cycles_per;
    th->bus->scheduler.only_add_abs_w_tag(next_frame, 0, th, &schedule_frame, nullptr, 2);
}


u16 core::read_control_port(u16 old, bool has_effect)
{
    command.latch = 0;
    //printf("\nREAD VDP CONTROL PORT. FILL PEND?%d", dma.fill_pending);
    // Ares 360c  1000001100
    // Me   3688  1010001000

    u16 v = bus->PAL;
    v |= command.pending << 1; // no DMA yet
    v |= bus->clock.vdp.hblank_active << 2;
    v |= (bus->clock.vdp.vblank_active || (1 ^ io.enable_display)) << 3;
    v |= (bus->clock.vdp.field && io.interlace_field) << 4;
    v |= sprite_collision << 5;
    v |= sprite_overflow << 6;
    //<< 5; SC: 1 = any two sprites have non-transparent pixels overlapping. Used for pixel-accurate collision detection.
    //<< 6; SO: 1 = sprite limit has been hit on current scanline. i.e. 17+ in 256 pixel wide mode or 21+ in 320 pixel wide mode.
    //<< 7; VI: 1 = vertical interrupt occurred.
    v |= io.vblank << 7;

    v |= fifo_full() << 8;
    v |= fifo_empty() << 9;

    // Open bus bits 10-15
    //v |= (old & 0xFC00);
    v |= 0b0011010000000000;

    if (has_effect && !io.mode5) {
        //assert(1==2);
        //io.vblank = 0;
        //genesis_m68k_vblank_irq(0);
        //printf("\nWHAT!?!");
    }
    //if (dbg.traces.vdp3) DFT("\nRD VDP CP DATA:%04x", v);
    return v;
}

u16 core::read_data_port(u16 old, u16 mask)
{
    command.latch = 0;
    command.ready = 0;
    u16 addr, data;
    switch(command.target) {
        case 0: // VRAM read
            addr = (command.address & 0x1FFFE) >> 1;
            data = VRAM_read(addr, 1);
            command.address = (command.address + command.increment) & 0x1FFFF;
            return data;
        case 4: // VSRAM read
            addr = (command.address >> 1) & 63;
            data = VSRAM_read(addr);
            command.address = (command.address + command.increment) & 0x1FFFF;
            return data;
        case 8: // CRAM read
            addr = (command.address >> 1) & 0x3F;
            data = CRAM_read(addr);
            command.address = (command.address + command.increment) & 0x1FFFF;
            return ((data & 7) << 1) | (((data >> 3) & 7) << 5) | (((data >> 6) & 7) << 9);
    }

    //if (dbg.traces.vdp) printf("\nUnknown read port target %d", command.target);
    return 0;
}

void core::write_data_port(u16 val, bool is_cpu)
{
    //if (dbg.traces.vdp && is_cpu) dbg_printf("\nVDP data port write val:%04x is_cpu:%d target:%d", val, is_cpu, command.target);

    command.latch = 0;
    //if (dbg.traces.vdp) printf("\nWRITE VDP DATA PORT %04x cyc:%lld", val, bus->clock.master_cycle_count);

    if(dma.fill_pending) {
        dma.fill_pending = 0;
        dma.fill_value = val >> 8;
        DBG_EVENT(DBG_GEN_EVENT_DMA_FILL);
        //if (dbg.traces.vdp3) DFT("\nWR VDP DATA, FILL START");
        /*if (bus->clock.master_cycle_count == 53021297) {
            printf("\nPC: %06x", this->m68k.regs.PC);
            //dbg_break("Last good fill?", bus->clock.master_cycle_count);
        }*/
        //printf("\nFILL START PC:%06x cyc:%lld", this->m68k.regs.PC, bus->clock.master_cycle_count);
    }

    u32 addr;

    if (command.target == 1) { // VRAM write
        //addr = (command.address << 1) & 0x1FFFE;
        addr = (command.address & 0x1FFFE) >> 1;
        if (command.address & 1) val = (val >> 8) | (val << 8);
        //if (dbg.traces.vdp3) DFT("\nWR VDP DP VRAM DEST:%04x DATA:%04x", addr, val);
        VRAM_write(addr, val);
        command.address = (command.address + command.increment) & 0x1FFFF;
        dma_run_if_ready();
        return;
    }

    if (command.target == 5) { // VSRAM write
        addr = (command.address >> 1) & 0x3F;
        //data format: ---- --yy yyyy yyyy
        addr = (addr % 40);
        //if (dbg.traces.vdp3) DFT("\nWR VDP DP VSRAM DEST:%04x DATA:%04x", addr, val);
        VSRAM[addr] = val & 0x3FF;
        command.address = (command.address + command.increment) & 0x1FFFF;
        return;
    }

    if (command.target == 3) { // CRAM write
        addr = (command.address >> 1) & 0x3F;
        //if (dbg.traces.vdp3) DFT("\nWR VDP DP CRAM DEST:%04x DATA:%04x", addr, val);
        u32 v = (((val >> 1) & 7) << 0) | (((val >> 5) & 7) << 3) | (((val >> 9) & 7) << 6);
        CRAM_write(addr, v);
        command.address = (command.address + command.increment) & 0x1FFFF;
        return;
    }
    printf("\nUnknown data port write! Target:%d is_cpu:%d addr:%04x PC:%06x  cyc:%lld", command.target, is_cpu, command.address, bus->m68k.regs.PC, bus->clock.master_cycle_count);
    //dbg_break("Bad port write", bus->clock.master_cycle_count);
}

u16 core::mainbus_read(u32 addr, u16 old, u16 mask, bool has_effect)
{
    /* 110. .000 .... .... 000m mmmm */
    if (((addr & 0b110000000000000000000000) ^ 0b001001110000000011100000) != 0b111001110000000011100000) {
        printf("\nERROR LOCK UP VDP!!!!");
        return 0;
    }

    addr = (addr & 0x1F) | 0xC00000;

    switch(addr) {
        case 0xC00000: // VDP data port
        case 0xC00002: // VDP data port
            return read_data_port(old, mask) & mask;
        case 0xC00004: // VDP control
        case 0xC00006: // VDP control
            return read_control_port(old, has_effect) & mask;
        case 0xC00008:
        case 0xC0000A:
        case 0xC0000C:
        case 0xC0000E:
            return read_counter() & mask;
    }

    //printf("\nBAD VDP READ addr:%06x cycle:%lld", addr, bus->clock.master_cycle_count);
    //gen_test_dbg_break("vdp_mainbus_read");
    return old;
}

u8 core::z80_read(u32 addr, u8 old, bool has_effect)
{
    if ((addr >= 0x7F00) && (addr < 0x7F20)) {
        return mainbus_read((addr & 0x1E) | 0xC00000, old, 0xFF, has_effect) & 0xFF;
    }
    return 0xFF;
}

void core::z80_write(u32 addr, u8 val)
{
    if ((addr >= 0x7F00) && (addr < 0x7F20)) {
        mainbus_write((addr & 0x1E) | 0xC00000, val, 0xFF);
    }
}

void core::mainbus_write(u32 addr, u16 val, u16 mask)
{
    if (((addr & 0b110000000000000000000000) ^ 0b001001110000000011100000) != 0b111001110000000011100000) {
        printf("\nERROR LOCK UP VDP!!!!");
        return;
    }

    // Duplicate!
    if (mask == 0xFF) {
        val = (val & 0xFF) | (val << 8);
    }
    else if (mask == 0xFF00) {
        val = (val & 0xFF00) | (val >> 8);
    }

    addr = (addr & 0x1F) | 0xC00000;
    switch(addr) {
        case 0xC00000: // VDP data port
        case 0xC00002:
            write_data_port(val, true);
            return;
        case 0xC00004: // VDP control
        case 0xC00006:
            write_control_port(val, mask);
            return;
        case 0xC00008: // H/V READ ONLY
        case 0xC0000A:
        case 0xC0000C:
        case 0xC0000E:
            return;
        case 0xC00010: // SN76489 write
        case 0xC00012:
        case 0xC00014:
        case 0xC00016:
            if ((mask & 0xFF) == 0) return; // Byte writes to wrong address have no effect
            bus->psg.write_data(val & 0xFF);
            return;
        case 0xC00018:
        case 0xC0001A:
            printf("\nDEBUG REG ADDR WRITE!");
            return;
        case 0xC0001C: // debug reg
        case 0xC0001E:
            printf("\nDEBUG REG WRITE!");
            return;
    }
    printf("\nBAD VDP WRITE addr:%06x val:%04x cycle:%lld", addr, val, bus->clock.master_cycle_count);
    bus->test_dbg_break("vdp_mainbus_write");
}

void core::reset()
{
    io.h40 = true; // H40 mode to start
    bus->clock.vdp.hblank_active = bus->clock.vdp.vblank_active = 0;
    bus->clock.vdp.hcount = 0;
    bus->clock.vdp.vcount = -1;
    cur_output = static_cast<u16 *>(display->output[display->last_written ^ 1]);
    bus->clock.current_back_buffer = display->last_written ^ 1;
    bus->clock.current_front_buffer = display->last_written;
    io.enable_display = 0;
    display->scan_x = display->scan_y = 0;
    set_clock_divisor();
}

void core::get_line_hscroll(u32 line_num, u32 *planes)
{
    u32 addr = io.hscroll_addr;
    switch(io.hscroll_mode) {
        case 0: // full-screen
            break;
        case 1: // invalid!
            printf("\nWARN INVALID HSCROLL MODE 1");
            return;
        case 2: // 8-pixel rows
            line_num >>= 3; // We only care about every 8 lines
            addr += line_num << 4; // Every 16 words
            break;
        case 3: // per-line scrolls
            addr += line_num << 1;
            break;
    }
    // - 1 & 1023 ^ 1023 should convert from negative to positive here. 0 = FFFF = 0 for instance. 1 = 0 = FFFF.
    planes[PLANE_A] = VRAM_read(addr, 0);
    planes[PLANE_B] = VRAM_read(addr + 1, 0);
}

void core::get_vscrolls(int column, u32 *planes)
{
    if (io.vscroll_mode == 0) {
        //planes[0] = VSRAM[0];
        //planes[1] = VSRAM[1];
        planes[0] = fetcher.vscroll_latch[0];
        planes[1] = fetcher.vscroll_latch[1];
        return;
    }
    if (column == -1) { // TODO: correctly simulate this
        if (latch.h40) {
            u32 v = VSRAM[19] & VSRAM[39];
            planes[0] = v;
            planes[1] = v;
            return;
        }
        else {
            planes[0] = 0;
            planes[1] = 0;
            return;
        }
    }
    //u32 col = (u32)column << 1;
    u32 col = static_cast<u32>(column) << 1;
    //assert(col<21);
    planes[0] = VSRAM[col];
    planes[1] = VSRAM[col+1];
}

static int fetch_order[4] = { 1, 0, 3, 2 };

void core::render_to_ringbuffer(u32 plane, slice *slice, u32 pattern_pos, u32 pattern_max)
{
    const u32 epos = pattern_max + 1;
    u32 *tail = &ringbuf[plane].tail;
    i32 *num = &ringbuf[plane].num;
    while(pattern_pos < epos) {
        auto *b = &ringbuf[plane].buf[*tail];
        const u32 c = slice->pattern[pattern_pos] & 63;
        const u32 priority = (slice->pattern[pattern_pos] >> 7) & 1;
        b->bg_table_y = slice->v;
        b->bg_table_x = slice->h + pattern_pos;
        if (c != 0) {
            b->has = 1;
            b->color = c;
        } else {
            b->has = 0;
            b->color = 0;
        }
        b->priority = priority;
        pattern_pos++;
        *tail = (*tail + 1) & 31;
        (*num)++;
    }
}


u32 core::window_transitions_off() const
{
    u32 row = bus->clock.vdp.vcount >> 3;
    if ((row >= window.top_row) && (row < window.bottom_row)) return 0;
    if (io.window_RIGHT) return 0; // Window only transitions OFF if left-to-middle
    if (window.right_col == 0) return 0; // Window doesn't transition OFF if hpos == 0
    int col = fetcher.column;

    return window.right_col == static_cast<u32>(col);
}
u32 core::window_transitions_on() const
{
    const u32 row = bus->clock.vdp.vcount >> 3;
    if ((row >= window.top_row) && (row < window.bottom_row)) return 0;
    if (!io.window_RIGHT) return 0; // Window only transitions OFF if left-to-middle
    if (window.left_col == 0) return 0; // Window doesn't transition ON if hpos == 0
    const int col = fetcher.column;
    return window.left_col == static_cast<u32>(col);
}


bool core::in_window() const
{
    u32 col = fetcher.column;
    u32 row = bus->clock.vdp.vcount >> 3;
    return (((col >= window.left_col) && (col < window.right_col)) ||
            ((row >= window.top_row) && (row < window.bottom_row)));
}

void core::fetch_slice(u32 nt_base_addr, u32 col, u32 row, slice *slice, u32 plane_num)
{
    // Fetch a 16-pixel slice of a nametable to a provided struct
    u32 tile_row = (row >> 3) & io.foreground_height_mask;
    u32 tile_col = (col >> 3) & io.foreground_width_mask;
    u32 fine_row = row & 7;
    u32 foreground_width = io.foreground_width;
    if ((plane_num == 0) && in_window()) {
        // Replace with a window fetch!
        slice->plane = 2;
        nt_base_addr = window.nt_base;
        tile_row = bus->clock.vdp.vcount >> 3;
        tile_col = fetcher.column << 1;
        fine_row = bus->clock.vdp.vcount & 7;
        foreground_width = latch.h40 ? 64 : 32;
    }
    else slice->plane = plane_num;
    slice->v = row & io.foreground_height_mask_pixels;
    slice->h = (tile_col << 3) & io.foreground_width_mask_pixels;
    const u32 foreground_width_mask = foreground_width - 1;
    u32 tile_data = VRAM_read(nt_base_addr + (tile_row * foreground_width) + tile_col, 0);
    u32 vflip = (tile_data >> 12) & 1;
    u32 tile_addr = (tile_data & 0x7FF) << 5;
    tile_addr += (vflip ? (7 - fine_row) : fine_row) << 2;

    const u8 *ptr = reinterpret_cast<u8 *>(VRAM) + tile_addr;
    u32 pattern_idex = 0;
    u32 hflip = (tile_data >> 11) & 1;
    u32 palette = ((tile_data >> 13) & 3) << 4;
    u32 priority = ((tile_data >> 15) & 1) << 7;
    for (u32 i = 0; i < 4; i++) {
        const u32 offset = hflip ? 3 - i : i;
        const u8 px2 = ptr[fetch_order[offset]];
        u8 px[2];
        px[hflip] = px2 >> 4;
        px[hflip ^ 1] = px2 & 15;
        for (unsigned char j : px) {
            slice->pattern[pattern_idex++] = priority | (j ? (palette | j) : 0);
        }
    }

    tile_col = (tile_col + 1) & foreground_width_mask;
    tile_data = VRAM_read(nt_base_addr + (tile_row * foreground_width) + tile_col, 0);

    tile_addr = (tile_data & 0x7FF) << 5;
    vflip = (tile_data >> 12) & 1;
    tile_addr += (vflip ? (7 - fine_row) : fine_row) << 2;

    ptr = reinterpret_cast<u8 *>(VRAM) + tile_addr;
    hflip = (tile_data >> 11) & 1;
    palette = ((tile_data >> 13) & 3) << 4;
    priority = ((tile_data >> 15) & 1) << 7;
    for (u32 i = 0; i < 4; i++) {
        u32 offset = hflip ? 3 - i : i;
        u8 px2 = ptr[fetch_order[offset]];
        u8 px[2];
        px[hflip] = px2 >> 4;
        px[hflip ^ 1] = px2 & 15;
        for (unsigned char j : px) {
            slice->pattern[pattern_idex++] = priority | (j ? (palette | j) : 0);
        }
    }
}

struct spr_out {
    u32 hpos, vpos;
    u32 hsize, vsize;
    u32 priority, palette;
    u32 vflip, hflip;
    u32 gfx; // tile # in VRAM
};

u32 core::fetch_sprites(spr_out *sprites, u32 vcount, u32 sprites_max_on_line)
{
    const u32 compare_y = vcount + 128;
    u16 *sprite_table_ptr = VRAM + io.sprite_table_addr;
    u32 num = 0;
    u16 next_sprite = 0;
    u32 tiles_on_line = 0;
    const u32 sprites_total_max = latch.h40 ? 80 : 64;
    for (u32 i = 0; i < sprites_total_max; i++) {
        const u16 *sp = sprite_table_ptr + (4 * next_sprite);
        next_sprite = sp[1] & 127;

        sprites->vpos = sp[0] & 0x1FF;
        sprites->vsize = ((sp[1] >> 8) & 3) + 1;

        const u32 sprite_max = sprites->vpos + (sprites->vsize * 8);
        if ((compare_y >= sprites->vpos) && (compare_y < sprite_max)) {
            sprites->hpos = sp[3] & 0x1FF;
            if (sprites->hpos == 0) {
                if (num==0) continue; // Masking not effective if first sprite
                //if (line.dot_overflow) continue; // Except it IS effective if dot overflow happened
                return num;
            }
            sprites->hsize = ((sp[1] >> 10) & 3) + 1;
            sprites->hflip = (sp[2] >> 11) & 1;
            sprites->vflip = (sp[2] >> 12) & 1;
            sprites->palette = ((sp[2] >> 13) & 3) << 4;
            sprites->gfx = sp[2] & 0x7FF;
            sprites->priority = (sp[2] >> 15) & 1;

            tiles_on_line += sprites->hsize;

            sprites++;
            num++;
        }
        if ((tiles_on_line >= line.sprite_patterns) || (num >= sprites_max_on_line)) break;
        if ((next_sprite == 0) || (next_sprite >= sprites_total_max)) break;
    }
    //u32 delay = sprites_max_on_line - num

    return num;
}

static inline u32 sprite_tile_index(u32 tile_x, u32 tile_y, u32 hsize, u32 vsize, u32 hflip, u32 vflip)
{
    const u32 ty = vflip ? (vsize-1) - tile_y : tile_y;
    const u32 tx = hflip ? (hsize-1) - tile_x : tile_x;
    return (vsize * tx) + ty;
}

void core::render_sprites()
{
    // Render sprites for the next line!
    const u32 vc = (bus->clock.vdp.vcount == (bus->clock.timing.frame.scanlines_per-1)) ? 0 : bus->clock.vdp.vcount + 1;
    if (vc > bus->clock.vdp.bottom_rendered_line) return;

    memset(&sprite_line_buf[0], 0, sizeof(sprite_line_buf));

    spr_out sprites[20]; // Up to 20 can be visible on a line!

    const u32 num_sprites = fetch_sprites(&sprites[0], vc, line.sprite_mappings);

    const i32 xmax = latch.h40 ? 320 : 256;

    const u32 cur_y = vc + 128;
    u32 total_tiles = 0;
    u32 max_tiles = latch.h40 ? 40 : 32;
    u32 dot_overflow_amount = latch.h40 ? 40 : 32;
    u32 dot_overflow_triggered = 0;

    for (u32 spr=0; spr < num_sprites; spr++) {
        // Render sprite from left to right
        spr_out *sp = &sprites[spr];
        //u32 x_right = 128 + (latch.h40 ? 320 : 256);
        //u32 tile_stride = sp->vsize;

        u32 y_min = sp->vpos;
        u32 sp_line = cur_y - y_min;
        u32 fine_row = sp_line & 7;
        u32 sp_ytile = sp_line >> 3;
        u32 tile_y_addr_offset = (sp->vflip ? (7 - fine_row) : fine_row) << 2;

        //if (bus->clock.vdp.vcount == 80) printf("\nSPRITE NUM:%d HSIZE: %d VSIZE:%d", spr, sp->hsize, sp->vsize);
        for (u32 tx = 0; tx < sp->hsize; tx++) { // Render across tiles...
            const u32 tile_num = sp->gfx + sprite_tile_index(tx, sp_ytile, sp->hsize, sp->vsize, sp->hflip, sp->vflip);
            const u32 tile_addr = (tile_num << 5) + tile_y_addr_offset;
            i32 x = (static_cast<i32>(sp->hpos) - 128) + (static_cast<i32>(tx) * 8);
            const u8 *ptr = reinterpret_cast<u8 *>(VRAM) + tile_addr;
            for (u32 byte = 0; byte < 4; byte++) { // 4 bytes per 8 pixels
                const u32 offset = sp->hflip ? 3 - byte : byte;
                const u8 px2 = ptr[fetch_order[offset]];
                u32 px[2];
                px[sp->hflip] = px2 >> 4;
                px[sp->hflip ^ 1] = px2 & 15;
                for (unsigned int v : px) {
                    if ((x >= 0) && (x < xmax)) {
                        //if (v != 0) { // If there's not already a sprite pixel here, and ours HAS something other than transparent...
                            auto *p = &sprite_line_buf[x];
                            if ((p->has_px) && ((p->color & 15) != 0)) {
                                sprite_collision = 1;
                            }
                            else {
                                p->has_px = 1;
                                p->priority = sp->priority;
                                p->color = sp->palette | v;
                            }
                        //}
                    }
                    x++;
                }
            }
            total_tiles++;
            if (total_tiles >= dot_overflow_amount) {
                dot_overflow_triggered = 1;
            }
            if (total_tiles >= line.sprite_patterns) {
                line.dot_overflow = dot_overflow_triggered;
                return;
            }
        }
    }
    line.dot_overflow = dot_overflow_triggered;
}

void core::output_16()
{
    // Point of this function is to render 16 pixels outta the ringbuffer, to the pixelbuffer. Also combine with sprites and priority.
    // At this point, there will be 16-31 pixels in the ring buffer.
    const u32 wide = latch.h40 ? 4 : 5;
    const u32 xmax = latch.h40 ? 320 : 256;
    for (u32 num = 0; num < 16; num++) {
        auto *ring0 = ringbuf[PLANE_A].buf + ringbuf[PLANE_A].head;
        auto *ring1 = ringbuf[PLANE_B].buf + ringbuf[PLANE_B].head;
        ringbuf[PLANE_A].head = (ringbuf[PLANE_A].head + 1) & 31;
        ringbuf[PLANE_B].head = (ringbuf[PLANE_B].head + 1) & 31;
        ringbuf[PLANE_A].num--;
        ringbuf[PLANE_B].num--;
        debug.px_displayed[bus->clock.current_back_buffer][0][(ring0->bg_table_y * 32) + (ring0->bg_table_x >> 5)] |= 1 << (ring0->bg_table_x & 31);
        debug.px_displayed[bus->clock.current_back_buffer][1][(ring1->bg_table_y * 32) + (ring1->bg_table_x >> 5)] |= 1 << (ring1->bg_table_x & 31);
        //printf("BACKBUFFER %d", bus->clock.current_back_buffer);
        if (line.screen_x >= xmax) continue;

        // We've got out ringbuffer. Now get our sprite pixel...
        auto *sprite = sprite_line_buf + line.screen_x;

        if (!bus->v_opts.vdp.enable_A) {
            ring0->has = ring0->color = ring0->priority = 0;
        }
        if (!bus->v_opts.vdp.enable_B) {
            ring1->has = ring1->color = ring1->priority = 0;
        }
        if (!bus->v_opts.vdp.enable_sprites) {
            sprite->has_px = sprite->color = sprite->priority = 0;
        }


#define PX_NONE 0
#define PX_A 1
#define PX_B 2
#define PX_SP 3
#define PX_BG 4
        // Using logic from Ares here.
        // I originally had my own. Then I went to do shadow&highlight, and use Ares logic.
        // I couldn't get it to work. Then I tried making my own again and realized I was just reproducing Ares but
        //  understanding it, so gave in.

#define solid_sprite (sprite->has_px && (sprite->color & 15))
#define above_sprite (solid_sprite && sprite->priority)
#define solid(plane) (ring##plane->has && (ring##plane->color & 15))
#define above(plane) (solid(plane) && ring##plane->priority)

        u32 bg = PX_NONE;
        u32 fg = PX_NONE;
        if (above(0) || solid(0) && (!above(1))) {
            bg = PX_A;
        }
        else {
            if (solid(1)) {
                bg = PX_B;
            }
            else
            {
                bg = PX_BG;
            }
        }
        if (above_sprite || (solid_sprite && (!above(1)) && (!above(0))))
            fg = PX_SP;
        else
            fg = bg;

        u32 pixel = fg; // Output pixel will now be sprite or backgorund color or A or B
        u32 mode = 1; // 0 = shadow, 1 = normal, 2 = hightlight


        if(io.enable_shadow_highlight) {
            mode = ring0->priority || ring1->priority;
            if(fg == PX_SP) switch(sprite->color) {
                    case 0x0E:
                    case 0x1E:
                    case 0x2E: mode  = 1; break;
                    case 0x3E: mode += 1; pixel = bg; break;
                    case 0x3F: mode  = 0; pixel = bg; break;
                    default:  mode |= sprite->priority; break;
                }
        }
        u32 src = 0;
        u32 final_color = 0;
        switch(pixel) {
            case PX_A:
                final_color = ring0->color;
                break;
            case PX_B:
                final_color = ring1->color;
                src = 1;
                break;
            case PX_SP:
                final_color = sprite->color;
                src = 2;
                break;
            case PX_BG:
                final_color = io.bg_color;
                src = 3;
                break;
            default:
                assert(1==2);
                break;
        }

        assert(final_color < 64);
        final_color = CRAM[final_color] | (mode << 10) | (src << 12);
        for (u32 i = 0; i < wide; i++) {
            cur_output[cur_pixel] = final_color;

            cur_pixel++;
            assert(cur_pixel < (1280*240));
        }
        line.screen_x++;
    }
}

void core::render_16_more()
{
    if (bus->clock.vdp.vcount > bus->clock.vdp.bottom_rendered_line) return;
    // Fetch a slice
    u32 vscrolls[2];
    get_vscrolls(fetcher.column, vscrolls);
    const u32 plane_wrap = io.foreground_width_mask_pixels;

    // Dump to ringbuffer
    slice slice;
    for (u32 plane = 0; plane < 2; plane++) {
        if ((plane == 0) && window_transitions_on() && (fetcher.fine_x[0] != 0)) {
            const u32 head = ringbuf[0].head;
            ringbuf[0].head = (head + (fetcher.fine_x[0] + 1)) & 31;
        }

        fetch_slice(plane == 0 ? io.plane_a_table_addr : io.plane_b_table_addr, fetcher.hscroll[plane], vscrolls[plane]+bus->clock.vdp.vcount, &slice, plane);
        //u32 col = fetcher.column;
        fetcher.hscroll[plane] = (fetcher.hscroll[plane] + 16) & plane_wrap;

        // If we just went from window to regular and (xscroll % 15) != 0, glitch time!
        if ((plane == 0) && window_transitions_off() && (fetcher.fine_x[0] != 0)) {
            render_to_ringbuffer(plane, &slice, 15 - fetcher.fine_x[0], 15);
        }

        render_to_ringbuffer(plane, &slice, 0, 15);
    }
    fetcher.hscroll[2] += 16;
    fetcher.column++;

    // Render 16 pixels from ringbuffer
    output_16();
}


void core::render_left_column()
{
    // Fetch left column. Discard xscroll pixels, render the rest.
    //printf("\nRENDER LEFT COLUMN line:%d", bus->clock.vdp.vcount);
    if (bus->clock.vdp.vcount > bus->clock.vdp.bottom_rendered_line) return;
    const u32 plane_wrap = (8 * io.foreground_width) - 1;

    u32 vscrolls[2];
    get_vscrolls(-1, vscrolls);
    // Now draw 0-15 pixels of each one out to the ringbuffer...
    slice slice;
    for (u32 pl = 0; pl < 2; pl++) {
        const u32 plane = pl;
        const u32 hs = fetcher.hscroll[plane];
        fetch_slice(plane == 0 ? io.plane_a_table_addr : io.plane_b_table_addr, hs & 0x3F0, vscrolls[plane]+bus->clock.vdp.vcount, &slice, plane);
        const u32 fine_x = fetcher.fine_x[plane];
        if ((((plane == 0) && (!in_window())) || (plane == 1)) && (fine_x > 0)) {
            render_to_ringbuffer(plane, &slice, 16 - fine_x, 15);
        }
        fetcher.hscroll[plane] = (fetcher.hscroll[plane] + fine_x) & plane_wrap;
    }

    // Now that the initial data and pixel load is done, do one more regular one!
    render_16_more();
}

void core::do_cycle()
{
    bus->timing.vdp_cycles++;
    u32 run_dma = 0;

    display->scan_x+=2;
    bus->clock.vdp.hcount+=2;

    sc_slot++;
    // Do FIFO if this is an external slot
    slot_kinds sk = slot_array[sc_array][sc_slot];
    switch(sk) {
        case slot_sprite_pattern:
            line.sprite_patterns += io.enable_display;
            break;
        case slot_sprite_mapping:
            line.sprite_mappings += io.enable_display;
            break;
        case slot_external_access:
            sc_skip++;
            run_dma = sc_skip & 1;
            break;
        case slot_layer_b_pattern_first_trigger:
            render_left_column();
            break;
        case slot_layer_b_pattern_trigger:
            render_16_more();
            break;
    }

    if (!latch.h40) { // h32
        switch(sc_slot) {
            case 6:
                hblank(0);
                break;
            case 164:
                hblank(1);
                break;
        }
    }
    else {
        switch(sc_slot) {
            case 3:
                hblank(0);
                break;
            case 197:
                hblank(1);
                break;
        }
    }
    run_dma |= (io.enable_display ^ 1);
    if (run_dma) {
        dma_run_if_ready();
    }
}

}
//
// Created by . on 6/18/25.
//

#include <cstring>
#include <cstdio>

#include "helpers/color.h"
#include "helpers/debug.h"
#include "helpers/debugger/debugger.h"

#include "huc6270.h"

/*
 * OK SO
 * this is "driven" by the VCE.
 * each clock just draws a pixel, no worry about timing.
 * BUT! it's a state machine that expects things to happen at a time.
 * HSW is horixontal sync window, it expects hsync within this time. if it doesn't get it it just moves on
 * if it gets it, the window ends, it moves on
 * if it gets it OUTSIDE THIS WINDOW, the whole line is aborted and we move to next
 * vsw works the same way
 * VCE gives these at fixed times, so...yeah.
 */


namespace HUC6270 {

void chip::write_VRAM(u32 addr, u16 val)
{
    DBG_EVENT(dbg.events.WRITE_VRAM);
    if (addr < 0x8000)
        VRAM[addr] = val;
}

void chip::setup_new_frame()
{
    //printf("\n\n6270 NEW FRAME line:%d line compare: %d", regs.y_counter -64, regs.y_counter);
    regs.yscroll = io.BYR.u;
    regs.next_yscroll = regs.yscroll;
}

void chip::eval_sprites() {
    static constexpr u32 sprite_widths_sprs[2] = {1, 2};
    static constexpr u32 sprite_height_sprs[4] = {1, 2, 2, 4};
    static constexpr u32 sprite_widths_px[2] = {16, 32};
    static constexpr u32 sprite_heights_px[4] = {16, 32, 64, 64};
    sprites.num_tiles_on_line = 0;
    for (u32 sn = 0; sn < 64; sn++) {
        u16 *spr = SAT + (sn << 2);
        i32 sy = (spr[0] & 0x3FF);
        if (sy > sprites.y_compare) continue;
        u32 cgy = (spr[3] >> 12) & 3;
        u32 height = sprite_heights_px[cgy];

        u32 sprite_line = sprites.y_compare - sy;
        if (sprite_line >= height) continue;
        u32 cgx = (spr[3] >> 8) & 1;
        u32 width = sprite_widths_px[cgx];
        i32 sx = (spr[1] & 0x3FF) - 32;
        u32 tile_index = (spr[2] & 0x7FE) >> 1;
        u32 palette = spr[3] & 15;
        u32 spbg = (spr[3] >> 7) & 1;
        u32 xflip = (spr[3] >> 11) & 1;
        u32 yflip = (spr[3] >> 15) & 1;
        //xflip = yflip = 0;

        if(width == 32)
            tile_index &= ~0x01;

        if(height == 32)
            tile_index &= ~0x02;
        else if(height == 64)
            tile_index &= ~0x06;
        u32 pat_addr = tile_index << 6;

        if (yflip) sprite_line = (height - 1) - sprite_line;

        u32 vertical_sprite_num = sprite_line >> 4;
        // 64 words per sprite
        u32 words_per_line = 128;
        u32 line_in_sprite = sprite_line & 15;
        u32 addr = pat_addr + (words_per_line * vertical_sprite_num) + line_in_sprite;
        if (xflip) {
            addr += 64 * (sprite_widths_sprs[cgx] - 1);
        }

        for (i32 x = 0; x < width; x += 16) {
            if (sprites.num_tiles_on_line == 16) {
                io.STATUS.OR = 1;
                update_irqs();
                break;
            }
            auto &tile = sprites.tiles[sprites.num_tiles_on_line++];
            tile.num_left = 16;
            tile.x = sx + x;
            tile.original_num = sn;
            tile.triggered = 0;
            tile.priority = spbg; // TODO: verify this isn't !spbg?
            tile.palette = palette;

            // Fetch from +0, +16, +32, +48
            u32 plane0 = read_VRAM(addr);
            u32 plane1 = read_VRAM((addr+16) & 0xFFFF);
            u32 plane2 = read_VRAM((addr+32) & 0xFFFF);
            u32 plane3 = read_VRAM((addr+48) & 0xFFFF);

            for (u32 i = 0; i < 16; i++) {
                if (xflip) tile.pattern_shifter >>= 4;
                else tile.pattern_shifter <<= 4;
                u64 data = (plane0 & 1) << 0;
                data |= (plane1 & 1) << 1;
                data |= (plane2 & 1) << 2;
                data |= (plane3 & 1) << 3;
                plane0 >>= 1;
                plane1 >>= 1;
                plane2 >>= 1;
                plane3 >>= 1;
                if (xflip) data <<= 60;
                tile.pattern_shifter |= data;
            }

            if (xflip) addr -= 64;
            else addr += 64;

            if (tile.x < 0) {
                i32 num_to_left = 0 - tile.x;
                tile.pattern_shifter >>= 4 * num_to_left;
                tile.num_left -= num_to_left;
                tile.x = 0;
            }
        }
        if (sprites.num_tiles_on_line >= 16) break;
    }
}

void chip::setup_new_line() {
    if ((timing.v.state == S_wait_for_display) && (timing.v.counter == 1)) {
        regs.y_counter = 63;
    }
    if (timing.v.state == S_display || ((timing.v.state == S_wait_for_display) && (timing.v.counter == 1))) {
        //printf("\nLINE %lld Y SCROLL:%d", events_view_get_current_line(dbg.events.view), regs.next_yscroll);
        regs.yscroll = regs.next_yscroll;
        regs.next_yscroll = regs.yscroll + 1;
        latch.sprites_on = io.CR.SB;
        latch.bg_on = io.CR.BB;
        sprites.y_compare++;
        regs.first_render = 1;
        regs.draw_clock = 0;
        bg.x_tile = (io.BXR.u >> 3) & bg.x_tiles_mask;
        bg.y_tile = (regs.yscroll >> 3) & bg.y_tiles_mask;
        pixel_shifter.num = 0;

        eval_sprites();
    }
    regs.y_counter++;
    // TODO: this stuff
    timing.v.counter--;
}


void chip::new_h_state(states st)
{
    timing.h.state = st;
    switch(st) {
        case S_sync_window:
            timing.h.counter = (io.HSW+1) << 3;
            //printf("\nSYNC WINDOW %d", timing.h.counter);
            if (timing.v.counter < 1) {
                new_v_state(static_cast<states>((static_cast<i32>(timing.v.state) + 1) & 3));
            }
            break;
        case S_wait_for_display:
            timing.h.counter = (io.HDS+1) << 3;
            //printf("\nWAIT_DISPLAY %d", timing.h.counter);
            break;
        case S_display:
            //printf("\nH.S_DISPLAY START LINE:%lld CYCLE:%lld", events_view_get_current_line(dbg.events.view), events_view_get_current_line_pos(dbg.events.view));
            hblank(false);
            regs.x_counter = 0;
            timing.h.counter = (io.HDW+1) << 3;
            break;
        case S_wait_for_sync_window:
            setup_new_line();
            hblank(true);
            timing.h.counter = (io.HDE+1) << 3;
            update_RCR(this, 0, 0, 0);
            //printf("\nWAIT SYNC WINDOW %d", timing.h.counter);
            break;
    }
}

void chip::new_v_state(states st)
{
    timing.v.state = st;
    switch(st) {
        case S_sync_window:
            vblank(false);
            timing.v.counter = io.VSW + 1;
            break;
        case S_wait_for_display:
            vblank(false);
            setup_new_frame();
            timing.v.counter = io.VDS + 2;
            break;
        case S_display:
            //printf("\nV.H6S DISPLAY START LINE %lld CYCLE:%lld", events_view_get_current_line(dbg.events.view), events_view_get_current_line_pos(dbg.events.view));
            timing.v.counter = io.VDW.u + 2; // It will get decremented by setup_new_line()
            sprites.y_compare = 63;
            regs.blank_line = 1;
            break;
        case S_wait_for_sync_window:
            vblank(true);
            regs.px_out = 0x100;
            timing.v.counter = io.VCR;
            break;
    }
}

void chip::force_new_frame()
{
    //printf("\nbadly timed vsync!!! %d", timing.v.state);
}

void chip::force_new_line()
{
    //printf("\nbadly timed hsync!? state:%d  left:%d", timing.h.state, timing.h.counter);
}

void chip::hsync(bool val)
{
    if (regs.ignore_hsync) return;
    if (val) { // 0->1 means it's time for HSW to end and/or new line starting at wait for display
        if (timing.h.state != S_sync_window) {
            force_new_line();
        }
        new_h_state(S_wait_for_display);
    }
    else {
    }
}

void chip::vsync(bool val)
{
    if (regs.ignore_vsync) return;

    if (val == 1) { // OK, reset our frame-ish!
        if (timing.v.state != S_sync_window) {
            force_new_frame();
        }
        new_v_state(S_wait_for_display);
    }
}

void chip::cycle()
{
    timing.h.counter--;
    if (timing.h.counter < 1) {
        new_h_state(static_cast<states>((static_cast<i32>(timing.h.state) + 1) & 3));
    }
    if ((timing.v.state == S_display) && (timing.h.state == S_display)) {
        // clock a pixel, yo!
        // grab the current 8 pixels
        if (pixel_shifter.num == 0) {
            u32 addr = (bg.y_tile * bg.x_tiles) + bg.x_tile;
            u32 entry = read_VRAM(addr);
            addr = ((entry & 0xFFF) << 4) + (regs.yscroll & 7);
            pixel_shifter.palette = (entry >> 12) & 15;
            u32 plane12 = read_VRAM(addr);
            u32 plane34 = read_VRAM(addr+8);
            pixel_shifter.pattern_shifter = tg16_decode_line(plane12, plane34);
            pixel_shifter.num = 8;
            bg.x_tile = (bg.x_tile + 1) & bg.x_tiles_mask;
        }
        if (regs.first_render) {
            /*regs.px_out_fifo.num = 16; // delay 16 pixels...
            regs.px_out_fifo.head = 0;
            regs.px_out_fifo.tail = 16;
            for (u32 i = 0; i < 16; i++) {
                regs.px_out_fifo.vals[i] = regs.px_out;
            }*/
            dbg.evptr->report_draw_start();
            u32 scroll_discard = io.BXR.u & 7;
            if (scroll_discard) {
                pixel_shifter.pattern_shifter >>= 4 * scroll_discard;
                pixel_shifter.num -= scroll_discard;
            }
            regs.first_render = 0;
        }
        if (pixel_shifter.num > 0) {
            pixel_shifter.num--;
            u32 bg_color = (pixel_shifter.pattern_shifter & 15);
            pixel_shifter.pattern_shifter >>= 4;

            u32 sp_color = 0;
            u32 sp_pal, sp_prio;
            for (i32 spnum = static_cast<i32>(sprites.num_tiles_on_line) - 1; spnum >= 0; spnum--) {
                auto &sp = sprites.tiles[spnum];
                // If not triggered, check if triggered!
                if (!sp.triggered) {
                    sp.triggered |= sp.x == regs.x_counter;
                }
                // If triggered, clock a pixel!
                if (sp.triggered && sp.num_left) {
                    sp.num_left--;
                    u64 col = sp.pattern_shifter & 15;
                    sp.pattern_shifter >>= 4;
                    if (col) {
                        // Sprite #0 hit
                        if ((sp.original_num == 0) && (sp_color != 0) && !io.STATUS.CR) {
                            io.STATUS.CR = 1;
                            update_irqs();
                        }
                        sp_color = col;
                        sp_pal = sp.palette;
                        sp_prio = sp.priority;
                    }
                }
            }

            // BGS OFF FOR DEBUG

            if (!io.CR.BB) bg_color = 0;
            if (!io.CR.SB) sp_color = 0;
            if (bg_color && sp_color) { // Discriminate!
                if (!sp_prio) // Sprite on top!
                    sp_color = 0;
                else
                    bg_color = 0;
            }

            u32 px;
            if (sp_color) { // Sprite color!
                px = 0x100 | (sp_pal << 4) | sp_color;
            } else if (bg_color) { // Background color!
                px = (pixel_shifter.palette << 4) | bg_color;
            } else { // Backdrop color!
                px = 0;
            }

            regs.px_out = px;
            //if (regs.y_counter == 64) regs.px_out = 0xFFFF;
            //if (events_view_get_current_line(dbg.events.view) == 1) regs.px_out = 0xFFFF;
            // Append px to fifo
            /*u32 n = regs.px_out_fifo.tail;
            regs.px_out_fifo.tail = (regs.px_out_fifo.tail + 1) & 31;
            regs.px_out_fifo.vals[n] = px;
            regs.px_out_fifo.num++;

            */
        }
    }
    else {
        regs.px_out = 0x100;
    }
    if (regs.x_counter == ((regs.HDW - 1) * 8 + 2)) update_RCR(this, 0, 0, 0);
    regs.x_counter++;
    /*if (regs.px_out_fifo.num) {
        regs.px_out = regs.px_out_fifo.vals[regs.px_out_fifo.head];
        regs.px_out_fifo.head = (regs.px_out_fifo.head + 1) & 31;
        regs.px_out_fifo.num--;
    }
    else {
        regs.px_out = 0x100;
    }*/
}


void chip::reset()
{
    io.HSW = 2;
    io.HDS = 2;
    io.HDW = 31;
    io.HDE = 4;
    io.VSW = 2;
    io.VDW.u = 239;
    io.VCR = 4;
}

void chip::update_RCR(void *ptr, u64 key, u64 clock, u32 jitter)
{
    auto *th = static_cast<chip *>(ptr);;
    u32 signal = (th->regs.y_counter == th->io.RCR.u);
    //printf("\nTEST LINE %d against %d @%lld", regs.y_counter, io.RCR.u, *scheduler->clock);
    if (signal && (signal != th->io.STATUS.RR) && (th->io.CR.IE & 4)) {
        DBG_EVENT_TH(th->dbg.events.HIT_RCR);
        th->io.STATUS.RR = 1;
        //printf("\nRCR HIT LINE %d  DL:%lld  X %lld", regs.y_counter, events_view_get_current_line(dbg.events.view), events_view_get_current_line_pos(dbg.events.view));
    }

    th->update_irqs();
}

void chip::vram_satb_end(void *ptr, u64 key, u64 clock, u32 jitter)
{
    auto *th = static_cast<chip *>(ptr);
    th->io.STATUS.DS = 1;
    if (!th->io.DCR.DSR)
        th->regs.vram_satb_pending = 0;
    th->update_irqs();
}

void chip::vram_vram_end(void *ptr, u64 key, u64 clock, u32 jitter)
{
    auto *th = static_cast<chip *>(ptr);
    th->io.STATUS.DV = 1;
    th->update_irqs();
}

void chip::vram_satb()
{
    // TODO: make not instant
    scheduler->only_add_abs((*scheduler->clock) + (1024 * regs.divisor), 0, this, &vram_satb_end, nullptr);
    for (u32 i = 0; i < 0x100; i++) {
        u32 addr = (io.DVSSR.u + i) & 0xFFFF;
        SAT[i] = read_VRAM(addr);
    }
}

void chip::trigger_vram_satb()
{
    regs.vram_satb_pending = 1;
}

void chip::trigger_vram_vram()
{
    // TODO: make not instant
    if (!regs.in_vblank) {
        printf("\nWarn attempt to VRAM-VRAM DMA outside vblank!");
        //return;
    }
#ifdef TG16_LYCODER
    dbg_printf("\nVRAM-VRAM src:%04x dst:%04x len:%d", io.SOUR.u, io.DESR.u, io.LENR.u);
#endif

    scheduler->only_add_abs((*scheduler->clock) + (4 * io.LENR.u), 0, this, &vram_vram_end, nullptr);
    io.LENR.u++;
    while(true) {
        u16 val = read_VRAM(io.SOUR.u);
        write_VRAM(io.DESR.u, val);
        io.SOUR.u += io.DCR.SI_D ? -1 : 1;
        io.DESR.u += io.DCR.DI_D ? -1 : 1;

        io.LENR.u--;
        if (io.LENR.u == 0) break;
    }
}

void chip::hblank(bool val)
{
    if (val) {
        regs.px_out = 0x100;
    }
    else {

    }
}

void chip::vblank(bool val)
{
    regs.in_vblank = val;
    if (val) {
        regs.px_out = 0x100;
        io.STATUS.VD = 1;
        update_irqs();
        bg.x_tiles = io.bg.x_tiles;
        bg.y_tiles = io.bg.y_tiles;
        bg.x_tiles_mask = bg.x_tiles - 1;
        bg.y_tiles_mask = bg.y_tiles - 1;

        if (regs.vram_satb_pending)
            vram_satb();
    }
    else {
        update_irqs();
    }
}


void chip::write_addr(u8 val)
{
    io.ADDR = val & 0x1F;
}

void chip::update_ie()
{
    u32 ie = (io.CR.IE & 7) | ((io.CR.IE & 8) << 2);
    ie |= io.DCR.DSC << 3; // 0001 to 1000
    ie |= io.DCR.DVC << 4; // 0001 to 10000
    regs.IE = ie;
    /*
     * Bits are...
    0  Collision detect sprite #0 collide with 1-63
    1  >16 sprite/line, not enough time for sprites, CGX error
    2  scanning line detect
    3  VRAM->SATB complete
    4  VRAM->VRAM complete
    5  vblank started
     */
}

void chip::update_irqs()
{
    u32 old_line = irq.line;
    irq.line = !!(regs.IE & io.STATUS.u);
    if (old_line != irq.line) {
        //printf("\nIE:%02x STATUS:%02x IRQL:%d  DISPLAY LINE:%lld", regs.IE, io.STATUS.u, irq.line, events_view_get_current_line(dbg.events.view));
        irq.update_func(irq.update_func_ptr, irq.line);
    }
}

void chip::write_lsb(u8 val)
{
    //printf("\nWRITE LSB %x: %02x", io.ADDR, val);
    switch(io.ADDR) {
        case 0x00: // MAWR
            io.MAWR.lo = val;
            return;
        case 0x01: // MARR
            io.MARR.lo = val;
            return;
        case 0x02: // VWR
            io.VWR.lo = val;
            return;
        case 0x03:
            //io.VRR.lo = val;
            return;
        case 0x04:
            printf("\nWARN write reserved huc6270 register %d", io.ADDR);
            return;
        case 0x05: // CR
            io.CR.lo = val;
            //printf("\nWRITE CTRL:%02x", val);
            update_ie();
            update_irqs();
            //printf("\nRCR Hit ENABLE: %d", regs.IE & 4);
            switch((val >> 4) & 3) {
                case 0:
                    regs.ignore_hsync = 0;
                    regs.ignore_vsync = 0;
                    break;
                case 1:
                    regs.ignore_hsync = 1;
                    regs.ignore_vsync = 0;

                    break;
                case 2:
                    printf("\nWARNING INVALID HSYNC/VSYNC COMBO");
                    break;
                case 3:
                    regs.ignore_hsync = 1;
                    regs.ignore_vsync = 1;
                    break;
            }
            return;
        case 0x06:
            io.RCR.lo = val;
            DBG_EVENT(dbg.events.WRITE_RCR);
            //printf("\nRCR WRITE %d/%x. CUR LINE:%d   RR:%d   IE:%x", io.RCR.u, io.RCR.u, regs.y_counter, io.STATUS.RR, regs.IE);
            return;
        case 0x07: // BGX
            io.BXR.lo = val;
            DBG_EVENT(dbg.events.WRITE_XSCROLL);
            return;
        case 0x08: // BGY
            io.BYR.lo = val;
            DBG_EVENT(dbg.events.WRITE_YSCROLL);
            regs.next_yscroll = io.BYR.u+1;
            //printf("\nYSCROLL write line %lld: %d", events_view_get_current_line(dbg.events.view), regs.next_yscroll);
            return;
        case 0x09: {
            static const u32 screen_sizes[8][2] = { {32, 32}, {64, 32},
                                                    {128, 32}, {128, 32},
                                                    {32, 64}, {64, 64},
                                                    {128, 64}, {128,64}
            };
            u32 sr = (val >> 4) & 7;
            io.bg.x_tiles = screen_sizes[sr][0];
            io.bg.y_tiles = screen_sizes[sr][1];
            regs.BAT_size = io.bg.x_tiles * io.bg.y_tiles;
            return; }
        case 0x0A:
            io.HSW = val & 0x1F;
            return;
        case 0x0B:
            io.HDW = val & 0x7F;
            regs.HDW = (io.HDW + 1) << 3;
            return;
        case 0x0C:
            io.VSW = val & 0x1F;
            return;
        case 0x0D:
            io.VDW.lo = val;
            return;
        case 0x0E:
            io.VCR = val;
            return;
        case 0x0F:
            io.DCR.u = val & 0x1F;
            update_ie();
            update_irqs();
            return;
        case 0x10:
            io.SOUR.lo = val;
            return;
        case 0x11:
            io.DESR.lo = val;
        case 0x12:
            io.LENR.lo = val;
            return;
        case 0x13:
            io.DVSSR.lo = val;
            return;
    }
    printf("\nWRITE LSB NOT IMPL: %02x", io.ADDR);
}

void chip::write_msb(u8 val)
{
    //printf("\nWRITE MSB %x: %02x", io.ADDR, val);
    switch(io.ADDR) {
        case 0x00: // MAWR
            io.MAWR.hi = val;
            return;
        case 0x01: // MARR
            io.MARR.hi = val;
            io.VRR.u = read_VRAM(io.MARR.u);
            io.MARR.u += regs.vram_inc;
            return;
        case 0x02: // VWR
            io.VWR.hi = val;
            write_VRAM(io.MAWR.u, io.VWR.u);
            io.MAWR.u += regs.vram_inc;
            return;
        case 0x03:
            //io.VRR.hi = val;
            return;
        case 0x04:
            printf("\nWARN write reserved huc6270 register %d", io.ADDR);
            return;
        case 0x05:
            io.CR.hi = val;
            switch(io.CR.IW) {
                case 0:
                    regs.vram_inc = 1;
                    break;
                case 1:
                    regs.vram_inc = 0x20;
                    break;
                case 2:
                    regs.vram_inc = 0x40;
                    break;
                case 3:
                    regs.vram_inc = 0x80;
                    break;
            }
            return;
        case 0x06:
            io.RCR.hi = val & 3;
            DBG_EVENT(dbg.events.WRITE_RCR);
            //printf("\nRCR WRITE %d/%x. CUR LINE:%d   RR:%d   IE:%x", io.RCR.u, io.RCR.u, regs.y_counter, io.STATUS.RR, regs.IE);
            return;
        case 0x07: // BGX scroll BXR
            DBG_EVENT(dbg.events.WRITE_XSCROLL);
            io.BXR.hi = val & 3;
            return;
        case 0x08: // BGY
            io.BYR.hi = val & 1;
            DBG_EVENT(dbg.events.WRITE_YSCROLL);
            //printf("\nBYR H: %d", io.BYR.hi);
            regs.next_yscroll = io.BYR.u+1;
            return;
        case 0x09:
            return;
        case 0x0A:
            io.HDS = val & 0x7F;
            return;
        case 0x0B:
            io.HDE = val & 0x7F;
            return;
        case 0x0C:
            io.VDS = val;
            return;
        case 0x0D:
            io.VDW.hi = val & 1;
            return;
        case 0x0E:
            return;
        case 0x0F:
            return;
        case 0x10:
            io.SOUR.hi = val;
            return;
        case 0x11:
            io.DESR.hi = val;
            return;
        case 0x12:
            io.LENR.hi = val;
            // Trigger VRAM-VRAM transfer
            trigger_vram_vram();
            return;
        case 0x13:
            io.DVSSR.hi = val;
            // Trigger VRAM-SATB Transfer
            trigger_vram_satb();
            return;
    }
    printf("\nWRITE MSB NOT IMPL: %02x", io.ADDR);
}

void chip::write(u32 addr, u8 val)
{
    addr &= 3;
#ifdef TG16_LYCODER2
    dbg_printf("VDC WRITE %04X: %02x\n", addr & 0x1FFF, val);
#endif
    //printf("\nHUC6270 WRITE %06x %02x", addr, val);
    switch(addr) {
        case 0:
            write_addr(val);
            return;
        case 1:
            return;
        case 2:
            write_lsb(val);
            return;
        case 3:
            write_msb(val);
            return;
    }
}

u8 chip::read_status()
{
    u32 v = io.STATUS.u;
    io.STATUS.u &= 0b1000000;
    update_irqs();
    return v;
}

u8 chip::read(u32 addr, u8 old)
{
    addr &= 3;
    switch(addr) {
        case 0:
            return read_status();
        case 1:
            return 0;
        case 2:
            return io.VRR.lo;
        case 3: {
            u32 v = io.VRR.hi;

            if (io.ADDR == 2) {
                io.VRR.u = read_VRAM(io.MARR.u);
                io.MARR.u += regs.vram_inc;
            }
            return v;
        }
    }
    printf("\nUnserviced HUC6270 (VDC) read: %06x", addr);
    return old;
}

}
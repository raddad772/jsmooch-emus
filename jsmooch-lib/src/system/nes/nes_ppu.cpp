//
// Created by Dave on 2/5/2024.
//

#include <cstdio>
#include <cassert>

#include "helpers/debugger/debugger.h"

#include "nes_debugger.h"
#include "nes.h"
#include "nes_cpu.h"
#include "nes_ppu.h"

i32 PPU_effect_buffer::get(u32 cycle)
{
    u32 ci = cycle % 16;
    i32 r = items[ci];
    items[ci] = -1;
    return r;
}

void PPU_effect_buffer::set(u32 cycle, i32 value)
{
    items[cycle % 16] = value;
}

void NES_PPU::update_nmi() const
{
    nes->cpu.notify_NMI(status.nmi_out && io.nmi_enable);
}

void NES_PPU::scanline_prerender() {
    // 261
    if (line_cycle == 1) {
        io.sprite0_hit = 0;
        io.sprite_overflow = 0;
        status.nmi_out = 0;
        update_nmi();
    }
    if (rendering_enabled) {
        // Reload horizontal scroll 258-320
        if (line_cycle == 257) io.v = (io.v & 0xFBE0) | (io.t & 0x41F);
        if ((line_cycle >= 280) && (line_cycle <= 304)) {
            io.v = (io.v & 0x041F) | (io.t & 0x7BE0);
        }
    }
}

NES_PPU::NES_PPU(NES* nes) : nes(nes)
{
}

void NES_PPU::reset()
{
    line_cycle = 0;
    io.w = 0;
    if (!display)
        printf("\nCRT not reset, null");
    else
        display->scan_x = display->scan_y = 0;
}

void NES_PPU::write_cgram(u32 addr, u32 val)
{
    nes->bus.mapper->a12_watch(addr | 0x3F00);
    if((addr & 0x13) == 0x10) addr &= 0xEF;
    CGRAM[addr & 0x1F] = val & 0x3F;
}

u32 NES_PPU::read_cgram(u32 addr) const {
    if((addr & 0x13) == 0x10) addr &= 0xEF;
    u32 data = CGRAM[addr & 0x1F];
    if(io.greyscale) data &= 0x30;
    return data;
}

void NES_PPU::mem_write(u32 addr, u32 val)
{
    if ((addr & 0x3FFF) < 0x3F00) nes->bus.PPU_write(addr, val);
    else write_cgram(addr & 0x1F, val);
}

void NES_PPU::write_regs(u32 addr, u32 val)
{
    switch((addr & 7) | 0x2000) {
        case 0x2000: // PPUCTRL
            DBG_EVENT(DBG_NES_EVENT_W2000);
            io.sprite_pattern_table = (val & 8) >> 3;
            io.bg_pattern_table = (val & 0x10) >> 4;
            status.sprite_height = (val & 0x20) >> 5 ? 16 : 8;
            io.nmi_enable = (val & 0x80) >> 7;
            io.vram_increment = (val & 4) ? 32 : 1;

            io.t = (io.t & 0x73FF) | ((val & 3) << 10);

            update_nmi();
            return;
        case 0x2001: // PPUMASK
            io.greyscale = val & 1;
            io.bg_hide_left_8 = (val & 2) >> 1;
            io.sprite_hide_left_8 = (val & 4) >> 2;
            io.bg_enable = (val & 8) >> 3;
            io.sprite_enable = (val & 0x10) >> 4;
            io.emph_r = (val & 0x20) >> 5;
            io.emph_g = (val & 0x40) >> 6;
            io.emph_b = (val & 0x80) >> 7;
            io.emph_bits = (val & 0xE0) << 1;
            new_rendering_enabled = io.bg_enable || io.sprite_enable;
            return;
        case 0x2003: // OAMADDR
            io.OAM_addr = val;
            return;
        case 0x2004: // OAMDATA
            OAM[io.OAM_addr] = val;
            io.OAM_addr = (io.OAM_addr + 1) & 0xFF;
            return;
        case 0x2005: // PPUSCROLL
            if (io.w == 0) {
                io.x = val & 7;
                io.t = (io.t & 0x7FE0) | (val >> 3);
                io.w = 1;
            } else {
                io.t = (io.t & 0x0C1F) | ((val & 0xF8) << 2) | ((val & 7) << 12);
                io.w = 0;
            }
            return;
        case 0x2006: // PPUADDR
            DBG_EVENT(DBG_NES_EVENT_W2006);
            if (io.w == 0) {
                io.t = (io.t & 0xFF) | ((val & 0x3F) << 8);
                io.w = 1;
            } else {
                io.t = (io.t & 0x7F00) | val;

                //effect_buffer_set(&w2006_buffer, (nes->clock.ppu_master_clock / nes->clock.timing.ppu_divisor)+(3*nes->clock.timing.ppu_divisor), (i32)io.t);
                io.v = io.t;
                nes->bus.mapper->a12_watch(io.v);
                io.w = 0;
            }
            return;
        case 0x2007: // PPUDATA
            DBG_EVENT(DBG_NES_EVENT_W2007);
            if (rendering_enabled && ((nes->clock.ppu_y < nes->clock.timing.vblank_start) || (nes->clock.ppu_y > nes->clock.timing.vblank_end))) {
                printf("REJECT WRITE y:%d sp:%d bg:%d v:%04x data:%02x", nes->clock.ppu_y, io.sprite_enable, io.bg_enable, io.v, val);
                return;
            }
            mem_write(io.v, val);
            io.v = (io.v + io.vram_increment) & 0x7FFF;

            return;
        default:
            printf("WRITE UNIMPLEMENTED %04x", addr);
            break;
    }
}

u32 NES_PPU::read_regs(u32 addr, u32 val, u32 has_effect)
{
    u32 output = val;
    switch(addr) {
        case 0x2002:
            output = (io.sprite_overflow << 5) | (io.sprite0_hit << 6) | (status.nmi_out << 7);
            //if (status.nmi_out) dbg_break("YAY!", 0);
            if (has_effect) {
                status.nmi_out = 0;
                update_nmi();

                io.w = 0;
            }
            break;
        case 0x2004: // OAMDATA
            output = OAM[io.OAM_addr];
            // reads do not increment counter
            break;
        case 0x2007:
            if (rendering_enabled && ((nes->clock.ppu_y < nes->clock.timing.vblank_start) || (nes->clock.ppu_y > nes->clock.timing.vblank_end))) {
                return 0;
            }
            if ((io.v & 0x3FFF) >= 0x3F00) {
                output = read_cgram(addr);
            }
            else {
                output = latch.VRAM_read;
                latch.VRAM_read = nes->bus.PPU_read_effect(io.v & 0x3FFF);
            }
            io.v = (io.v + io.vram_increment) & 0x7FFF;
            //nes->bus.a12_watch(nes, io.v & 0x3FFF);
            break;
        default:
            printf("\nREAD UNIMPLEMENTED %04x", addr);
            break;
    }
    return output;
}

u32 NES_PPU::fetch_chr_line(u32 table, u32 tile, u32 line, u32 has_effect) {
    u32 r = (0x1000 * table) + (tile * 16) + line;
    last_sprite_addr = r + 8;
    u32 lo = nes->bus.PPU_read_effect(r);
    u32 hi = nes->bus.PPU_read_effect(r + 8);
    u32 output = 0;
    for (u32 i = 0; i < 8; i++) {
        output <<= 2;
        output |= (lo & 1) | ((hi & 1) << 1);
        lo >>= 1;
        hi >>= 1;
    }
    return output;
}

static inline u32 fetch_chr_addr(u32 table, u32 tile, u32 line) {
    return (0x1000 * table) + (tile * 16) + line;
}

u32 NES_PPU::fetch_chr_line_low(u32 addr) const {
    u32 low = nes->bus.PPU_read_effect(addr);
    u32 output = 0;
    for (u32 i = 0; i < 8; i++) {
        output <<= 2;
        output |= (low & 1);
        low >>= 1;
    }
    return output;
}

u32 NES_PPU::fetch_chr_line_high(u32 addr, u32 o) const {
    u32 high = nes->bus.PPU_read_effect(addr + 8);
    u32 output = 0;
    for (u32 i = 0; i < 8; i++) {
        output <<= 2;
        output |= ((high & 1) << 1);
        high >>= 1;
    }
    return output | o;
}

void NES_PPU::oam_evaluate_slow() {
    u32 odd = (line_cycle % 2) == 1;
    i32 eval_y = nes->clock.ppu_y;
    if (line_cycle < 65) {
        if (line_cycle == 1) {
            secondary_OAM_sprite_total = 0;
            secondary_OAM_index = 0;
            OAM_eval_index = 0;
            secondary_OAM_lock = false;
            OAM_eval_done = false;
            sprite0_on_next_line = false;
            for (unsigned char & n : secondary_OAM) {
                n = 0xFF;
            }
        }
        return;
    }
    if (line_cycle <= 256) { // and >= 65...
        if (OAM_eval_done) return;
        if (!odd) {
            OAM_transfer_latch = OAM[OAM_eval_index];
            if (!secondary_OAM_lock) {
                secondary_OAM[secondary_OAM_index] = OAM_transfer_latch;
                if ((eval_y >= OAM_transfer_latch) && (eval_y < (OAM_transfer_latch + status.sprite_height))) {
                    if (OAM_eval_index == 0) sprite0_on_next_line = true;
                    secondary_OAM[secondary_OAM_index + 1] = OAM[OAM_eval_index + 1];
                    secondary_OAM[secondary_OAM_index + 2] = OAM[OAM_eval_index + 2];
                    secondary_OAM[secondary_OAM_index + 3] = OAM[OAM_eval_index + 3];
                    secondary_OAM_index += 4;
                    secondary_OAM_sprite_total++;
                    //secondary_OAM_lock = secondary_OAM_index >= 32;
                    OAM_eval_done |= secondary_OAM_index >= 32;
                }
            }
            OAM_eval_index += 4;
            if (OAM_eval_index >= 256) {
                OAM_eval_index = 0;
                secondary_OAM_lock = true;
                OAM_eval_done = true;
            }
        }
        return;
    }

    if ((line_cycle >= 257) && (line_cycle <= 320)) { // Sprite tile fetches
        if (line_cycle == 257) { // Do some housekeeping on cycle 257
            sprite0_on_this_line = sprite0_on_next_line;
            secondary_OAM_index = 0;
            secondary_OAM_sprite_index = 0;
            if (!io.sprite_overflow) {
                // Perform weird sprite overflow glitch
                u32 n = 0;
                u32 m = 0;
                u32 f = 0;
                while (n < 64) {
                    u32 e = OAM[(n * 4) + m];
                    // If value is in range....
                    if ((eval_y >= e) && (eval_y < (e + status.sprite_height))) {
                        // Set overflow flag if needed
                        f++;
                        if (f > 8) {
                            io.sprite_overflow = 1;
                            break;
                        }
                        m = (m + 4) & 0x03;
                        n++;
                    }
                    // Value is not in range...
                    else {
                        n++;
                        m = (m + 4) & 0x03; // Here is the hardware bug. This should be set to 0 instead!
                    }
                }
            }
        }

        // Sprite data fetches into shift registers
        u32 sub_cycle = (line_cycle - 257) & 0x07;
        switch (sub_cycle) {
            case 0: {// Read Y coordinate.  257
                i32 syl = eval_y - secondary_OAM[secondary_OAM_index];
                if (syl < 0) syl = 0;
                if (syl > (status.sprite_height - 1)) syl = static_cast<i32>(status.sprite_height) - 1;
                sprite_y_lines[secondary_OAM_sprite_index] = syl;
                secondary_OAM_index++;
                break; }
            case 1: // Read tile number 258, and do garbage NT access
                sprite_pattern_shifters[secondary_OAM_sprite_index] = secondary_OAM[secondary_OAM_index];
                secondary_OAM_index++;
                nes->bus.mapper->a12_watch(io.v);
                //printf("\nGARBAGE 1! %04x", io.v);
                break;
            case 2: // Read attributes 259
                sprite_attribute_latches[secondary_OAM_sprite_index] = secondary_OAM[secondary_OAM_index];
                secondary_OAM_index++;
                break;
            case 3: // Read X-coordinate 260 and do garbage NT access
                sprite_x_counters[secondary_OAM_sprite_index] = secondary_OAM[secondary_OAM_index];
                secondary_OAM_index++;
                nes->bus.mapper->a12_watch(io.v);
                break;
            case 4: // Fetch tiles for the shifters 261
                break;
            case 5: {
                u32 tn = sprite_pattern_shifters[secondary_OAM_sprite_index];
                u32 sy = sprite_y_lines[secondary_OAM_sprite_index];
                u32 table = io.sprite_pattern_table;
                u32 attr = sprite_attribute_latches[secondary_OAM_sprite_index];
                // Vertical flip....
                if (attr & 0x80) sy = (status.sprite_height - 1) - sy;
                if (status.sprite_height == 16) {
                    table = tn & 1;
                    tn &= 0xFE;
                }
                if (sy > 7) {
                    sy -= 8;
                    tn += 1;
                }
                sprite_pattern_shifters[secondary_OAM_sprite_index] = fetch_chr_line(table, tn, sy,
                                                                                                 1);
                break;
            }
            case 6: // 263
                break;
            case 7:
                nes->bus.mapper->a12_watch(last_sprite_addr);
                secondary_OAM_sprite_index++;
                break;
            default:
                assert(1==2);
        }
    }
}

void NES_PPU::cycle_scanline_addr() {
    u32 enabled = rendering_enabled;
    if (enabled && (nes->clock.ppu_y < nes->clock.timing.bottom_rendered_line)) {
        // Sprites
        if ((line_cycle > 0) && (line_cycle < 257)) {
            sprite_x_counters[0]--;
            sprite_x_counters[1]--;
            sprite_x_counters[2]--;
            sprite_x_counters[3]--;
            sprite_x_counters[4]--;
            sprite_x_counters[5]--;
            sprite_x_counters[6]--;
            sprite_x_counters[7]--;
        }
    }
    if ((!enabled) || (line_cycle == 0)) return;
    // Cycle # 8, 16,...248, and 328, 336. BUT NOT 0
    if (line_cycle == 256) {
        if ((io.v & 0x7000) != 0x7000) { // if fine y != 7
            io.v += 0x1000;               // add 1 to fine y
        }
        else {                                   // else it is overflow so
            io.v &= 0x8FFF;                 // clear fine y to 0
            u32 y = (io.v & 0x03E0) >> 5;  // get coarse y
            if (y == 29) {                      // y overflows 30->0 with vertical nametable swap
                y = 0;
                io.v ^= 0x0800;             // Change vertical nametable
            } else if (y == 31) {               // y also overflows at 31 but without nametable swap
                y = 0;
            }
            else                                 // just add to coarse scroll
                y += 1;
            io.v = (io.v & 0xFC1F) | (y << 5); // put scroll back in
        }
        return;
    }
    if (((line_cycle & 7) == 0) && ((line_cycle >= 328) || (line_cycle <= 256))) {
        // INCREMENT HORIZONTAL SCROLL IN v
        if ((io.v & 0x1F) == 0x1F) // If X scroll is 31...
            io.v = (io.v & 0xFFE0) ^ 0x0400; // clear counter scroll to 0 (& FFE0) and swap nametable (^ 0x400)
        else
            io.v++;  // just increment the X scroll
        return;
    }
    // INCREMENT VERTICAL SCROLL IN v
    // Cycles 257, copy parts of T to V
    if ((line_cycle == 257) && rendering_enabled) {
        io.v = (io.v & 0xFBE0) | (io.t & 0x41F);
    }
}

void NES_PPU::perform_bg_fetches() { // Only called from prerender and visible scanlines
    if (!rendering_enabled) return;
    u32 in_tile_y = (io.v >> 12) & 7; // Y position inside tile

    if (((line_cycle > 0) && (line_cycle <= 256)) || (line_cycle > 320)) {
        // Do memory accesses and shifters
        switch (line_cycle & 7) {
            case 1: // nametable, tile #
                bg_fetches[0] = nes->bus.PPU_read_effect(0x2000 | (io.v & 0xFFF));
                bg_tile_fetch_addr = fetch_chr_addr(io.bg_pattern_table, bg_fetches[0], in_tile_y);
                bg_tile_fetch_buffer = 0;
                // Reload shifters if needed
                if (line_cycle != 1) { // reload shifter at interval #9 9....257
                    bg_shifter = (bg_shifter >> 16) | (bg_fetches[2] << 16) | (bg_fetches[3] << 24);
                    bg_palette_shifter = ((bg_palette_shifter << 2) | bg_fetches[1]) & 0x0F; //(bg_palette_shifter >> 8) | (bg_fetches[1] << 8);
                }
                break;
            case 3: {
                // attribute table
                u32 attrib_addr = 0x23C0 | (io.v & 0x0C00) | ((io.v >> 4) & 0x38) | ((io.v >> 2) & 7);
                u32 shift = ((io.v >> 4) & 0x04) | (io.v & 0x02);
                bg_fetches[1] = (nes->bus.PPU_read_effect(attrib_addr) >> shift) & 3;
                break; }
            case 5: // low buffer
                bg_tile_fetch_buffer = fetch_chr_line_low(bg_tile_fetch_addr);
                break;
            case 7: // high buffer
                bg_tile_fetch_buffer = fetch_chr_line_high(bg_tile_fetch_addr, bg_tile_fetch_buffer);
                bg_fetches[2] = bg_tile_fetch_buffer & 0xFF;
                bg_fetches[3] = (bg_tile_fetch_buffer >> 8);
                break;
        }
    }
}

void NES_PPU::scanline_visible()
{
    i32 sx = line_cycle-1;
    i32 sy = nes->clock.ppu_y;
    i32 bo = (sy * 256) + sx;
    if (!rendering_enabled) {
        if (line_cycle < 256) {
            cur_output[bo] = CGRAM[0] | io.emph_bits;
        }
        return;
    }
    if (line_cycle < 1) {
        if (nes->clock.ppu_y == 0)
            nes->clock.ppu_frame_cycle = 0;
        return;
    }

    if ((line_cycle == 1) && (nes->clock.ppu_y == 32)) { // Capture scroll info for display
        dbg.v = io.v;
        dbg.t = io.t;
        dbg.x = io.x;
        dbg.w = io.w;
    }

    cycle_scanline_addr();
    oam_evaluate_slow();
    perform_bg_fetches();

    // Shift out some bits for backgrounds
    u32 bg_color=0;
    u32 bg_has_pixel = false;
    if (io.bg_enable) {
        u32 bg_shift = (((sx & 7) + io.x) & 15) * 2;
        bg_color = (bg_shifter >> bg_shift) & 3;
        bg_has_pixel = bg_color != 0;
    }
    //u32 sprite_has_pixel = false;
    if (bg_has_pixel) {
        u32 agb = bg_palette_shifter;
        if (io.x + (sx & 0x07) < 8) agb >>= 2;
        bg_color = CGRAM[bg_color | ((agb & 3) << 2)];
    }
    else bg_color = CGRAM[0];

    //scanline_timer.record_split('bgcolor')


    u32 sprite_priority = 0;
    u32 sprite_color = 0;

    // Check if any sprites need drawing
    //for (u32 m = 0; m < 8; m++) {
    for (i32 m = 7; m >= 0; m--) {
        if ((sprite_x_counters[m] >= -8) && (sprite_x_counters[m] <= -1) && line_cycle < 256) {
            u32 s_x_flip = (sprite_attribute_latches[m] & 0x40) >> 6;
            u32 my_color;
            if (s_x_flip) {
                my_color = (sprite_pattern_shifters[m] & 0xC000) >> 14;
                sprite_pattern_shifters[m] <<= 2;
            } else {
                my_color = (sprite_pattern_shifters[m] & 3);
                sprite_pattern_shifters[m] >>= 2;
            }
            if (my_color != 0) {
                //sprite_has_pixel = true;
                my_color |= (sprite_attribute_latches[m] & 3) << 2;
                sprite_priority = (sprite_attribute_latches[m] & 0x20) >> 5;
                sprite_color = CGRAM[0x10 + my_color];
                if ((!io.sprite0_hit) && (sprite0_on_this_line) && (m == 0) && bg_has_pixel && (line_cycle < 256)) {
                    //printf("\nSPRITE 0 LINE:%d CYCLE:%d FRAME:%lld", nes->clock.ppu_y, line_cycle, nes->clock.master_frame);
                    io.sprite0_hit = 1;
                }
            }
        }
    }

    // Decide background or sprite
    u32 out_color = bg_color;
    if (io.sprite_enable) {
        if (sprite_color != 0) {
            if (!bg_has_pixel) {
                out_color = sprite_color;
            } else {
                if (!sprite_priority) out_color = sprite_color;
                else out_color = bg_color;
            }
        }
    }

    cur_output[bo] = out_color | io.emph_bits;
}

void NES_PPU::scanline_postrender() {
    // 240, (also 241-260)
    // LITERALLY DO NOTHING
    if ((nes->clock.ppu_y == nes->clock.timing.vblank_start) && (line_cycle == 1)) {
        status.nmi_out = 1;
        update_nmi();
    }
}

void NES_PPU::new_frame() {
    //nes->clock.lclock = nes->clock.cpu_master_clock;
    debugger_report_frame(nes->dbg.interface);

    display->scan_y = 0;
    nes->clock.ppu_y = 0;
    nes->clock.frames_since_restart++;
    nes->clock.frame_odd = (nes->clock.frame_odd + 1) & 1;
    nes->clock.master_frame++;
    nes->clock.cpu_frame_cycle = 0;
    cur_output = static_cast<u16 *>(display->output[display->last_written]);
    assert((u64)cur_output > 1);
    display->last_written ^= 1;
}

void NES_PPU::set_scanline(u32 to) {
    if (to == 0) {
        render_cycle = &NES_PPU::scanline_visible;
    }
    else if (to == nes->clock.timing.post_render_ppu_idle) {
        render_cycle = &NES_PPU::scanline_postrender;
    }
    else if (to == nes->clock.timing.ppu_pre_render) {
        render_cycle = &NES_PPU::scanline_prerender;
    }
}

void NES_PPU::new_scanline() {
    display->scan_x = 0;
    display->scan_y++;
    if (nes->clock.ppu_y == nes->clock.timing.ppu_pre_render)
        new_frame();
    else
        nes->clock.ppu_y++;
        // debugger_copy_scanline_info

    if ((nes->clock.ppu_y >= 0) && (nes->clock.ppu_y < 240)) {
        NES::NESDBGDATA::DBGNESROW &rw = nes->dbg_data.rows[nes->clock.ppu_y];
        /*rw.io.bg_enable = io.bg_enable;
        rw.io.bg_hide_left_8 = io.bg_hide_left_8;*/
        rw.io.bg_pattern_table = io.bg_pattern_table;
        rw.io.emph_bits = io.emph_bits;
        rw.io.x_scroll = (io.x | ((io.t & 0x1F) << 3));
        rw.io.y_scroll = ((((io.t >> 5) & 0x1F) << 3) | ((io.t >> 12) & 7));
        rw.io.x_scroll += ((io.t >> 10) & 1) * 256;
        rw.io.y_scroll += ((io.t >> 11) & 1) * 240;
    }
    debugger_report_line(nes->dbg.interface, nes->clock.ppu_y);


    /*if (nes->clock.ppu_y == nes->clock.timing.vblank_start) {
        //nes->clock.vblank = 1;
        NES_PPU_update_nmi();
    }
    else if (nes->clock.ppu_y == nes->clock.timing.vblank_end) {
        //nes->clock.vblank = 0;
        NES_PPU_update_nmi();
    }*/
    set_scanline(nes->clock.ppu_y);
    line_cycle = 0;
}

u32 NES_PPU::cycle(u32 howmany) {
        for (u32 i = 0; i < howmany; i++) {
            /*i32 r = effect_buffer_get(&w2006_buffer, nes->clock.ppu_master_clock / nes->clock.timing.ppu_divisor);
            if (r != -1) {
                io.v = r;
                NES_bus_a12_watch(nes, r);
            }*/
            (this->*render_cycle)();
            rendering_enabled = new_rendering_enabled;
            line_cycle++;
            display->scan_x++;
            nes->clock.ppu_frame_cycle++;
            if (line_cycle == 341) new_scanline();
            nes->clock.ppu_master_clock += nes->clock.timing.ppu_divisor;
        }
        return howmany;
}


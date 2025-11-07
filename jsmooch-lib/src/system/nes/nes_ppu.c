//
// Created by Dave on 2/5/2024.
//

#include "stdio.h"

#include "helpers_c/debugger/debugger.h"

#include "nes_debugger.h"
#include "nes.h"
#include "nes_cpu.h"
#include "nes_ppu.h"

#define THIS struct NES_PPU* this

void PPU_scanline_visible(struct NES_PPU* this);

static void effect_buffer_init(struct PPU_effect_buffer* this)
{
    this->length = 16;
    for (u32 i = 0; i < 16; i++) {
        this->items[i] = -1;
    }
}

static i32 effect_buffer_get(struct PPU_effect_buffer* this, u32 cycle)
{
    u32 ci = cycle % 16;
    i32 r = this->items[ci];
    this->items[ci] = -1;
    return r;
}

static void effect_buffer_set(struct PPU_effect_buffer* this, u32 cycle, i32 value)
{
    this->items[cycle % 16] = value;
}

void NES_PPU_update_nmi(THIS)
{
    r2A03_notify_NMI(&this->nes->cpu, this->status.nmi_out && this->io.nmi_enable);
}

void PPU_scanline_prerender(THIS) {
    // 261
    if (this->line_cycle == 1) {
        this->io.sprite0_hit = 0;
        this->io.sprite_overflow = 0;
        this->status.nmi_out = 0;
        NES_PPU_update_nmi(this);
    }
    if (this->rendering_enabled) {
        // Reload horizontal scroll 258-320
        if (this->line_cycle == 257) this->io.v = (this->io.v & 0xFBE0) | (this->io.t & 0x41F);
        if ((this->line_cycle >= 280) && (this->line_cycle <= 304)) {
            this->io.v = (this->io.v & 0x041F) | (this->io.t & 0x7BE0);
        }
    }
}

void NES_PPU_init(struct NES_PPU* this, struct NES* nes)
{
    this->nes = nes;

    this->render_cycle = &PPU_scanline_visible;

    this->line_cycle = 0;
    u32 i;
    for (i = 0; i < 256; i++) {
        this->OAM[i] = 0;
    }
    for (i = 0; i < 32; i++) {
        this->secondary_OAM[i] = 0;
        this->CGRAM[i] = 0;
        if (i < 4) this->bg_fetches[i] = 0;
        if (i < 8) {
            this->sprite_pattern_shifters[i] = 0;
            this->sprite_attribute_latches[i] = 0;
            this->sprite_x_counters[i] = 0;
            this->sprite_y_lines[i] = 0;
        }
    }

    this->secondary_OAM_index = this->secondary_OAM_sprite_index = 0;
    this->secondary_OAM_sprite_total = this->secondary_OAM_lock = 0;
    this->OAM_transfer_latch = this->OAM_eval_index = this->OAM_eval_done = 0;
    this->sprite0_on_next_line = this->sprite0_on_this_line = 0;

    effect_buffer_init(&this->w2006_buffer);
    this->bg_shifter = this->bg_palette_shifter = 0;
    this->bg_tile_fetch_addr = this->bg_tile_fetch_buffer = 0;
    this->last_sprite_addr = 0;

    this->io.nmi_enable = 0;
    this->io.sprite_overflow = 0;
    this->io.sprite0_hit = 0;
    this->io.vram_increment = 1;
    this->io.sprite_pattern_table = 0;
    this->io.bg_pattern_table = 0;

    this->io.v = this->io.t = this->io.x = this->io.w = 0;

    this->io.greyscale = this->io.bg_hide_left_8 = 0;
    this->io.sprite_hide_left_8 = this->io.bg_enable = 0;
    this->io.sprite_enable = this->io.emph_bits = 0;

    this->io.emph_r = this->io.emph_g = this->io.emph_b = 0;
    this->io.OAM_addr = 0;

    this->dbg.v = this->dbg.t = this->dbg.x = this->dbg.w = 0;
    this->status.sprite_height = 8;
    this->status.nmi_out = 0;
    this->latch.VRAM_read = 0;
    this->rendering_enabled = this->new_rendering_enabled = 1;
}

void NES_PPU_reset(THIS)
{
    this->line_cycle = 0;
    this->io.w = 0;
    if (this->display == NULL)
        printf("\nCRT not reset, null");
    else
        this->display->scan_x = this->display->scan_y = 0;
}

void NES_PPU_write_cgram(THIS, u32 addr, u32 val)
{
    NES_bus_a12_watch(this->nes, addr | 0x3F00);
    if((addr & 0x13) == 0x10) addr &= 0xEF;
    this->CGRAM[addr & 0x1F] = val & 0x3F;
}

u32 NES_PPU_read_cgram(THIS, u32 addr)
{
    if((addr & 0x13) == 0x10) addr &= 0xEF;
    u32 data = this->CGRAM[addr & 0x1F];
    if(this->io.greyscale) data &= 0x30;
    return data;
}

void NES_PPU_mem_write(THIS, u32 addr, u32 val)
{
    if ((addr & 0x3FFF) < 0x3F00) NES_PPU_write(this->nes, addr, val);
    else NES_PPU_write_cgram(this, addr & 0x1F, val);
}

void NES_bus_PPU_write_regs(struct NES* nes, u32 addr, u32 val)
{
    struct NES_PPU* this = (struct NES_PPU*)&nes->ppu;
    switch((addr & 7) | 0x2000) {
        case 0x2000: // PPUCTRL
            DBG_EVENT(DBG_NES_EVENT_W2000);
            this->io.sprite_pattern_table = (val & 8) >> 3;
            this->io.bg_pattern_table = (val & 0x10) >> 4;
            this->status.sprite_height = (val & 0x20) >> 5 ? 16 : 8;
            this->io.nmi_enable = (val & 0x80) >> 7;
            this->io.vram_increment = (val & 4) ? 32 : 1;

            this->io.t = (this->io.t & 0x73FF) | ((val & 3) << 10);

            NES_PPU_update_nmi(this);
            return;
        case 0x2001: // PPUMASK
            this->io.greyscale = val & 1;
            this->io.bg_hide_left_8 = (val & 2) >> 1;
            this->io.sprite_hide_left_8 = (val & 4) >> 2;
            this->io.bg_enable = (val & 8) >> 3;
            this->io.sprite_enable = (val & 0x10) >> 4;
            this->io.emph_r = (val & 0x20) >> 5;
            this->io.emph_g = (val & 0x40) >> 6;
            this->io.emph_b = (val & 0x80) >> 7;
            this->io.emph_bits = (val & 0xE0) << 1;
            this->new_rendering_enabled = this->io.bg_enable || this->io.sprite_enable;
            return;
        case 0x2003: // OAMADDR
            this->io.OAM_addr = val;
            return;
        case 0x2004: // OAMDATA
            this->OAM[this->io.OAM_addr] = val;
            this->io.OAM_addr = (this->io.OAM_addr + 1) & 0xFF;
            return;
        case 0x2005: // PPUSCROLL
            if (this->io.w == 0) {
                this->io.x = val & 7;
                this->io.t = (this->io.t & 0x7FE0) | (val >> 3);
                this->io.w = 1;
            } else {
                this->io.t = (this->io.t & 0x0C1F) | ((val & 0xF8) << 2) | ((val & 7) << 12);
                this->io.w = 0;
            }
            return;
        case 0x2006: // PPUADDR
            DBG_EVENT(DBG_NES_EVENT_W2006);
            if (this->io.w == 0) {
                this->io.t = (this->io.t & 0xFF) | ((val & 0x3F) << 8);
                this->io.w = 1;
            } else {
                this->io.t = (this->io.t & 0x7F00) | val;

                //effect_buffer_set(&this->w2006_buffer, (nes->clock.ppu_master_clock / nes->clock.timing.ppu_divisor)+(3*nes->clock.timing.ppu_divisor), (i32)this->io.t);
                this->io.v = this->io.t;
                NES_bus_a12_watch(this->nes, this->io.v);
                this->io.w = 0;
            }
            return;
        case 0x2007: // PPUDATA
            DBG_EVENT(DBG_NES_EVENT_W2007);
            if (this->rendering_enabled && ((nes->clock.ppu_y < nes->clock.timing.vblank_start) || (nes->clock.ppu_y > nes->clock.timing.vblank_end))) {
                printf("REJECT WRITE y:%d sp:%d bg:%d v:%04x data:%02x", nes->clock.ppu_y, this->io.sprite_enable, this->io.bg_enable, this->io.v, val);
                return;
            }
            NES_PPU_mem_write(this, this->io.v, val);
            this->io.v = (this->io.v + this->io.vram_increment) & 0x7FFF;

            return;
        default:
            printf("WRITE UNIMPLEMENTED %04x", addr);
            break;
    }
}

u32 NES_bus_PPU_read_regs(struct NES* nes, u32 addr, u32 val, u32 has_effect)
{
    struct NES_PPU* this = (struct NES_PPU*)&nes->ppu;
    u32 output = val;
    switch(addr) {
        case 0x2002:
            output = (this->io.sprite_overflow << 5) | (this->io.sprite0_hit << 6) | (this->status.nmi_out << 7);
            if (has_effect) {
                this->status.nmi_out = 0;
                NES_PPU_update_nmi(this);

                this->io.w = 0;
            }
            break;
        case 0x2004: // OAMDATA
            output = this->OAM[this->io.OAM_addr];
            // reads do not increment counter
            break;
        case 0x2007:
            if (this->rendering_enabled && ((nes->clock.ppu_y < nes->clock.timing.vblank_start) || (nes->clock.ppu_y > nes->clock.timing.vblank_end))) {
                return 0;
            }
            if ((this->io.v & 0x3FFF) >= 0x3F00) {
                output = NES_PPU_read_cgram(this, addr);
            }
            else {
                output = this->latch.VRAM_read;
                this->latch.VRAM_read = NES_PPU_read_effect(this->nes, this->io.v & 0x3FFF);
            }
            this->io.v = (this->io.v + this->io.vram_increment) & 0x7FFF;
            //this->nes->bus.a12_watch(this->nes, this->io.v & 0x3FFF);
            break;
        default:
            printf("\nREAD UNIMPLEMENTED %04x", addr);
            break;
    }
    return output;
}

u32 fetch_chr_line(THIS, u32 table, u32 tile, u32 line, u32 has_effect) {
    u32 r = (0x1000 * table) + (tile * 16) + line;
    this->last_sprite_addr = r + 8;
    u32 lo = NES_PPU_read_effect(this->nes, r);
    u32 hi = NES_PPU_read_effect(this->nes, r + 8);
    u32 output = 0;
    for (u32 i = 0; i < 8; i++) {
        output <<= 2;
        output |= (lo & 1) | ((hi & 1) << 1);
        lo >>= 1;
        hi >>= 1;
    }
    return output;
}

u32 fetch_chr_addr(u32 table, u32 tile, u32 line) {
    return (0x1000 * table) + (tile * 16) + line;
}

u32 fetch_chr_line_low(THIS, u32 addr) {
        u32 low = NES_PPU_read_effect(this->nes, addr);
        u32 output = 0;
        for (u32 i = 0; i < 8; i++) {
            output <<= 2;
            output |= (low & 1);
            low >>= 1;
        }
        return output;
}

u32 fetch_chr_line_high(THIS, u32 addr, u32 o) {
    u32 high = NES_PPU_read_effect(this->nes, addr + 8);
    u32 output = 0;
    for (u32 i = 0; i < 8; i++) {
        output <<= 2;
        output |= ((high & 1) << 1);
        high >>= 1;
    }
    return output | o;
}

void oam_evaluate_slow(THIS) {
    u32 odd = (this->line_cycle % 2) == 1;
    i32 eval_y = this->nes->clock.ppu_y;
    if (this->line_cycle < 65) {
        if (this->line_cycle == 1) {
            this->secondary_OAM_sprite_total = 0;
            this->secondary_OAM_index = 0;
            this->OAM_eval_index = 0;
            this->secondary_OAM_lock = false;
            this->OAM_eval_done = false;
            this->sprite0_on_next_line = false;
            for (u32 n = 0; n < 32; n++) {
                this->secondary_OAM[n] = 0xFF;
            }
        }
        return;
    }
    if (this->line_cycle <= 256) { // and >= 65...
        if (this->OAM_eval_done) return;
        if (!odd) {
            this->OAM_transfer_latch = this->OAM[this->OAM_eval_index];
            if (!this->secondary_OAM_lock) {
                this->secondary_OAM[this->secondary_OAM_index] = this->OAM_transfer_latch;
                if ((eval_y >= this->OAM_transfer_latch) && (eval_y < (this->OAM_transfer_latch + this->status.sprite_height))) {
                    if (this->OAM_eval_index == 0) this->sprite0_on_next_line = true;
                    this->secondary_OAM[this->secondary_OAM_index + 1] = this->OAM[this->OAM_eval_index + 1];
                    this->secondary_OAM[this->secondary_OAM_index + 2] = this->OAM[this->OAM_eval_index + 2];
                    this->secondary_OAM[this->secondary_OAM_index + 3] = this->OAM[this->OAM_eval_index + 3];
                    this->secondary_OAM_index += 4;
                    this->secondary_OAM_sprite_total++;
                    //this->secondary_OAM_lock = this->secondary_OAM_index >= 32;
                    this->OAM_eval_done |= this->secondary_OAM_index >= 32;
                }
            }
            this->OAM_eval_index += 4;
            if (this->OAM_eval_index >= 256) {
                this->OAM_eval_index = 0;
                this->secondary_OAM_lock = true;
                this->OAM_eval_done = true;
            }
        }
        return;
    }

    if ((this->line_cycle >= 257) && (this->line_cycle <= 320)) { // Sprite tile fetches
        if (this->line_cycle == 257) { // Do some housekeeping on cycle 257
            this->sprite0_on_this_line = this->sprite0_on_next_line;
            this->secondary_OAM_index = 0;
            this->secondary_OAM_sprite_index = 0;
            if (!this->io.sprite_overflow) {
                // Perform weird sprite overflow glitch
                u32 n = 0;
                u32 m = 0;
                u32 f = 0;
                while (n < 64) {
                    u32 e = this->OAM[(n * 4) + m];
                    // If value is in range....
                    if ((eval_y >= e) && (eval_y < (e + this->status.sprite_height))) {
                        // Set overflow flag if needed
                        f++;
                        if (f > 8) {
                            this->io.sprite_overflow = 1;
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
        u32 sub_cycle = (this->line_cycle - 257) & 0x07;
        switch (sub_cycle) {
            case 0: {// Read Y coordinate.  257
                i32 syl = eval_y - this->secondary_OAM[this->secondary_OAM_index];
                if (syl < 0) syl = 0;
                if (syl > (this->status.sprite_height - 1)) syl = (i32)this->status.sprite_height - 1;
                this->sprite_y_lines[this->secondary_OAM_sprite_index] = syl;
                this->secondary_OAM_index++;
                break; }
            case 1: // Read tile number 258, and do garbage NT access
                this->sprite_pattern_shifters[this->secondary_OAM_sprite_index] = this->secondary_OAM[this->secondary_OAM_index];
                this->secondary_OAM_index++;
                NES_bus_a12_watch(this->nes, this->io.v);
                //printf("\nGARBAGE 1! %04x", this->io.v);
                break;
            case 2: // Read attributes 259
                this->sprite_attribute_latches[this->secondary_OAM_sprite_index] = this->secondary_OAM[this->secondary_OAM_index];
                this->secondary_OAM_index++;
                break;
            case 3: // Read X-coordinate 260 and do garbage NT access
                this->sprite_x_counters[this->secondary_OAM_sprite_index] = this->secondary_OAM[this->secondary_OAM_index];
                this->secondary_OAM_index++;
                NES_bus_a12_watch(this->nes, this->io.v);
                break;
            case 4: // Fetch tiles for the shifters 261
                break;
            case 5: {
                u32 tn = this->sprite_pattern_shifters[this->secondary_OAM_sprite_index];
                u32 sy = this->sprite_y_lines[this->secondary_OAM_sprite_index];
                u32 table = this->io.sprite_pattern_table;
                u32 attr = this->sprite_attribute_latches[this->secondary_OAM_sprite_index];
                // Vertical flip....
                if (attr & 0x80) sy = (this->status.sprite_height - 1) - sy;
                if (this->status.sprite_height == 16) {
                    table = tn & 1;
                    tn &= 0xFE;
                }
                if (sy > 7) {
                    sy -= 8;
                    tn += 1;
                }
                this->sprite_pattern_shifters[this->secondary_OAM_sprite_index] = fetch_chr_line(this, table, tn, sy,
                                                                                                 1);
                break;
            }
            case 6: // 263
                break;
            case 7:
                NES_bus_a12_watch(this->nes, this->last_sprite_addr);
                this->secondary_OAM_sprite_index++;
                break;
            default:
                assert(1==2);
        }
    }
}

void cycle_scanline_addr(THIS) {
    u32 enabled = this->rendering_enabled;
    if (enabled && (this->nes->clock.ppu_y < this->nes->clock.timing.bottom_rendered_line)) {
        // Sprites
        if ((this->line_cycle > 0) && (this->line_cycle < 257)) {
            this->sprite_x_counters[0]--;
            this->sprite_x_counters[1]--;
            this->sprite_x_counters[2]--;
            this->sprite_x_counters[3]--;
            this->sprite_x_counters[4]--;
            this->sprite_x_counters[5]--;
            this->sprite_x_counters[6]--;
            this->sprite_x_counters[7]--;
        }
    }
    if ((!enabled) || (this->line_cycle == 0)) return;
    // Cycle # 8, 16,...248, and 328, 336. BUT NOT 0
    if (this->line_cycle == 256) {
        if ((this->io.v & 0x7000) != 0x7000) { // if fine y != 7
            this->io.v += 0x1000;               // add 1 to fine y
        }
        else {                                   // else it is overflow so
            this->io.v &= 0x8FFF;                 // clear fine y to 0
            u32 y = (this->io.v & 0x03E0) >> 5;  // get coarse y
            if (y == 29) {                      // y overflows 30->0 with vertical nametable swap
                y = 0;
                this->io.v ^= 0x0800;             // Change vertical nametable
            } else if (y == 31) {               // y also overflows at 31 but without nametable swap
                y = 0;
            }
            else                                 // just add to coarse scroll
                y += 1;
            this->io.v = (this->io.v & 0xFC1F) | (y << 5); // put scroll back in
        }
        return;
    }
    if (((this->line_cycle & 7) == 0) && ((this->line_cycle >= 328) || (this->line_cycle <= 256))) {
        // INCREMENT HORIZONTAL SCROLL IN v
        if ((this->io.v & 0x1F) == 0x1F) // If X scroll is 31...
            this->io.v = (this->io.v & 0xFFE0) ^ 0x0400; // clear counter scroll to 0 (& FFE0) and swap nametable (^ 0x400)
        else
            this->io.v++;  // just increment the X scroll
        return;
    }
    // INCREMENT VERTICAL SCROLL IN v
    // Cycles 257, copy parts of T to V
    if ((this->line_cycle == 257) && this->rendering_enabled) {
        this->io.v = (this->io.v & 0xFBE0) | (this->io.t & 0x41F);
    }
}

void perform_bg_fetches(THIS) { // Only called from prerender and visible scanlines
    if (!this->rendering_enabled) return;
    u32 in_tile_y = (this->io.v >> 12) & 7; // Y position inside tile

    if (((this->line_cycle > 0) && (this->line_cycle <= 256)) || (this->line_cycle > 320)) {
        // Do memory accesses and shifters
        switch (this->line_cycle & 7) {
            case 1: // nametable, tile #
                this->bg_fetches[0] = NES_PPU_read_effect(this->nes, 0x2000 | (this->io.v & 0xFFF));
                this->bg_tile_fetch_addr = fetch_chr_addr(this->io.bg_pattern_table, this->bg_fetches[0], in_tile_y);
                this->bg_tile_fetch_buffer = 0;
                // Reload shifters if needed
                if (this->line_cycle != 1) { // reload shifter at interval #9 9....257
                    this->bg_shifter = (this->bg_shifter >> 16) | (this->bg_fetches[2] << 16) | (this->bg_fetches[3] << 24);
                    this->bg_palette_shifter = ((this->bg_palette_shifter << 2) | this->bg_fetches[1]) & 0x0F; //(this->bg_palette_shifter >> 8) | (this->bg_fetches[1] << 8);
                }
                break;
            case 3: // attribute table
                ;u32 attrib_addr = 0x23C0 | (this->io.v & 0x0C00) | ((this->io.v >> 4) & 0x38) | ((this->io.v >> 2) & 7);
                u32 shift = ((this->io.v >> 4) & 0x04) | (this->io.v & 0x02);
                this->bg_fetches[1] = (NES_PPU_read_effect(this->nes, attrib_addr) >> shift) & 3;
                break;
            case 5: // low buffer
                this->bg_tile_fetch_buffer = fetch_chr_line_low(this, this->bg_tile_fetch_addr);
                break;
            case 7: // high buffer
                this->bg_tile_fetch_buffer = fetch_chr_line_high(this, this->bg_tile_fetch_addr, this->bg_tile_fetch_buffer);
                this->bg_fetches[2] = this->bg_tile_fetch_buffer & 0xFF;
                this->bg_fetches[3] = (this->bg_tile_fetch_buffer >> 8);
                break;
        }
    }
}

void PPU_scanline_visible(struct NES_PPU* this)
{
    i32 sx = this->line_cycle-1;
    i32 sy = this->nes->clock.ppu_y;
    i32 bo = (sy * 256) + sx;
    if (!this->rendering_enabled) {
        if (this->line_cycle < 256) {
            this->cur_output[bo] = this->CGRAM[0] | this->io.emph_bits;
        }
        return;
    }
    if (this->line_cycle < 1) {
        if (this->nes->clock.ppu_y == 0)
            this->nes->clock.ppu_frame_cycle = 0;
        return;
    }

    if ((this->line_cycle == 1) && (this->nes->clock.ppu_y == 32)) { // Capture scroll info for display
        this->dbg.v = this->io.v;
        this->dbg.t = this->io.t;
        this->dbg.x = this->io.x;
        this->dbg.w = this->io.w;
    }

    cycle_scanline_addr(this);
    oam_evaluate_slow(this);
    perform_bg_fetches(this);

    // Shift out some bits for backgrounds
    u32 bg_shift, bg_color;
    u32 bg_has_pixel = false;
    if (this->io.bg_enable) {
        bg_shift = (((sx & 7) + this->io.x) & 15) * 2;
        bg_color = (this->bg_shifter >> bg_shift) & 3;
        bg_has_pixel = bg_color != 0;
    }
    //u32 sprite_has_pixel = false;
    if (bg_has_pixel) {
        u32 agb = this->bg_palette_shifter;
        if (this->io.x + (sx & 0x07) < 8) agb >>= 2;
        bg_color = this->CGRAM[bg_color | ((agb & 3) << 2)];
    }
    else bg_color = this->CGRAM[0];

    //this->scanline_timer.record_split('bgcolor')


    u32 sprite_priority = 0;
    u32 sprite_color = 0;

    // Check if any sprites need drawing
    //for (u32 m = 0; m < 8; m++) {
    for (i32 m = 7; m >= 0; m--) {
        if ((this->sprite_x_counters[m] >= -8) && (this->sprite_x_counters[m] <= -1) && this->line_cycle < 256) {
            u32 s_x_flip = (this->sprite_attribute_latches[m] & 0x40) >> 6;
            u32 my_color;
            if (s_x_flip) {
                my_color = (this->sprite_pattern_shifters[m] & 0xC000) >> 14;
                this->sprite_pattern_shifters[m] <<= 2;
            } else {
                my_color = (this->sprite_pattern_shifters[m] & 3);
                this->sprite_pattern_shifters[m] >>= 2;
            }
            if (my_color != 0) {
                //sprite_has_pixel = true;
                my_color |= (this->sprite_attribute_latches[m] & 3) << 2;
                sprite_priority = (this->sprite_attribute_latches[m] & 0x20) >> 5;
                sprite_color = this->CGRAM[0x10 + my_color];
                if ((!this->io.sprite0_hit) && (this->sprite0_on_this_line) && (m == 0) && bg_has_pixel && (this->line_cycle < 256)) {
                    //printf("\nSPRITE 0 LINE:%d CYCLE:%d FRAME:%lld", this->nes->clock.ppu_y, this->line_cycle, this->nes->clock.master_frame);
                    this->io.sprite0_hit = 1;
                }
            }
        }
    }

    // Decide background or sprite
    u32 out_color = bg_color;
    if (this->io.sprite_enable) {
        if (sprite_color != 0) {
            if (!bg_has_pixel) {
                out_color = sprite_color;
            } else {
                if (!sprite_priority) out_color = sprite_color;
                else out_color = bg_color;
            }
        }
    }

    this->cur_output[bo] = out_color | this->io.emph_bits;
}

void PPU_scanline_postrender(THIS) {
    // 240, (also 241-260)
    // LITERALLY DO NOTHING
    if ((this->nes->clock.ppu_y == this->nes->clock.timing.vblank_start) && (this->line_cycle == 1)) {
        this->status.nmi_out = 1;
        NES_PPU_update_nmi(this);
    }
}

void new_frame(THIS) {
    //this->nes->clock.lclock = this->nes->clock.cpu_master_clock;
    debugger_report_frame(this->nes->dbg.interface);

    this->display->scan_y = 0;
    this->nes->clock.ppu_y = 0;
    this->nes->clock.frames_since_restart++;
    this->nes->clock.frame_odd = (this->nes->clock.frame_odd + 1) & 1;
    this->nes->clock.master_frame++;
    this->nes->clock.cpu_frame_cycle = 0;
    this->cur_output = this->display->output[this->display->last_written];
    assert((u64)this->cur_output > 1);
    this->display->last_written ^= 1;
}

void set_scanline(THIS, u32 to) {
    if (to == 0) {
        this->render_cycle = &PPU_scanline_visible;
    }
    else if (to == this->nes->clock.timing.post_render_ppu_idle) {
        this->render_cycle = &PPU_scanline_postrender;
    }
    else if (to == this->nes->clock.timing.ppu_pre_render) {
        this->render_cycle = &PPU_scanline_prerender;
    }
}

void new_scanline(THIS) {
    this->display->scan_x = 0;
    this->display->scan_y++;
    if (this->nes->clock.ppu_y == this->nes->clock.timing.ppu_pre_render)
        new_frame(this);
    else
        this->nes->clock.ppu_y++;
        // debugger_copy_scanline_info

    if ((this->nes->clock.ppu_y >= 0) && (this->nes->clock.ppu_y < 240)) {
        struct DBGNESROW *rw = (&this->nes->dbg_data.rows[this->nes->clock.ppu_y]);
        /*rw->io.bg_enable = this->io.bg_enable;
        rw->io.bg_hide_left_8 = this->io.bg_hide_left_8;*/
        rw->io.bg_pattern_table = this->io.bg_pattern_table;
        rw->io.emph_bits = this->io.emph_bits;
        rw->io.x_scroll = (this->io.x | ((this->io.t & 0x1F) << 3));
        rw->io.y_scroll = ((((this->io.t >> 5) & 0x1F) << 3) | ((this->io.t >> 12) & 7));
        rw->io.x_scroll += ((this->io.t >> 10) & 1) * 256;
        rw->io.y_scroll += ((this->io.t >> 11) & 1) * 240;
    }
    debugger_report_line(this->nes->dbg.interface, this->nes->clock.ppu_y);


    /*if (this->nes->clock.ppu_y == this->nes->clock.timing.vblank_start) {
        //this->nes->clock.vblank = 1;
        NES_PPU_update_nmi(this);
    }
    else if (this->nes->clock.ppu_y == this->nes->clock.timing.vblank_end) {
        //this->nes->clock.vblank = 0;
        NES_PPU_update_nmi(this);
    }*/
    set_scanline(this, this->nes->clock.ppu_y);
    this->line_cycle = 0;
}

u32 NES_PPU_cycle(THIS, u32 howmany) {
        for (u32 i = 0; i < howmany; i++) {
            /*i32 r = effect_buffer_get(&this->w2006_buffer, this->nes->clock.ppu_master_clock / this->nes->clock.timing.ppu_divisor);
            if (r != -1) {
                this->io.v = r;
                NES_bus_a12_watch(this->nes, r);
            }*/
            this->render_cycle(this);
            this->rendering_enabled = this->new_rendering_enabled;
            this->line_cycle++;
            this->display->scan_x++;
            this->nes->clock.ppu_frame_cycle++;
            if (this->line_cycle == 341) new_scanline(this);
            this->nes->clock.ppu_master_clock += this->nes->clock.timing.ppu_divisor;
        }
        return howmany;
}


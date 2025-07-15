//
// Created by . on 6/18/25.
//

#include <string.h>
#include <stdio.h>

#include "helpers/color.h"
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


static void update_irqs(struct HUC6270 *this);
static void new_v_state(struct HUC6270 *this, enum HUC6270_states st);
static void update_RCR(struct HUC6270 *this);
static void vblank(struct HUC6270 *this, u32 val);
static void hblank(struct HUC6270 *this, u32 val);

static inline u16 read_VRAM(struct HUC6270 *this, u32 addr)
{
    return (addr & 0x8000) ? 0 : this->VRAM[addr];
}

static inline void write_VRAM(struct HUC6270 *this, u32 addr, u16 val)
{
    DBG_EVENT(this->dbg.events.WRITE_VRAM);
    if (addr < 0x8000)
        this->VRAM[addr] = val;
}


static void setup_new_frame(struct HUC6270 *this)
{
    this->regs.y_counter = 63; // return this +64
    this->regs.yscroll = this->io.BYR.u;
    this->regs.next_yscroll = this->regs.yscroll + 1;
}

static void eval_sprites(struct HUC6270 *this) {
    static const u32 sprite_widths_sprs[2] = {1, 2};
    static const u32 sprite_height_sprs[4] = {1, 2, 2, 4};
    static const u32 sprite_widths_px[2] = {16, 32};
    static const u32 sprite_heights_px[4] = {16, 32, 64, 64};
    this->sprites.num_tiles_on_line = 0;
    for (u32 sn = 0; sn < 64; sn++) {
        u16 *spr = this->SAT + (sn << 2);
        i32 sy = (i32)(spr[0] & 0x3FF);
        if (sy > this->sprites.y_compare) continue;
        u32 cgy = (spr[3] >> 12) & 3;
        u32 height = sprite_heights_px[cgy];

        u32 sprite_line = this->sprites.y_compare - sy;
        if (sprite_line >= height) continue;
        u32 cgx = (spr[3] >> 8) & 1;
        u32 width = sprite_widths_px[cgx];
        i32 sx = (i32)(spr[1] & 0x3FF) - 32;
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
            if (this->sprites.num_tiles_on_line == 16) {
                this->io.STATUS.OR = 1;
                update_irqs(this);
                break;
            }
            struct HUC6270_sprite *tile = &this->sprites.tiles[this->sprites.num_tiles_on_line++];
            tile->num_left = 16;
            tile->x = sx + x;
            tile->original_num = sn;
            tile->triggered = 0;
            tile->priority = spbg; // TODO: verify this isn't !spbg?
            tile->palette = palette;

            // Fetch from +0, +16, +32, +48
            u32 plane0 = read_VRAM(this, addr);
            u32 plane1 = read_VRAM(this, (addr+16) & 0xFFFF);
            u32 plane2 = read_VRAM(this, (addr+32) & 0xFFFF);
            u32 plane3 = read_VRAM(this, (addr+48) & 0xFFFF);

            for (u32 i = 0; i < 16; i++) {
                if (xflip) tile->pattern_shifter >>= 4;
                else tile->pattern_shifter <<= 4;
                u64 data = (plane0 & 1) << 0;
                data |= (plane1 & 1) << 1;
                data |= (plane2 & 1) << 2;
                data |= (plane3 & 1) << 3;
                plane0 >>= 1;
                plane1 >>= 1;
                plane2 >>= 1;
                plane3 >>= 1;
                if (xflip) data <<= 60;
                tile->pattern_shifter |= data;
            }

            if (xflip) addr -= 64;
            else addr += 64;

            if (tile->x < 0) {
                i32 num_to_left = 0 - tile->x;
                tile->pattern_shifter >>= 4 * num_to_left;
                tile->num_left -= num_to_left;
                tile->x = 0;
            }
        }
        if (this->sprites.num_tiles_on_line >= 16) break;
    }
}

static void setup_new_line(struct HUC6270 *this) {
    if (this->timing.v.state == H6S_display) {
        this->regs.yscroll = this->regs.next_yscroll;
        this->regs.next_yscroll = this->regs.yscroll + 1;
        this->latch.sprites_on = this->io.CR.SB;
        this->latch.bg_on = this->io.CR.BB;
        this->bg.y_compare++;
        this->sprites.y_compare++;
        this->regs.y_counter++;
        this->regs.first_render = 1;
        this->regs.draw_clock = 0;
        this->bg.x_tile = (this->io.BXR.u >> 3) & this->bg.x_tiles_mask;
        this->bg.y_tile = (this->regs.yscroll >> 3) & this->bg.y_tiles_mask;
        this->pixel_shifter.num = 0;
        this->regs.x_counter = 0;
        this->regs.draw_delay = 16;

        eval_sprites(this);
    }

    // TODO: this stuff
    this->timing.v.counter--;
    if (this->timing.v.counter < 1) {
        new_v_state(this, (this->timing.v.state + 1) & 3);
    }
}


static void new_h_state(struct HUC6270 *this, enum HUC6270_states st)
{
    this->timing.h.state = st;
    switch(st) {
        case H6S_sync_window:
            this->timing.h.counter = (this->io.HSW+1) << 3;
            //printf("\nSYNC WINDOW %d", this->timing.h.counter);
            break;
        case H6S_wait_for_display:
            this->timing.h.counter = (this->io.HDS+1) << 3;
            //printf("\nWAIT_DISPLAY %d", this->timing.h.counter);
            setup_new_line(this);
            break;
        case H6S_display:
            hblank(this, 0);
            this->timing.h.counter = (this->io.HDW+1) << 3;
            //printf("\nDISPLAY %d", this->timing.h.counter);
            break;
        case H6S_wait_for_sync_window:
            hblank(this, 1);
            this->timing.h.counter = (this->io.HDE+1) << 3;
            //printf("\nWAIT SYNC WINDOW %d", this->timing.h.counter);
            break;
    }
}

static void new_v_state(struct HUC6270 *this, enum HUC6270_states st)
{
    this->timing.v.state = st;
    switch(st) {
        case H6S_sync_window:
            vblank(this, 0);
            this->timing.v.counter = this->io.VSW + 1;
            break;
        case H6S_wait_for_display:
            vblank(this, 0);
            setup_new_frame(this);
            this->timing.v.counter = this->io.VDS + 2;
            break;
        case H6S_display:
            this->timing.v.counter = this->io.VDW.u + 1;
            this->sprites.y_compare = 64;
            this->bg.y_compare = 0;
            this->regs.blank_line = 1;
            break;
        case H6S_wait_for_sync_window:
            vblank(this, 1);
            this->regs.px_out = 0x100;
            this->timing.v.counter = this->io.VCR;
            break;
    }
}

static void force_new_frame(struct HUC6270 *this)
{
    //printf("\nbadly timed vsync!!! %d", this->timing.v.state);
}

static void force_new_line(struct HUC6270 *this)
{
    printf("\nbadly timed hsync!?");
}

void HUC6270_hsync(struct HUC6270 *this, u32 val)
{
    if (this->regs.ignore_hsync) return;
    if (val) { // 0->1 means it's time for HSW to end and/or new line starting at wait for display
        if (this->timing.h.state != H6S_sync_window) {
            force_new_line(this);
        }
        new_h_state(this, H6S_wait_for_display);
    }
}

void HUC6270_vsync(struct HUC6270 *this, u32 val)
{
    if (this->regs.ignore_vsync) return;

    if (val == 1) { // OK, reset our frame-ish!
        if (this->timing.v.state != H6S_sync_window) {
            force_new_frame(this);
        }
        new_v_state(this, H6S_wait_for_display);
    }
}

void HUC6270_cycle(struct HUC6270 *this)
{
    this->timing.h.counter--;
    if (this->timing.h.counter < 1) {
        new_h_state(this, (this->timing.h.state + 1) & 3);
    }
    if ((this->timing.v.state == H6S_display) && (this->timing.h.state == H6S_display)) {
        if (this->regs.draw_delay) {
            this->regs.draw_delay--;
            return;
        }
        // clock a pixel, yo!
        // grab the current 8 pixels
        if (this->pixel_shifter.num == 0) {
            u32 addr = (this->bg.y_tile * this->bg.x_tiles) + this->bg.x_tile;
            u32 entry = read_VRAM(this, addr);
            addr = ((entry & 0xFFF) << 4) + (this->regs.yscroll & 7);
            this->pixel_shifter.palette = (entry >> 12) & 15;
            u32 plane12 = read_VRAM(this, addr);
            u32 plane34 = read_VRAM(this, addr+8);
            this->pixel_shifter.pattern_shifter = tg16_decode_line(plane12, plane34);
            this->pixel_shifter.num = 8;
            this->bg.x_tile = (this->bg.x_tile + 1) & this->bg.x_tiles_mask;
        }
        if (this->regs.first_render) {
            events_view_report_draw_start(this->dbg.evptr);
            u32 scroll_discard = this->io.BXR.u & 7;
            if (scroll_discard) {
                this->pixel_shifter.pattern_shifter >>= 4 * scroll_discard;
                this->pixel_shifter.num -= scroll_discard;
            }
            this->regs.first_render = 0;
        }
        this->pixel_shifter.num--;
        u32 bg_color = (this->pixel_shifter.pattern_shifter & 15);
        this->pixel_shifter.pattern_shifter >>= 4;

        u32 sp_color = 0;
        u32 sp_pal, sp_prio;
        for (i32 spnum = this->sprites.num_tiles_on_line-1; spnum >= 0; spnum--) {
            struct HUC6270_sprite *sp = &this->sprites.tiles[spnum];
            // If not triggered, check if triggered!
            if (!sp->triggered) {
                sp->triggered |= sp->x == this->regs.x_counter;
            }
            // If triggered, clock a pixel!
            if (sp->triggered && sp->num_left) {
                sp->num_left--;
                u64 col = sp->pattern_shifter & 15;
                sp->pattern_shifter >>= 4;
                if (col) {
                    // Sprite #0 hit
                    if ((sp->original_num == 0) && (sp_color != 0) && !this->io.STATUS.CR) {
                        this->io.STATUS.CR = 1;
                        update_irqs(this);
                    }
                    sp_color = col;
                    sp_pal = sp->palette;
                    sp_prio = sp->priority;
                }
            }
        }

        // BGS OFF FOR DEBUG

        if (!this->io.CR.BB) bg_color = 0;
        if (!this->io.CR.SB) sp_color = 0;
        if (bg_color && sp_color) { // Discriminate!
            if (!sp_prio) // Sprite on top!
                sp_color = 0;
            else
                bg_color = 0;
        }


        if (sp_color) { // Sprite color!
            this->regs.px_out = 0x100 | (sp_pal << 4) | sp_color;
        }
        else if (bg_color) { // Background color!
            this->regs.px_out = (this->pixel_shifter.palette << 4) | bg_color;
        }
        else { // Backdrop color!
            this->regs.px_out = 0;
        }
        this->regs.x_counter++;
    }
}

void HUC6270_init(struct HUC6270 *this, struct scheduler_t *scheduler)
{
    memset(this, 0, sizeof(*this));
    this->scheduler = scheduler;
    this->io.bg.x_tiles = 32;
    this->io.bg.y_tiles = 32;
    this->regs.BAT_size = this->io.bg.x_tiles * this->io.bg.y_tiles;
}


void HUC6270_delete(struct HUC6270 *this)
{
}

void HUC6270_reset(struct HUC6270 *this)
{

}

static void update_RCR(struct HUC6270 *this)
{
    u32 signal = this->regs.y_counter == this->io.RCR.u;
    if (!this->io.STATUS.RR && signal) {
        DBG_EVENT(this->dbg.events.HIT_RCR);
        this->io.STATUS.RR = 1;
    }

    //if (this->regs.y_counter == this->io.RCR.u) printf("\nRR HIT! %d", this->bg.y_compare);
    update_irqs(this);
}

static void vram_satb_end(void *ptr, u64 key, u64 clock, u32 jitter)
{
    struct HUC6270 *this = (struct HUC6270 *)ptr;
    this->io.STATUS.DS = 1;
    if (!this->io.DCR.DSR)
        this->regs.vram_satb_pending = 0;
    update_irqs(this);
}

static void vram_vram_end(void *ptr, u64 key, u64 clock, u32 jitter)
{
    struct HUC6270 *this = (struct HUC6270 *)ptr;
    this->io.STATUS.DV = 1;
    update_irqs(this);
}

static void vram_satb(struct HUC6270 *this)
{
    // TODO: make not instant
    scheduler_only_add_abs(this->scheduler, (*this->scheduler->clock) + 256, 0, this, &vram_satb_end, NULL);
    for (u32 i = 0; i < 0x100; i++) {
        u32 addr = (this->io.DVSSR.u + i) & 0xFFFF;
        this->SAT[i] = read_VRAM(this, addr);
    }
}

static void trigger_vram_satb(struct HUC6270 *this)
{
    this->regs.vram_satb_pending = 1;
}

static void trigger_vram_vram(struct HUC6270 *this)
{
    // TODO: make not instant
    if (!this->regs.in_vblank) {
        printf("\nWarn attempt to VRAM-VRAM DMA outside vblank!");
        return;
    }
    scheduler_only_add_abs(this->scheduler, (*this->scheduler->clock) + 256, 0, this, &vram_vram_end, NULL);
    while(true) {
        u16 val = read_VRAM(this, this->io.SOUR.u);
        write_VRAM(this, this->io.DESR.u, val);
        this->io.SOUR.u += this->io.DCR.SI_D ? -1 : 1;
        this->io.DESR.u += this->io.DCR.DI_D ? -1 : 1;

        this->io.LENR.u--;
        if (this->io.LENR.u == 0) break;
    }
}

static void hblank(struct HUC6270 *this, u32 val)
{
    if (val) {
        this->regs.px_out = 0x100;
        update_RCR(this);
    }
    else {

    }
}

static void vblank(struct HUC6270 *this, u32 val)
{
    this->regs.in_vblank = val;
    if (val) {
        this->regs.px_out = 0x100;
        this->io.STATUS.VD = 1;
        update_irqs(this);
        this->bg.x_tiles = this->io.bg.x_tiles;
        this->bg.y_tiles = this->io.bg.y_tiles;
        this->bg.x_tiles_mask = this->bg.x_tiles - 1;
        this->bg.y_tiles_mask = this->bg.y_tiles - 1;

        if (this->regs.vram_satb_pending)
            vram_satb(this);
    }
    else {
        update_irqs(this);
    }
}


static void run_cycle(void *ptr, u64 key, u64 clock, u32 jitter);


static void write_addr(struct HUC6270 *this, u32 val)
{
    this->io.ADDR = val & 0x1F;
}

static void update_ie(struct HUC6270 *this)
{
    u32 ie = (this->io.CR.IE & 7) | ((this->io.CR.IE & 8) << 2);
    ie |= this->io.DCR.DSC << 3; // 0001 to 1000
    ie |= this->io.DCR.DSC << 4; // 0001 to 10000
    this->regs.IE = ie;
}

static void update_irqs(struct HUC6270 *this)
{
    u32 old_line = this->irq.line;
    this->irq.line = !!(this->regs.IE & this->io.STATUS.u);
    if (old_line != this->irq.line) {
        this->irq.update_func(this->irq.update_func_ptr, this->irq.line);
    }
}

static void write_lsb(struct HUC6270 *this, u32 val)
{
    //printf("\nWRITE LSB %x: %02x", this->io.ADDR, val);
    switch(this->io.ADDR) {
        case 0x00: // MAWR
            this->io.MAWR.lo = val;
            return;
        case 0x01: // MARR
            this->io.MARR.lo = val;
            return;
        case 0x02: // VWR
            this->io.VWR.lo = val;
            return;
        case 0x03:
            //this->io.VRR.lo = val;
            return;
        case 0x04:
            printf("\nWARN write reserved huc6270 register %d", this->io.ADDR);
            return;
        case 0x05: // CR
            this->io.CR.lo = val;
            update_ie(this);
            update_irqs(this);
            switch((val >> 4) & 3) {
                case 0:
                    this->regs.ignore_hsync = 0;
                    this->regs.ignore_vsync = 0;
                    break;
                case 1:
                    this->regs.ignore_hsync = 1;
                    this->regs.ignore_vsync = 0;

                    break;
                case 2:
                    printf("\nWARNING INVALID HSYNC/VSYNC COMBO");
                    break;
                case 3:
                    this->regs.ignore_hsync = 1;
                    this->regs.ignore_vsync = 1;
                    break;
            }
            return;
        case 0x06:
            this->io.RCR.lo = val;
            DBG_EVENT(this->dbg.events.WRITE_RCR);
            update_RCR(this);
            return;
        case 0x07: // BGX
            this->io.BXR.lo = val;
            DBG_EVENT(this->dbg.events.WRITE_XSCROLL);
            return;
        case 0x08: // BGY
            this->io.BYR.lo = val;
            DBG_EVENT(this->dbg.events.WRITE_YSCROLL);
            this->regs.next_yscroll = this->io.BYR.u;
            return;
        case 0x09: {
            static const u32 screen_sizes[8][2] = { {32, 32}, {64, 32},
                                                    {128, 32}, {128, 32},
                                                    {32, 64}, {64, 64},
                                                    {128, 64}, {128,64}
                                                    };
            u32 sr = (val >> 4) & 7;
            this->io.bg.x_tiles = screen_sizes[sr][0];
            this->io.bg.y_tiles = screen_sizes[sr][1];
            this->regs.BAT_size = this->io.bg.x_tiles * this->io.bg.y_tiles;
            return; }
        case 0x0A:
            this->io.HSW = val & 0x1F;
            return;
        case 0x0B:
            this->io.HDW = val & 0x7F;
            return;
        case 0x0C:
            this->io.VSW = val & 0x1F;
            return;
        case 0x0D:
            this->io.VDW.lo = val;
            return;
        case 0x0E:
            this->io.VCR = val;
            return;
        case 0x0F:
            this->io.DCR.u = val & 0x1F;
            update_ie(this);
            update_irqs(this);
            return;
        case 0x10:
            this->io.SOUR.lo = val;
            return;
        case 0x11:
            this->io.DESR.lo = val;
        case 0x12:
            this->io.LENR.lo = val;
            return;
        case 0x13:
            this->io.DVSSR.lo = val;
            return;
    }
    printf("\nWRITE LSB NOT IMPL: %02x", this->io.ADDR);
}

static void write_msb(struct HUC6270 *this, u32 val)
{
    //printf("\nWRITE MSB %x: %02x", this->io.ADDR, val);
    switch(this->io.ADDR) {
        case 0x00: // MAWR
            this->io.MAWR.hi = val;
            return;
        case 0x01: // MARR
            this->io.MARR.hi = val;
            this->io.VRR.u = read_VRAM(this, this->io.MARR.u);
            this->io.MARR.u += this->regs.vram_inc;
            return;
        case 0x02: // VWR
            this->io.VWR.hi = val;
            write_VRAM(this, this->io.MAWR.u, this->io.VWR.u);
            this->io.MAWR.u += this->regs.vram_inc;
            return;
        case 0x03:
            //this->io.VRR.hi = val;
            return;
        case 0x04:
            printf("\nWARN write reserved huc6270 register %d", this->io.ADDR);
            return;
        case 0x05:
            this->io.CR.hi = val;
            switch(this->io.CR.IW) {
                case 0:
                    this->regs.vram_inc = 1;
                    break;
                case 1:
                    this->regs.vram_inc = 0x20;
                    break;
                case 2:
                    this->regs.vram_inc = 0x40;
                    break;
                case 3:
                    this->regs.vram_inc = 0x80;
                    break;
            }
            return;
        case 0x06:
            this->io.RCR.hi = val & 3;
            DBG_EVENT(this->dbg.events.WRITE_RCR);
            update_RCR(this);
            return;
        case 0x07: // BGX scroll BXR
            DBG_EVENT(this->dbg.events.WRITE_XSCROLL);
            this->io.BXR.hi = val & 3;
            return;
        case 0x08: // BGY
            this->io.BYR.hi = val & 1;
            DBG_EVENT(this->dbg.events.WRITE_YSCROLL);
            //printf("\nBYR H: %d", this->io.BYR.hi);
            this->regs.next_yscroll = this->io.BYR.u;
            return;
        case 0x09:
            return;
        case 0x0A:
            this->io.HDS = val & 0x7F;
            return;
        case 0x0B:
            this->io.HDE = val & 0x7F;
            return;
        case 0x0C:
            this->io.VDS = val;
            return;
        case 0x0D:
            this->io.VDW.hi = val & 1;
            return;
        case 0x0E:
            return;
        case 0x0F:
            return;
        case 0x10:
            this->io.SOUR.hi = val;
            return;
        case 0x11:
            this->io.DESR.hi = val;
            return;
        case 0x12:
            this->io.LENR.hi = val;
            // Trigger VRAM-VRAM transfer
            trigger_vram_vram(this);
            return;
        case 0x13:
            this->io.DVSSR.hi = val;
            // Trigger VRAM-SATB Transfer
            trigger_vram_satb(this);
            return;
    }
    printf("\nWRITE MSB NOT IMPL: %02x", this->io.ADDR);
}

void HUC6270_write(struct HUC6270 *this, u32 addr, u32 val)
{
    addr &= 3;
    //printf("\nHUC6270 WRITE %06x %02x", addr, val);
    switch(addr) {
        case 0:
            write_addr(this, val);
            return;
        case 1:
            return;
        case 2:
            write_lsb(this, val);
            return;
        case 3:
            write_msb(this, val);
            return;
    }
}

static u32 read_status(struct HUC6270 *this)
{
    u32 v = this->io.STATUS.u;
    this->io.STATUS.u &= 0b1000000;
    update_irqs(this);
    return v;
}

u32 HUC6270_read(struct HUC6270 *this, u32 addr, u32 old)
{
    addr &= 3;
    switch(addr) {
        case 0:
            return read_status(this);
        case 1:
            return 0;
        case 2:
            return this->io.VRR.lo;
        case 3: {
            u32 v = this->io.VRR.hi;

            if (this->io.ADDR == 2) {
                this->io.VRR.u = read_VRAM(this, this->io.MARR.u);
                this->io.MARR.u += this->regs.vram_inc;
            }
            return v;
        }
    }
    printf("\nUnserviced HUC6270 (VDC) read: %06x", addr);
    return old;
}


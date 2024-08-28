//
// Created by Dave on 2/7/2024.
//

#include "stdio.h"
#include "string.h"

#include "helpers/debugger/debugger.h"

#include "sms_gg_vdp.h"
#include "sms_gg.h"
#include "smsgg_debugger.h"

u8 SMSGG_PALETTE[16] = {
        0x00, 0x00, 0x08, 0x0C, 0x10, 0x30, 0x01, 0x3C,
        0x02, 0x03, 0x05, 0x0F, 0x04, 0x33, 0x15, 0x3F
};

static void bg_gfx1(struct SMSGG_VDP* this) {
    u32 nta = ((this->bus->clock.hpos >> 3) & 0x1F) | ((this->bus->clock.vpos  << 2) & 0x3E0) | (this->io.bg_name_table_address << 10);
    u32 pattern = this->VRAM[nta];

    u32 paddr = (this->bus->clock.vpos & 7) | (pattern << 3) | (this->io.bg_pattern_table_address << 11);

    u32 caddr = ((paddr >> 3) & 0x1F) | (this->io.bg_color_table_address << 6);

    u32 color = this->VRAM[caddr];
    u32 index = this->bus->clock.hpos ^ 7;
    if ((this->VRAM[paddr] & (1 << index)) == 0)
        this->bg_color = color & 0x0F;
    else
        this->bg_color = (color >> 4) & 0x0F;
}

static void bg_gfx2(struct SMSGG_VDP* this) {
    u32 nta = ((this->bus->clock.hpos >> 3) & 0x1F) | ((this->bus->clock.vpos  << 2) & 0x3E0) | (this->io.bg_name_table_address << 10);
    u32 pattern = this->VRAM[nta];

    u32 paddr = (this->bus->clock.vpos & 7) | (pattern << 3);
    if (this->bus->clock.vpos >= 64 && this->bus->clock.vpos <= 127) paddr |= (this->io.bg_pattern_table_address & 1) << 11;
    if (this->bus->clock.vpos >= 128 && this->bus->clock.vpos <= 191) paddr |= (this->io.bg_pattern_table_address & 2) << 11;

    u32 caddr = paddr;
    paddr |= (this->io.bg_pattern_table_address & 4) << 11;
    caddr |= (this->io.bg_color_table_address & 0x80) << 4;

    //u32 cmask = ((this->io.bg_color_table_address & 0x7F) << 1) | 1;
    u32 color = this->VRAM[caddr];
    u32 index = this->bus->clock.hpos ^ 7;
    if (!(this->VRAM[paddr] & (1 << index)))
        this->bg_color = color & 0x0F;
    else
        this->bg_color = (color >> 4) & 0x0F;
}

static void bg_gfx3(struct SMSGG_VDP* this) {
    u32 hpos = this->bus->clock.hpos;
    u32 vpos = this->bus->clock.vpos;

    if (hpos < (this->latch.hscroll & 7)) {
        this->bg_color = 0;
        return;
    }

    if ((!this->io.bg_hscroll_lock) || (vpos >= 16)) hpos -= this->latch.hscroll;
    if ((!this->io.bg_vscroll_lock) || (hpos <= 191)) vpos += this->latch.vscroll;
    hpos &= 0xFF;
    vpos &= 0x1FF;

    u32 nta;
    if (this->bg_gfx_vlines == 192) {
        vpos %= 224;
        nta = (this->io.bg_name_table_address & 0x0E) << 10;
        nta += (vpos & 0xF8) << 3;
        nta += ((hpos & 0xF8) >> 3) << 1;
        if (this->variant == SYS_SMS1) {
            // NTA bit 10 (0x400) & with io nta bit 0
            nta &= (0x3BFF | ((this->io.bg_name_table_address & 1) << 10));
        }
    } else {
        vpos &= 0xFF;
        nta = ((this->io.bg_name_table_address & 0x0C) << 10) | 0x700;
        nta += (vpos & 0xF8) << 3;
        nta += ((hpos & 0xF8) >> 3) << 1;
    }

    u32 pattern = this->VRAM[nta] | (this->VRAM[nta | 1] << 8);
    if (pattern & 0x200) hpos ^= 7;
    if (pattern & 0x400) vpos ^= 7;
    u32 palette = (pattern & 0x800) >> 11;
    u32 priority = (pattern & 0x1000) >> 12;

    u32 pta = (vpos & 7) << 2;
    pta |= (pattern & 0x1FF) << 5;

    u32 index = (hpos & 7) ^ 7;
    u32 bmask = (1 << index);
    u32 color = (this->VRAM[pta] & bmask) ? 1 : 0;
    color += (this->VRAM[pta | 1] & bmask) ? 2 : 0;
    color += (this->VRAM[pta | 2] & bmask) ? 4 : 0;
    color += (this->VRAM[pta | 3] & bmask) ? 8 : 0;

    if (color == 0) priority = 0;
    this->bg_color = color;
    this->bg_priority = priority;
    this->bg_palette = palette;
}

static void sprite_gfx2(struct SMSGG_VDP* this) {
    u32 hlimit = (8 << (this->io.sprite_zoom + this->io.sprite_size)) - 1;
    u32 vlimit = hlimit;
    for (u32 i = 0; i < 8; i++) {
        struct SMSGG_object* o = &this->objects[i];
        if (o->y == 0xD0) continue;
        if (this->bus->clock.hpos < o->x) continue;
        if (this->bus->clock.hpos > (o->x + hlimit)) continue;

        u32 x = ((this->bus->clock.hpos - o->x) >> this->io.sprite_zoom);
        u32 y = ((this->bus->clock.vpos - o->y) >> this->io.sprite_zoom);

        u32 addr = (o->pattern << 3) + ((x & 0xF8) << 1) + (y & vlimit);
        addr |= this->io.sprite_pattern_table_address << 11;

        u32 index = x ^ 7;
        u32 bmask = 1 << index;
        if (this->VRAM[addr] & bmask) {
            if (this->sprite_color && this->io.display_enable) { this->io.sprite_collision = 1; break; }
            this->sprite_color = o->color;
        }
    }
}

static void sprite_gfx3(struct SMSGG_VDP* this) {
    u32 hlimit = (8 << this->io.sprite_zoom) - 1;
    u32 vlimit = (8 << (this->io.sprite_zoom + this->io.sprite_size)) - 1;
    for (u32 i = 0; i < 8; i++) {
        struct SMSGG_object *o = &this->objects[i];
        if (o->y == 0xD0) continue;
        if ((this->bus->clock.hpos < o->x) || (this->bus->clock.hpos > (o->x + hlimit))) continue;

        u32 x = (this->bus->clock.hpos - o->x) >> this->io.sprite_zoom;
        u32 y = (this->bus->clock.vpos - o->y) >> this->io.sprite_zoom;

        u32 addr = ((o->pattern << 3) + (y & vlimit)) << 2;
        addr |= (this->io.sprite_pattern_table_address & 4) << 11;

        u32 index = x ^ 7;
        u32 bmask = (1 << index);
        u32 color = (this->VRAM[addr] & bmask) ? 1 : 0;
        color |= (this->VRAM[addr | 1] & bmask) ? 2 : 0;
        color |= (this->VRAM[addr | 2] & bmask) ? 4 : 0;
        color |= (this->VRAM[addr | 3] & bmask) ? 8 : 0;
        if (color == 0) continue;

        if (this->sprite_color && this->io.display_enable) { this->io.sprite_collision = 1; break; }
        this->sprite_color = color;
    }
}


static void update_videomode(struct SMSGG_VDP* this) {
    u32 bottom_row = 192;
    if (this->variant == SYS_SMS1) bottom_row = 192;
    if (false) {}
    else {
        if (this->io.video_mode == 0x0B) bottom_row = 224;
        if (this->io.video_mode == 0x0E) bottom_row = 240;
    }

    this->bus->clock.timing.bottom_rendered_line = bottom_row - 1;
    this->bus->clock.timing.vblank_start = bottom_row + 1;
    this->bus->clock.timing.rendered_lines = bottom_row;

    switch (this->io.video_mode) {
        case 0:
            this->bg_gfx_mode = 1;
            this->bg_gfx = &bg_gfx1;
            this->sprite_gfx = &sprite_gfx2;
            break;
        case 2:
            this->bg_gfx_mode = 2;
            this->bg_gfx = &bg_gfx2;
            this->sprite_gfx = &sprite_gfx2;
            break;
        case 8:
        case 10:
            this->bg_gfx_vlines = 192;
            this->bg_gfx_mode = 3;
            this->bg_gfx = &bg_gfx3;
            this->sprite_gfx = &sprite_gfx3;
            break;
        case 11:
            this->bg_gfx_vlines = 224;
            this->bg_gfx_mode = 3;
            this->bg_gfx = &bg_gfx3;
            this->sprite_gfx = &sprite_gfx3;
            break;
        case 12:
            this->bg_gfx_vlines = 192;
            this->bg_gfx_mode = 3;
            this->bg_gfx = &bg_gfx3;
            this->sprite_gfx = &sprite_gfx3;
            break;
        case 14:
            this->bg_gfx_vlines = 240;
            this->bg_gfx_mode = 3;
            this->bg_gfx = &bg_gfx3;
            this->sprite_gfx = &sprite_gfx3;
            break;
        case 15:
            this->bg_gfx_vlines = 192;
            this->bg_gfx_mode = 3;
            this->bg_gfx = &bg_gfx3;
            this->sprite_gfx = &sprite_gfx3;
            break;
    }
}

static void update_irqs(struct SMSGG_VDP* this)
{
    /*u32 level = 0;
    if (this->io.irq_frame_pending && this->io.irq_frame_enabled) {
        level = 1;
    }
    if (this->io.irq_line_pending && this->io.irq_line_enabled) {
        level = 1;
        //this->io.irq_line_pending = 0;
    }
    SMSGG_bus_notify_IRQ(this->bus, level);*/
    SMSGG_bus_notify_IRQ(this->bus, ((this->io.irq_frame_pending && this->io.irq_frame_enabled) || (this->io.irq_line_pending && this->io.irq_line_enabled)));
}

static void sprite_setup(struct SMSGG_VDP* this)
{
    for (u32 i = 0; i < 8; i++) {
        this->objects[i].y = 0xFF;
    }
    u32 valid = 0;
    u32 vlimit = (8 << (this->io.sprite_zoom + this->io.sprite_size)) - 1;
    for (u32 i = 0; i < 8; i++) this->objects[i].y = 0xD0;

    i32 vpos = (i32)this->bus->clock.vpos;

    u32 attr_addr;
    if (!(this->io.video_mode & 8)) {
        attr_addr = this->io.sprite_attr_table_address << 7;
        for (u32 index = 0; index < 32; index++) {
            i32 y = this->VRAM[attr_addr++];
            if (y == 0xD0) break;
            if (y >= 0xE0) y = (y - 0xFF) & 0x1FF;
            if ((vpos < y) || (vpos > (y + vlimit))) {
                attr_addr += 3;
                continue;
            }

            u32 x = this->VRAM[attr_addr++];
            u32 pattern = this->VRAM[attr_addr++];
            u32 extra = this->VRAM[attr_addr++];

            if (extra & 0x80) x -= 32;

            if (vlimit == (this->io.sprite_zoom ? 31 : 15)) pattern &= 0xFC;

            if (valid == 4) {
                this->io.sprite_overflow = 1;
                this->io.sprite_overflow_index = index;
                break;
            }

            this->objects[valid].x = x;
            this->objects[valid].y = y;
            this->objects[valid].pattern = pattern;
            this->objects[valid].color = extra & 7;
            valid++;
        }
    } else {
        attr_addr = (this->io.sprite_attr_table_address & 0x7E) << 7;
        for (u32 index = 0; index < 64; index++) {
            i32 y = this->VRAM[attr_addr + index] + 1;
            if ((this->bg_gfx_vlines == 192) && (y == 0xD1)) break;
            if (y >= 0xF1) y = (y - 0xFF) & 0x1FF;
            if ((vpos < y) || (vpos > (y + vlimit))) continue;

            u32 x = this->VRAM[attr_addr + 0x80 + (index << 1)];
            u32 pattern = this->VRAM[attr_addr + 0x81 + (index << 1)];

            if (this->io.sprite_shift) x = (x - 8) & 0xFF;
            if (vlimit == (this->io.sprite_zoom ? 31 : 15)) pattern &= 0xFE;

            if (valid == 8) {
                this->io.sprite_overflow = 1;
                this->io.sprite_overflow_index = index;
                break;
            }
            this->objects[valid].x = x;
            this->objects[valid].y = y;
            this->objects[valid].pattern = pattern;
            valid++;
        }
    }    
}

u32 dac_palette(struct SMSGG_VDP* this, u32 index)
{
    if (this->variant != SYS_GG) {
        if (!(this->io.video_mode & 8)) return SMSGG_PALETTE[index & 0x0F];
        return this->CRAM[index] & 0x3F;
    }
    if (this->mode == VDP_SMS) {
        u32 color = this->CRAM[index];
        if (!(this->io.video_mode & 8)) color = SMSGG_PALETTE[index & 0x0F];
        u32 r = (color & 3) | ((color & 3) << 2);
        u32 g = ((color & 0x0C) >> 2) | (color & 0x0C);
        u32 b = ((color & 0x30) >> 4) | (color & 0x30) >> 2;
        return r | (g << 4) | (b << 8);
    }
    if (this->mode == VDP_GG) {
        if (!(this->io.video_mode & 8)) {
            u32 color = SMSGG_PALETTE[index & 7];
            u32 r = (color & 3) | ((color & 3) << 2);
            u32 g = ((color & 0x0C) >> 2) | (color & 0x0C);
            u32 b = ((color & 0x30) >> 4) | (color & 0x30) >> 2;
            return r | (g << 4) | (b << 8);
        }
        return this->CRAM[index];
    }
    return 0;
}

static void dac_gfx(struct SMSGG_VDP* this)
{
    u32 color = dac_palette(this, 16 | this->io.bg_color);

    if (this->io.display_enable) {
        if ((!this->io.left_clip) || (this->bus->clock.hpos >= 8)) {
            if (this->bg_priority || (this->sprite_color == 0)) {
                color = dac_palette(this, (this->bg_palette << 4) | this->bg_color);
            } else if (this->sprite_color != 0) {
                color = dac_palette(this, 16 | this->sprite_color);
            }
        }
    }

    this->cur_output[this->doi] = (u16)color;
    this->doi++;    
}

static void scanline_invisible(struct SMSGG_VDP* this)
{

}

static void scanline_visible(struct SMSGG_VDP* this)
{
    this->bg_color = this->bg_priority = this->bg_palette = 0;
    this->sprite_color = 0;
    if (this->bus->clock.hpos == 0) {
        this->latch.hscroll = this->io.hscroll;

        sprite_setup(this);
        this->doi = (this->bus->clock.vpos * 256);
    }
    if ((this->bus->clock.hpos < 256) && (this->bus->clock.vpos < this->bus->clock.timing.frame_lines)) {
        this->bg_gfx(this);
        this->sprite_gfx(this);
        dac_gfx(this);
    }
}

void SMSGG_VDP_init(struct SMSGG_VDP* this, struct SMSGG* bus, enum jsm_systems variant)
{
    this->variant = variant;
    this->bus = bus;

    this->mode = variant == SYS_GG ? VDP_GG : VDP_SMS;
    
    for (u32 i = 0; i < 8; i++) {
        this->objects[i] = (struct SMSGG_object) {0, 0, 0, 0 };
    }
    
    this->io = (struct SMSGG_VDP_io) {
        .code = 0, .video_mode = 0, .display_enable = 0,
        .bg_name_table_address = 0, .bg_color_table_address = 0,
        .bg_pattern_table_address = 0, .sprite_attr_table_address = 0,
        .sprite_pattern_table_address = 0,.bg_color = 0,
        .hscroll = 0, .vscroll = 0,
        .line_irq_reload = 255,
        .sprite_overflow_index = 0, .sprite_collision = 0,
        .sprite_overflow = 0x1F, .sprite_shift = 0, .sprite_zoom = 0,
        .sprite_size = 0, .left_clip = 0,
        .bg_hscroll_lock = 0, .bg_vscroll_lock = 0,
        
        .irq_frame_pending = 0, .irq_frame_enabled = 0,
        .irq_line_pending = 0, .irq_line_enabled = 0,
        .address = 0
    };
    
    this->latch = (struct SMSGG_VDP_latch) {
        .control = 0,
        .vram = 0,
        .cram = 0,
        .hcounter = 0,
        .hscroll = 0,
        .vscroll = 0
    };
    
    this->bg_color = this->bg_priority = this->bg_palette = 0;
    this->sprite_color = 0;
    
    this->bg_gfx_vlines = 192;
    this->bg_gfx = NULL;
    this->sprite_gfx = NULL;
    
    this->doi = 0;
    
    this->scanline_cycle = &scanline_visible;
}

static void new_frame(struct SMSGG_VDP* this)
{
    event_view_begin_frame(this->bus->dbg.events.view);
    this->display->scan_x = this->display->scan_y = 0;
    this->bus->clock.frames_since_restart++;
    this->bus->clock.vpos = 0;
    this->bus->clock.vdp_frame_cycle = 0;
    this->latch.vscroll = this->io.vscroll;
    this->cur_output = this->display->output[this->display->last_written];
    this->display->last_written ^= 1;
}

static void set_scanline_kind(struct SMSGG_VDP* this, u32 vpos)
{
    if (vpos == 0)
        this->scanline_cycle = &scanline_visible;
    else if (vpos == this->bus->clock.timing.rendered_lines)
        this->scanline_cycle = &scanline_invisible;
}

static void new_scanline(struct SMSGG_VDP* this)
{
    this->display->scan_x = 0;
    this->display->scan_y++;
    this->bus->clock.hpos = 0;
    this->bus->clock.vpos++;
    if (this->bus->clock.vpos == this->bus->clock.timing.frame_lines) {
        new_frame(this);
    }

    debugger_report_line(this->bus->dbg.interface, (i32)this->bus->clock.vpos);
    if (this->bus->clock.vpos <= this->bus->clock.timing.rendered_lines) {
        struct DBGSMSGGROW *rw = (&this->bus->dbg_data.rows[this->bus->clock.vpos]);
        rw->io.hscroll = this->latch.hscroll;
        rw->io.vscroll = this->latch.vscroll;
        rw->io.bg_name_table_address = this->io.bg_name_table_address;
        rw->io.bg_color_table_address = this->io.bg_color_table_address;
        rw->io.bg_pattern_table_address = this->io.bg_pattern_table_address;
        rw->io.bg_color = this->io.bg_color;
        rw->io.bg_hscroll_lock = this->io.bg_hscroll_lock;
        rw->io.bg_vscroll_lock = this->io.bg_vscroll_lock;
        rw->io.num_lines = this->bg_gfx_vlines;
        rw->io.left_clip = this->io.left_clip;

        this->bus->clock.line_counter = (this->bus->clock.line_counter - 1);
        if (this->bus->clock.line_counter < 0) {
            this->bus->clock.line_counter = (i32)this->io.line_irq_reload;
            this->io.irq_line_pending = 1;

            update_irqs(this);
        }
    }
    else {
        this->bus->clock.line_counter = (i32)this->io.line_irq_reload;
    }

    if (this->bus->clock.vpos == this->bus->clock.timing.vblank_start) {
        //if (this->io.irq_frame_enabled) this->io.irq_frame_pending = 1;
        this->io.irq_frame_pending = 1;
        update_irqs(this);
    }

    set_scanline_kind(this, this->bus->clock.vpos);

    if (this->bus->clock.vpos < this->bus->clock.timing.cc_line)
        this->bus->clock.ccounter = (this->bus->clock.ccounter + 1) & 0xFFF;
}

void SMSGG_VDP_cycle(struct SMSGG_VDP* this)
{
    this->scanline_cycle(this);
    this->bus->clock.vdp_frame_cycle += this->bus->clock.vdp_divisor;
    this->bus->clock.hpos++;
    this->display->scan_x++;
    if (this->bus->clock.hpos == 342) new_scanline(this);
}

void SMSGG_VDP_reset(struct SMSGG_VDP* this)
{
    this->io.line_irq_reload = 255;
    this->bus->clock.hpos = 0;
    this->bus->clock.vpos = 0;
    this->bus->clock.ccounter = 0;
    this->io.sprite_overflow_index = 0x1F;
    this->bus->clock.frames_since_restart = 0;
    this->scanline_cycle = &scanline_visible;
    for (u32 i = 0; i < 8; i++) {
        this->objects[i].y = 0xD0;
    }
    memset(this->VRAM, 0, 16384);
    memset(this->CRAM, 0, 64);
    update_videomode(this);
}

u32 SMSGG_VDP_read_hcounter(struct SMSGG_VDP* this)
{
    u32 hcounter = this->latch.hcounter;
    if (hcounter >= 592) hcounter += 340;
    return hcounter >> 2;
}

u32 SMSGG_VDP_read_data(struct SMSGG_VDP* this)
{
    this->latch.control = 0;
    u32 r = this->latch.vram;
    this->latch.vram = this->VRAM[this->io.address];
    this->io.address = (this->io.address + 1) & 0x3FFF;
    return r;
}

void SMSGG_VDP_write_data(struct SMSGG_VDP* this, u32 val)
{
    DBG_EVENT(DBG_SMSGG_EVENT_WRITE_VRAM);
    this->latch.control = 0;
    this->latch.vram = val;
    if (this->io.code <= 2) {
        this->VRAM[this->io.address] = val;
    }
    else {
        if (this->mode == VDP_GG) {
            // even writes store 8-bit data into latch
            // odd writes store 12-bits into CRAM
            if ((this->io.address & 1) == 0)
                this->latch.cram = val;
            else {
                this->CRAM[this->io.address >> 1] = ((val & 0x0F) << 8) | this->latch.cram;
            }

        }
        else {
            // 6 bits for SMS
            this->CRAM[this->io.address] = val & 0x3F;
        }
    }
    this->io.address = (this->io.address + 1) & 0x3FFF;
}

u32 SMSGG_VDP_read_vcounter(struct SMSGG_VDP* this)
{
    if (this->bus->clock.timing.region == NTSC) {
        switch(this->io.video_mode) {
            case 0x0B: // 1011 256x224
                return (this->bus->clock.vpos <= 234) ? this->bus->clock.vpos : (this->bus->clock.vpos - 6);
            case 0x0E: // 1110 256x240 NOT FUNCTIONAL ON NTSC really
                return this->bus->clock.vpos;
            default: // 256x192
                return (this->bus->clock.vpos <= 218) ? this->bus->clock.vpos : (this->bus->clock.vpos - 6);
        }
    }
    else {
        switch(this->io.video_mode) {
            case 0x0B: // 1011 256x224
                return (this->bus->clock.vpos <= 258) ? this->bus->clock.vpos :  (this->bus->clock.vpos - 57);
            case 0x0E: // 1110 256x240
                return (this->bus->clock.vpos <= 266) ? this->bus->clock.vpos : (this->bus->clock.vpos - 56);
            default: // 256x192
                return (this->bus->clock.vpos <= 242) ? this->bus->clock.vpos : (this->bus->clock.vpos - 57);
        }
    }
}

u32 SMSGG_VDP_read_status(struct SMSGG_VDP* this)
{
    this->latch.control = 0;

    u32 val = this->io.sprite_overflow_index | (this->io.sprite_collision << 5) | (this->io.sprite_overflow << 6) | (this->io.irq_frame_pending << 7);

    this->io.sprite_overflow_index = 0x1F;
    this->io.sprite_collision = 0;
    this->io.sprite_overflow = 0;
    this->io.irq_frame_pending = 0;
    this->io.irq_line_pending = 0;

    update_irqs(this);
    return val;
}

static void register_write(struct SMSGG_VDP* this, u32 addr, u32 val)
{
    switch(addr) {
        case 0: // mode control thing, #1
            this->io.video_mode = (this->io.video_mode & 5) | (val & 2) | ((val & 4) << 1);
            this->io.sprite_shift = (val & 8) >> 3;
            this->io.irq_line_enabled = (val & 0x10) >> 4;
            this->io.left_clip = (val & 0x20) >> 5;
            this->io.bg_hscroll_lock = (val & 0x40) >> 6;
            this->io.bg_vscroll_lock = (val & 0x80) >> 7;
            update_irqs(this);
            update_videomode(this);
            return;
        case 1: // mode control thing, #2
            //dbg_break();
            this->io.sprite_zoom = val & 1;
            this->io.sprite_size = (val & 2) >> 1;
            this->io.video_mode = (this->io.video_mode & 0x0A) | ((val & 8) >> 1) | ((val & 0x10) >> 4);
            this->io.irq_frame_enabled = (val & 0x20) >> 5;
            this->io.display_enable = (val & 0x40) >> 6;
            //this->io.irq_frame_pending &= this->io.irq_frame_enabled;
            update_irqs(this);
            update_videomode(this);
            return;
        case 2: // name table base address
            this->io.bg_name_table_address = val & 0x0F;
            return;
        case 3: // color table base address
            this->io.bg_color_table_address = val;
            return;
        case 4: // pattern table base address
            this->io.bg_pattern_table_address = val & 0x07;
            return;
        case 5:
            this->io.sprite_attr_table_address = val & 0x7F;
            return;
        case 6:
            this->io.sprite_pattern_table_address = val & 7;
            return;
        case 7:
            this->io.bg_color = val & 0x0F;
            return;
        case 8:
            DBG_EVENT(DBG_SMSGG_EVENT_WRITE_HSCROLL);
            this->io.hscroll = val;
            return;
        case 9:
            DBG_EVENT(DBG_SMSGG_EVENT_WRITE_VSCROLL);
            this->io.vscroll = val;
            return;
        case 10:
            this->io.line_irq_reload = val;
            return;
    }
    
}

void SMSGG_VDP_write_control(struct SMSGG_VDP* this, u32 val)
{
    if (this->latch.control == 0) {
        this->latch.control = 1;
        this->io.address = (this->io.address & 0xFF00) | val;
        return;
    }

    this->latch.control = 0;
    this->io.address = ((val & 0x3F) << 8) | (this->io.address & 0xC0FF);
    this->io.code = (val & 0xC0) >> 6;

    if (this->io.code == 0) {
        this->latch.vram = this->VRAM[this->io.address];
        this->io.address = (this->io.address + 1) & 0x3FFF;
    }

    if (this->io.code == 2) {
        register_write(this, (this->io.address >> 8) & 0x0F, this->io.address & 0xFF);
    }
}
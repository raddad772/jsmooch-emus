//
// Created by Dave on 2/7/2024.
//

#include <cstdio>
#include <cstring>

#include "sms_gg_vdp.h"
#include "sms_gg.h"
#include "smsgg_debugger.h"
namespace SMSGG {

constexpr u8 SMSGG_PALETTE[16] = {
        0x00, 0x00, 0x08, 0x0C, 0x10, 0x30, 0x01, 0x3C,
        0x02, 0x03, 0x05, 0x0F, 0x04, 0x33, 0x15, 0x3F
};

void VDP::bg_gfx1() {
    u32 nta = ((bus->clock.hpos >> 3) & 0x1F) | ((bus->clock.vpos  << 2) & 0x3E0) | (io.bg_name_table_address << 10);
    u32 pattern = VRAM[nta];

    u32 paddr = (bus->clock.vpos & 7) | (pattern << 3) | (io.bg_pattern_table_address << 11);

    u32 caddr = ((paddr >> 3) & 0x1F) | (io.bg_color_table_address << 6);

    u32 color = VRAM[caddr];
    u32 index = bus->clock.hpos ^ 7;
    if ((VRAM[paddr] & (1 << index)) == 0)
        bg_color = color & 0x0F;
    else
        bg_color = (color >> 4) & 0x0F;
}

void VDP::bg_gfx2() {
    u32 nta = ((bus->clock.hpos >> 3) & 0x1F) | ((bus->clock.vpos  << 2) & 0x3E0) | (io.bg_name_table_address << 10);
    u32 pattern = VRAM[nta];

    u32 paddr = (bus->clock.vpos & 7) | (pattern << 3);
    if (bus->clock.vpos >= 64 && bus->clock.vpos <= 127) paddr |= (io.bg_pattern_table_address & 1) << 11;
    if (bus->clock.vpos >= 128 && bus->clock.vpos <= 191) paddr |= (io.bg_pattern_table_address & 2) << 11;

    u32 caddr = paddr;
    paddr |= (io.bg_pattern_table_address & 4) << 11;
    caddr |= (io.bg_color_table_address & 0x80) << 4;

    //u32 cmask = ((io.bg_color_table_address & 0x7F) << 1) | 1;
    u32 color = VRAM[caddr];
    u32 index = bus->clock.hpos ^ 7;
    if (!(VRAM[paddr] & (1 << index)))
        bg_color = color & 0x0F;
    else
        bg_color = (color >> 4) & 0x0F;
}

void VDP::bg_gfx3() {
    u32 hpos = bus->clock.hpos;
    u32 vpos = bus->clock.vpos;

    if (hpos < (latch.hscroll & 7)) {
        bg_color = 0;
        return;
    }

    if ((!io.bg_hscroll_lock) || (vpos >= 16)) hpos -= latch.hscroll;
    if ((!io.bg_vscroll_lock) || (hpos <= 191)) vpos += latch.vscroll;
    hpos &= 0xFF;
    vpos &= 0x1FF;

    u32 nta;
    if (bg_gfx_vlines == 192) {
        vpos %= 224;
        nta = (io.bg_name_table_address & 0x0E) << 10;
        nta += (vpos & 0xF8) << 3;
        nta += ((hpos & 0xF8) >> 3) << 1;
        if (variant == jsm::systems::SMS1) {
            // NTA bit 10 (0x400) & with io nta bit 0
            nta &= (0x3BFF | ((io.bg_name_table_address & 1) << 10));
        }
    } else {
        vpos &= 0xFF;
        nta = ((io.bg_name_table_address & 0x0C) << 10) | 0x700;
        nta += (vpos & 0xF8) << 3;
        nta += ((hpos & 0xF8) >> 3) << 1;
    }

    u32 pattern = VRAM[nta] | (VRAM[nta | 1] << 8);
    if (pattern & 0x200) hpos ^= 7;
    if (pattern & 0x400) vpos ^= 7;
    u32 palette = (pattern & 0x800) >> 11;
    u32 priority = (pattern & 0x1000) >> 12;

    u32 pta = (vpos & 7) << 2;
    pta |= (pattern & 0x1FF) << 5;

    u32 index = (hpos & 7) ^ 7;
    u32 bmask = (1 << index);
    u32 color = (VRAM[pta] & bmask) ? 1 : 0;
    color += (VRAM[pta | 1] & bmask) ? 2 : 0;
    color += (VRAM[pta | 2] & bmask) ? 4 : 0;
    color += (VRAM[pta | 3] & bmask) ? 8 : 0;

    if (color == 0) priority = 0;
    bg_color = color;
    bg_priority = priority;
    bg_palette = palette;
}

void VDP::sprite_gfx2() {
    u32 hlimit = (8 << (io.sprite_zoom + io.sprite_size)) - 1;
    u32 vlimit = hlimit;
    u32 sl = sprite_limit_override ? 64 : 8;
    for (u32 i = 0; i < sl; i++) {
        VDP_object* o = &objects[i];
        if (o->y == 0xD0) continue;
        if (bus->clock.hpos < o->x) continue;
        if (bus->clock.hpos > (o->x + hlimit)) continue;

        u32 x = ((bus->clock.hpos - o->x) >> io.sprite_zoom);
        u32 y = ((bus->clock.vpos - o->y) >> io.sprite_zoom);

        u32 addr = (o->pattern << 3) + ((x & 0xF8) << 1) + (y & vlimit);
        addr |= io.sprite_pattern_table_address << 11;

        u32 index = x ^ 7;
        u32 bmask = 1 << index;
        if (VRAM[addr] & bmask) {
            if (sprite_color && io.display_enable) { io.sprite_collision = 1; break; }
            sprite_color = o->color;
        }
    }
}

void VDP::sprite_gfx3() {
    u32 hlimit = (8 << io.sprite_zoom) - 1;
    u32 vlimit = (8 << (io.sprite_zoom + io.sprite_size)) - 1;
    u32 sl = sprite_limit_override ? 64 : 8;
    for (u32 i = 0; i < sl; i++) {
        VDP_object *o = &objects[i];
        if (o->y == 0xD0) continue;
        if ((bus->clock.hpos < o->x) || (bus->clock.hpos > (o->x + hlimit))) continue;

        u32 x = (bus->clock.hpos - o->x) >> io.sprite_zoom;
        u32 y = (bus->clock.vpos - o->y) >> io.sprite_zoom;

        u32 addr = ((o->pattern << 3) + (y & vlimit)) << 2;
        addr |= (io.sprite_pattern_table_address & 4) << 11;

        u32 index = x ^ 7;
        u32 bmask = (1 << index);
        u32 color = (VRAM[addr] & bmask) ? 1 : 0;
        color |= (VRAM[addr | 1] & bmask) ? 2 : 0;
        color |= (VRAM[addr | 2] & bmask) ? 4 : 0;
        color |= (VRAM[addr | 3] & bmask) ? 8 : 0;
        if (color == 0) continue;

        if (sprite_color && io.display_enable) { io.sprite_collision = 1; break; }
        sprite_color = color;
    }
}


void VDP::update_videomode() {
    u32 bottom_row = 192;
    if (variant == jsm::systems::SMS1) bottom_row = 192;
    if (io.video_mode == 0x0B) bottom_row = 224;
    if (io.video_mode == 0x0E) bottom_row = 240;

    bus->clock.timing.bottom_rendered_line = bottom_row - 1;
    bus->clock.timing.vblank_start = bottom_row;
    bus->clock.timing.rendered_lines = bottom_row;

    switch (io.video_mode) {
        case 0:
            bg_gfx_mode = 1;
            bg_gfx = &VDP::bg_gfx1;
            sprite_gfx = &VDP::sprite_gfx2;
            break;
        case 2:
            bg_gfx_mode = 2;
            bg_gfx = &VDP::bg_gfx2;
            sprite_gfx = &VDP::sprite_gfx2;
            break;
        case 8:
        case 10:
            bg_gfx_vlines = 192;
            bg_gfx_mode = 3;
            bg_gfx = &VDP::bg_gfx3;
            sprite_gfx = &VDP::sprite_gfx3;
            break;
        case 11:
            bg_gfx_vlines = 224;
            bg_gfx_mode = 3;
            bg_gfx = &VDP::bg_gfx3;
            sprite_gfx = &VDP::sprite_gfx3;
            break;
        case 12:
            bg_gfx_vlines = 192;
            bg_gfx_mode = 3;
            bg_gfx = &VDP::bg_gfx3;
            sprite_gfx = &VDP::sprite_gfx3;
            break;
        case 14:
            bg_gfx_vlines = 240;
            bg_gfx_mode = 3;
            bg_gfx = &VDP::bg_gfx3;
            sprite_gfx = &VDP::sprite_gfx3;
            break;
        case 15:
            bg_gfx_vlines = 192;
            bg_gfx_mode = 3;
            bg_gfx = &VDP::bg_gfx3;
            sprite_gfx = &VDP::sprite_gfx3;
            break;
    }
}

void VDP::update_irqs()
{
    /*u32 level = 0;
    if (io.irq_frame_pending && io.irq_frame_enabled) {
        level = 1;
    }
    if (io.irq_line_pending && io.irq_line_enabled) {
        level = 1;
        //io.irq_line_pending = 0;
    }
    SMSGG_bus_notify_IRQ(bus, level);*/
    bus->notify_IRQ(((io.irq_frame_pending && io.irq_frame_enabled) || (io.irq_line_pending && io.irq_line_enabled)));
}

void VDP::sprite_setup()
{
    u32 valid = 0;
    u32 vlimit = (8 << (io.sprite_zoom + io.sprite_size)) - 1;
    for (u32 i = 0; i < 64; i++) objects[i].y = 0xD0;

    i32 vpos = static_cast<i32>(bus->clock.vpos);

    u32 attr_addr;
    if (!(io.video_mode & 8)) {
        attr_addr = io.sprite_attr_table_address << 7;
        for (u32 index = 0; index < 32; index++) {
            i32 y = VRAM[attr_addr++];
            if (y == 0xD0) break;
            if (y >= 0xE0) y = (y - 0xFF) & 0x1FF;
            if ((vpos < y) || (vpos > (y + vlimit))) {
                attr_addr += 3;
                continue;
            }

            u32 x = VRAM[attr_addr++];
            u32 pattern = VRAM[attr_addr++];
            u32 extra = VRAM[attr_addr++];

            if (extra & 0x80) x -= 32;

            if (vlimit == (io.sprite_zoom ? 31 : 15)) pattern &= 0xFC;

            if (valid == 4) {
                if (!io.sprite_overflow) {
                    io.sprite_overflow = 1;
                    io.sprite_overflow_index = index;
                }
                if (!sprite_limit_override)
                    break;
            }

            objects[valid].x = x;
            objects[valid].y = y;
            objects[valid].pattern = pattern;
            objects[valid].color = extra & 7;
            valid++;
        }
    } else {
        attr_addr = (io.sprite_attr_table_address & 0x7E) << 7;
        for (u32 index = 0; index < 64; index++) {
            i32 y = VRAM[attr_addr + index] + 1;
            if ((bg_gfx_vlines == 192) && (y == 0xD1)) break;
            if (y >= 0xF1) y = (y - 0xFF) & 0x1FF;
            if ((vpos < y) || (vpos > (y + vlimit))) continue;

            u32 x = VRAM[attr_addr + 0x80 + (index << 1)];
            u32 pattern = VRAM[attr_addr + 0x81 + (index << 1)];

            if (io.sprite_shift) x = (x - 8) & 0xFF;
            if (vlimit == (io.sprite_zoom ? 31 : 15)) pattern &= 0xFE;

            if (valid == 8) {
                if (!io.sprite_overflow) {
                    io.sprite_overflow = 1;
                    io.sprite_overflow_index = index;
                }
                if (!sprite_limit_override)
                    break;
            }
            objects[valid].x = x;
            objects[valid].y = y;
            objects[valid].pattern = pattern;
            valid++;
        }
    }
}

u32 VDP::dac_palette(u32 index)
{
    if (variant != jsm::systems::GG) {
        if (!(io.video_mode & 8)) return SMSGG_PALETTE[index & 0x0F];
        return CRAM[index] & 0x3F;
    }
    if (mode == VDP_SMS) {
        u32 color = CRAM[index];
        if (!(io.video_mode & 8)) color = SMSGG_PALETTE[index & 0x0F];
        u32 r = (color & 3) | ((color & 3) << 2);
        u32 g = ((color & 0x0C) >> 2) | (color & 0x0C);
        u32 b = ((color & 0x30) >> 4) | (color & 0x30) >> 2;
        return r | (g << 4) | (b << 8);
    }
    if (mode == VDP_GG) {
        if (!(io.video_mode & 8)) {
            u32 color = SMSGG_PALETTE[index & 7];
            u32 r = (color & 3) | ((color & 3) << 2);
            u32 g = ((color & 0x0C) >> 2) | (color & 0x0C);
            u32 b = ((color & 0x30) >> 4) | (color & 0x30) >> 2;
            return r | (g << 4) | (b << 8);
        }
        return CRAM[index];
    }
    return 0;
}

void VDP::dac_gfx()
{
    u32 color = dac_palette(16 | io.bg_color);

    if (io.display_enable) {
        if ((!io.left_clip) || (bus->clock.hpos >= 8)) {
            if (bg_priority || (sprite_color == 0)) {
                color = dac_palette((bg_palette << 4) | bg_color);
            } else if (sprite_color != 0) {
                color = dac_palette(16 | sprite_color);
            }
        }
    }

    cur_output[doi] = (u16)color;
    doi++;
}

void VDP::scanline_invisible()
{

}

void VDP::scanline_visible()
{
    bg_color = bg_priority = bg_palette = 0;
    sprite_color = 0;
    if (bus->clock.hpos == 0) {
        latch.hscroll = io.hscroll;

        sprite_setup();
        doi = (bus->clock.vpos * 256);
    }
    if ((bus->clock.hpos < 256) && (bus->clock.vpos < bus->clock.timing.frame_lines)) {
        (this->*bg_gfx)();
        (this->*sprite_gfx)();
        dac_gfx();
    }
}

VDP::VDP(core* parent, jsm::systems in_variant) : bus(parent), variant(in_variant) {
    if (variant == jsm::systems::SG1000) sprite_limit = 4;
    else sprite_limit = 8;
    mode = variant == jsm::systems::GG ? VDP_GG : VDP_SMS;

    for (auto & object : objects) {
        object = (VDP_object) {0, 0, 0, 0 };
    }

    scanline_cycle = &VDP::scanline_visible;
}

void VDP::new_frame()
{
    debugger_report_frame(bus->dbg.interface);
    display->scan_x = display->scan_y = 0;
    bus->clock.frames_since_restart++;
    bus->clock.vpos = 0;
    bus->clock.vdp_frame_cycle = 0;
    latch.vscroll = io.vscroll;
    cur_output = static_cast<u16 *>(display->output[display->last_written]);
    display->last_written ^= 1;
}

void VDP::set_scanline_kind(u32 vpos)
{
    if (vpos < bus->clock.timing.rendered_lines)
        scanline_cycle = &VDP::scanline_visible;
    else
        scanline_cycle = &VDP::scanline_invisible;
}

void VDP::new_scanline()
{
    display->scan_x = 0;
    display->scan_y++;
    bus->clock.hpos = 0;
    bus->clock.vpos++;
    if (bus->clock.vpos == bus->clock.timing.frame_lines) {
        new_frame();
    }

    debugger_report_line(bus->dbg.interface, (i32)bus->clock.vpos);
    if (bus->clock.vpos <= bus->clock.timing.rendered_lines) {
        auto *rw = &bus->dbg_data.rows[bus->clock.vpos];
        rw->io.hscroll = latch.hscroll;
        rw->io.vscroll = latch.vscroll;
        rw->io.bg_name_table_address = io.bg_name_table_address;
        rw->io.bg_color_table_address = io.bg_color_table_address;
        rw->io.bg_pattern_table_address = io.bg_pattern_table_address;
        rw->io.bg_color = io.bg_color;
        rw->io.bg_hscroll_lock = io.bg_hscroll_lock;
        rw->io.bg_vscroll_lock = io.bg_vscroll_lock;
        rw->io.num_lines = bg_gfx_vlines;
        rw->io.left_clip = io.left_clip;

        bus->clock.line_counter = (bus->clock.line_counter - 1);
        if (bus->clock.line_counter < 0) {
            bus->clock.line_counter = (i32)io.line_irq_reload;
            io.irq_line_pending = 1;

            update_irqs();
        }
    }
    else {
        bus->clock.line_counter = (i32)io.line_irq_reload;
    }

    if (bus->clock.vpos == bus->clock.timing.vblank_start) {
        //if (io.irq_frame_enabled) io.irq_frame_pending = 1;
        io.irq_frame_pending = 1;
        update_irqs();
    }

    set_scanline_kind(bus->clock.vpos);

    if (bus->clock.vpos < bus->clock.timing.cc_line)
        bus->clock.ccounter = (bus->clock.ccounter + 1) & 0xFFF;
}

void VDP::cycle()
{
    (this->*scanline_cycle)();
    bus->clock.vdp_frame_cycle += bus->clock.vdp_divisor;
    bus->clock.hpos++;
    display->scan_x++;
    if (bus->clock.hpos == 342) new_scanline();
}

void VDP::reset()
{
    io.line_irq_reload = 255;
    bus->clock.hpos = 0;
    bus->clock.vpos = 0;
    bus->clock.ccounter = 0;
    io.sprite_overflow_index = 0x1F;
    bus->clock.frames_since_restart = 0;
    scanline_cycle = &VDP::scanline_visible;
    for (u32 i = 0; i < 64; i++) {
        objects[i].y = 0xD0;
    }
    memset(VRAM, 0, 16384);
    memset(CRAM, 0, 64);
    update_videomode();
}

u32 VDP::read_hcounter()
{
    u32 hcounter = latch.hcounter;
    if (hcounter >= 592) hcounter += 340;
    return hcounter >> 2;
}

u32 VDP::read_data()
{
    latch.control = 0;
    u32 r = latch.vram;
    latch.vram = VRAM[io.address];
    io.address = (io.address + 1) & 0x3FFF;
    return r;
}

void VDP::write_data(u32 val)
{
    DBG_EVENT(DBG_SMSGG_EVENT_WRITE_VRAM);
    latch.control = 0;
    latch.vram = val;
    if (io.code <= 2) {
        VRAM[io.address] = val;
    }
    else {
        if (mode == VDP_GG) {
            // even writes store 8-bit data into latch
            // odd writes store 12-bits into CRAM
            if ((io.address & 1) == 0)
                latch.cram = val;
            else {
                CRAM[(io.address >> 1) & 31] = ((val & 0x0F) << 8) | latch.cram;
            }

        }
        else {
            // 6 bits for SMS
            CRAM[io.address & 31] = val & 0x3F;
        }
    }
    io.address = (io.address + 1) & 0x3FFF;
}

u32 VDP::read_vcounter()
{
    if (bus->clock.timing.region == jsm::display_standards::NTSC) {
        switch(io.video_mode) {
            case 0x0B: // 1011 256x224
                return (bus->clock.vpos <= 234) ? bus->clock.vpos : (bus->clock.vpos - 6);
            case 0x0E: // 1110 256x240 NOT FUNCTIONAL ON NTSC really
                return bus->clock.vpos;
            default: // 256x192
                return (bus->clock.vpos <= 218) ? bus->clock.vpos : (bus->clock.vpos - 6);
        }
    }
    else {
        switch(io.video_mode) {
            case 0x0B: // 1011 256x224
                return (bus->clock.vpos <= 258) ? bus->clock.vpos :  (bus->clock.vpos - 57);
            case 0x0E: // 1110 256x240
                return (bus->clock.vpos <= 266) ? bus->clock.vpos : (bus->clock.vpos - 56);
            default: // 256x192
                return (bus->clock.vpos <= 242) ? bus->clock.vpos : (bus->clock.vpos - 57);
        }
    }
}

u32 VDP::read_status()
{
    latch.control = 0;

    u32 val = io.sprite_overflow_index | (io.sprite_collision << 5) | (io.sprite_overflow << 6) | (io.irq_frame_pending << 7);

    io.sprite_overflow_index = 0x1F;
    io.sprite_collision = 0;
    io.sprite_overflow = 0;
    io.irq_frame_pending = 0;
    io.irq_line_pending = 0;

    update_irqs();
    return val;
}

void VDP::register_write(u32 addr, u32 val)
{
    switch(addr) {
        case 0: // mode control thing, #1
            io.video_mode = (io.video_mode & 5) | (val & 2) | ((val & 4) << 1);
            io.sprite_shift = (val & 8) >> 3;
            io.irq_line_enabled = (val & 0x10) >> 4;
            io.left_clip = (val & 0x20) >> 5;
            io.bg_hscroll_lock = (val & 0x40) >> 6;
            io.bg_vscroll_lock = (val & 0x80) >> 7;
            update_irqs();
            update_videomode();
            return;
        case 1: // mode control thing, #2
            //dbg_break();
            io.sprite_zoom = val & 1;
            io.sprite_size = (val & 2) >> 1;
            io.video_mode = (io.video_mode & 0x0A) | ((val & 8) >> 1) | ((val & 0x10) >> 4);
            io.irq_frame_enabled = (val & 0x20) >> 5;
            io.display_enable = (val & 0x40) >> 6;
            //io.irq_frame_pending &= io.irq_frame_enabled;
            update_irqs();
            update_videomode();
            return;
        case 2: // name table base address
            io.bg_name_table_address = val & 0x0F;
            return;
        case 3: // color table base address
            io.bg_color_table_address = val;
            return;
        case 4: // pattern table base address
            io.bg_pattern_table_address = val & 0x07;
            return;
        case 5:
            io.sprite_attr_table_address = val & 0x7F;
            return;
        case 6:
            io.sprite_pattern_table_address = val & 7;
            return;
        case 7:
            io.bg_color = val & 0x0F;
            return;
        case 8:
            DBG_EVENT(DBG_SMSGG_EVENT_WRITE_HSCROLL);
            io.hscroll = val;
            return;
        case 9:
            DBG_EVENT(DBG_SMSGG_EVENT_WRITE_VSCROLL);
            io.vscroll = val;
            return;
        case 10:
            io.line_irq_reload = val;
            return;
    }
}

void VDP::write_control(u32 val)
{
    if (latch.control == 0) {
        latch.control = 1;
        io.address = (io.address & 0xFF00) | val;
        return;
    }

    latch.control = 0;
    io.address = ((val & 0x3F) << 8) | (io.address & 0xC0FF);
    io.code = (val & 0xC0) >> 6;

    if (io.code == 0) {
        latch.vram = VRAM[io.address];
        io.address = (io.address + 1) & 0x3FFF;
    }

    if (io.code == 2) {
        register_write((io.address >> 8) & 0x0F, io.address & 0xFF);
    }
}
}
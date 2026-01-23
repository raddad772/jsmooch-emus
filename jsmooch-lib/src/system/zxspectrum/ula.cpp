//
// Created by . on 5/2/24.
//

#include <cstring>
#include <cstdio>
#include <cassert>
#include "helpers/int.h"
#include "helpers/physical_io.h"

#include "zxspectrum.h"
#include "zxspectrum_bus.h"
#include "ula.h"
namespace ZXSpectrum {

static constexpr u32 ZXSpectrum_keyboard_halfrows[8][5] = {
        { JK_CAPS, JK_Z, JK_X, JK_C, JK_V },
        { JK_A, JK_S, JK_D, JK_F, JK_G },
        { JK_Q, JK_W, JK_E, JK_R, JK_T },
        { JK_1, JK_2, JK_3, JK_4, JK_5 },
        { JK_0, JK_9, JK_8, JK_7, JK_6 },
        { JK_P, JK_O, JK_I, JK_U, JK_Y },
        { JK_ENTER, JK_L, JK_K, JK_J, JK_H },
        { JK_SPACE, JK_SHIFT, JK_M, JK_N, JK_B }
};

static constexpr u32 ZXSpectrum_keyboard_halfrows_consts[8] = {
        0x08, 0x10, 0x04, 0x20, 0x02, 0x40, 0x01, 0x80
};


ULA::ULA(variants variant_in, core *parent) : variant(variant_in), bus(parent)
{
    scanline_func = &ULA::scanline_vblank;
}


void ULA::scanline_vblank()
{
    bus->clock.contended = false;
    if (bus->clock.ula_y == 0) {
        switch(bus->clock.ula_x) {
            case 16: // IRQ up
                if (!first_vblank) bus->notify_IRQ(1);
                break;
            case 80: // IRQ down
                if (!first_vblank) bus->notify_IRQ(0);
                first_vblank = false;
                break;
        }
    }
}

void ULA::scanline_border_top()
{
    if ((screen_x >= 0) && (screen_x < 352)) {
        u64 bo = (352 * screen_y) + screen_x;
        cur_output[bo] = io.border_color;
    }
}

void ULA::scanline_border_bottom()
{
    scanline_border_top();
}

void ULA::scanline_visible()
{
    
    /* each scanline is 448 pixels wide.
    96 pixels of nothing
    48 pixels of border
    256 pixels of draw (read pattern bg, attrib, bg+1, attrib, none, none, none, none)
    48 pixels of border
    */
    // Border
    if (screen_x < 0) return;
    u32 bo = (screen_y * 352) + screen_x;
    if ((screen_x < 48) ||
        (screen_x >= 304)) {
        bus->clock.contended = false;
        cur_output[bo] = io.border_color;
        return;
    }
    // OK we are in drawing area
    i32 dx = screen_x - 48;
    i32 dy = screen_y - 56;
    switch(dx & 7) {
        case 0: { // Contention off
            bg_shift = next_bg_shift;
            bus->clock.contended = false;
            u32 brt = (next_attr & 0x40) ? 8 : 0;
            attr.flash = ((next_attr & 0x80) >> 7) & bus->clock.flash.bit;
            attr.colors[0] = ((next_attr >> 3) & 7) + brt;
            attr.colors[1] = (next_attr & 7) + brt;
            break; }
        case 2:
            bus->clock.contended = true;
            break;
        case 6: {// fetch next bg
            u32 addr = 0x4000 | ((dy & 0xC0) << 5) | ((dy & 7) << 8) | ((dy & 0x38) << 2) | (dx >> 3);
            u32 val = bus->ULA_readmem(addr);
            // Only 2 shifts left before we need this data
            next_bg_shift = val;
            break; }
        case 7: // next next attr
            next_attr = bus->ULA_readmem(0x5800 + ((dy >> 3) * 0x20) + (dx >> 3));
            break;
    }

    u32 out_bit = ((bg_shift & 0x80) >> 7) ^ attr.flash;
    bg_shift <<= 1;
    cur_output[bo] = attr.colors[out_bit];
}

void ULA::reset()
{
    bus->clock.ula_frame_cycle = bus->clock.ula_x = bus->clock.ula_y = 0;
    bus->clock.contended = false;
    bus->clock.flash.bit = 0;
    bus->clock.flash.count = 16;
    bus->clock.frames_since_restart = 0;
    bus->bank.disable = 0;

    bg_shift = 0;
    scanline_func = &ULA::scanline_vblank;
    first_vblank = true;
    screen_x = screen_y = 0;
    next_attr = 0;
}

u32 ULA::get_keypress(JKEYS key)
{
    physical_io_device *d = &keyboard_ptr.get();
    JSM_KEYBOARD *kbd = &d->keyboard;
    i32 v = -1;
    for (u32 i = 0; i < kbd->num_keys; i++) {
        if (kbd->key_defs[i] == key) {
            v = static_cast<i32>(kbd->key_states[i]);
        }
    }
    if (v == -1) {
        printf("\nUNKNOWN KEY %d", key);
    }
    return v == 0xFFFFFFFF ? 0 : v;
}

u8 ULA::kb_scan_row(const u32 *row) {
    u32 out = 0xE0;
    for (u32 i = 0; i < 5; i++) {
        u32 key = row[i];
        u32 kp = get_keypress(static_cast<JKEYS>(key)) ^ 1;
        assert((kp >=0 ) && (kp <= 1));
        out |= kp << i;
    }
    return static_cast<u8>(out);
}

u8 ULA::reg_read(u16 addr) {
    if ((addr & 1) == 1) {
        printf("\nUHOH IN TO %04x", addr);
        return 0xFF;
    }

    u32 hi = (addr >> 8) ^ 0xFF;
    u32 out = 0xFF;
    for (u32 i = 0; i < 8; i++) {
        if (hi & (1 << i)) out &= kb_scan_row(ZXSpectrum_keyboard_halfrows[i]);
    }
    
    return out;
}

void ULA::reg_write(u16 addr, u8 val)
{
    if (addr & 1) {
        printf("\nUHOH OUT TO %04x", addr);
        return;
    }
    if ((variant == spectrum128) && ((addr & 0x7FFD) == addr)) {
        // bits 0-2 16k RAM page for memory at 0xc000
        // bit 3 normal (bank 5) or shadow (bank 7) ram for display
        // bit 4 ROM select. lower 16 or upper 16k 0 or 1 I think
        // bit 5 if set, disables paging
        if ((val >> 5) & 1)
            bus->bank.disable = 1;
        if (bus->bank.disable) {
            bus->bank.RAM[2] = bus->RAM;
            bus->bank.display = bus->RAM + (0x4000 * 5);
            bus->bank.ROM = bus->ROM;

        }
        else {
            bus->bank.RAM[2] = bus->RAM + (0x4000 * (val & 7));
            bus->bank.display = bus->RAM + (0x4000 * ((val & 8) ? 7 : 5));
            bus->bank.ROM = bus->ROM + (0x4000 * (val >> 4));
        }
        return;
    }
    
    io.border_color = val & 7;
}

void ULA::new_scanline()
{
    bus->clock.ula_x = 0;
    display->scan_x = 0;
    display->scan_y++;
    printf("\nNEW SCANLINE Y:%d X:%d", display->scan_y, screen_x);
    screen_x = -96;
    bus->clock.ula_y++;
    screen_y++;
    if (bus->clock.ula_y >= bus->clock.screen_bottom) {
        bus->clock.ula_y = 0;
        display->scan_y = 0;
        bus->clock.ula_frame_cycle = 0;
        screen_y = -8;
        bus->clock.frames_since_restart++;
        bus->clock.flash.count--;
        if (bus->clock.flash.count <= 0) {
            bus->clock.flash.count = 16;
            bus->clock.flash.bit ^= 1;
        }

        // Swap buffer we're drawing to...
        cur_output = static_cast<u8 *>(display->output[display->last_written]);
        display->last_written ^= 1;
    }

    /*lines 0-7 are vblank
    lines 8-63 are upper border
    lines 64-255 are visible
    lines 256-311 are lower border*/
    switch(bus->clock.ula_y) {
        case 0: // lines 0-7 are vblank
            scanline_func = &ULA::scanline_vblank;
            break;
        case 16: // lines 8-63 are upper border. 8-62 on 128k
            //switch(variant) // Assume this was a typo!
            scanline_func = &ULA::scanline_border_top;
            break;
        case 63:
            if (variant == spectrum128)
                scanline_func = &ULA::scanline_visible;
            break;
        case 64: // lines 64-255 are visible
            scanline_func = &ULA::scanline_visible;
            break;
        case 255:
            if (variant == spectrum128)
                scanline_func = &ULA::scanline_border_bottom;
            break;
        case 256: // 256-311 or 310 are lower border
            scanline_func = &ULA::scanline_border_bottom;
            break;
    }
    
}

void ULA::cycle()
{
    (this->*scanline_func)();
    bus->clock.ula_x++;
    screen_x++;
    display->scan_x++;
    bus->clock.ula_frame_cycle++;
    if (bus->clock.ula_x >= bus->clock.screen_right) new_scanline();
}
}

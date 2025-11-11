//
// Created by . on 5/2/24.
//

#include <string.h>
#include <stdio.h>
#include <assert.h>
#include "helpers/int.h"
#include "helpers/physical_io.h"

#include "zxspectrum.h"
#include "ula.h"


#define BTHIS struct ZXSpectrum_ULA* this = &bus->ula

static u32 ZXSpectrum_keyboard_halfrows[8][5] = {
        { JK_CAPS, JK_Z, JK_X, JK_C, JK_V },
        { JK_A, JK_S, JK_D, JK_F, JK_G },
        { JK_Q, JK_W, JK_E, JK_R, JK_T },
        { JK_1, JK_2, JK_3, JK_4, JK_5 },
        { JK_0, JK_9, JK_8, JK_7, JK_6 },
        { JK_P, JK_O, JK_I, JK_U, JK_Y },
        { JK_ENTER, JK_L, JK_K, JK_J, JK_H },
        { JK_SPACE, JK_SHIFT, JK_M, JK_N, JK_B }
};

static u32 ZXSpectrum_keyboard_halfrows_consts[8] = {
        0x08, 0x10, 0x04, 0x20, 0x02, 0x40, 0x01, 0x80
};

static u32 ula_readmem(ZXSpectrum* this, u16 addr) {
    return this->bank.display[addr & 0x3FFF];
}

static void scanline_vblank(ZXSpectrum* bus)
{
    BTHIS;
    bus->clock.contended = 0;
    if (bus->clock.ula_y == 0) {
        switch(bus->clock.ula_x) {
            case 16: // IRQ up
                if (!this->first_vblank) ZXSpectrum_notify_IRQ(bus, 1);
                break;
            case 80: // IRQ down
                if (!this->first_vblank) ZXSpectrum_notify_IRQ(bus, 0);
                this->first_vblank = false;
                break;
        }
    }
}

static void scanline_border_top(ZXSpectrum* bus)
{
    BTHIS;
    if ((this->screen_x >= 0) && (this->screen_x < 352)) {
        u64 bo = (352 * this->screen_y) + this->screen_x;
        this->cur_output[bo] = this->io.border_color;
    }
}

static void scanline_border_bottom(ZXSpectrum* bus)
{
    scanline_border_top(bus);
}

static void scanline_visible(ZXSpectrum* bus)
{
    BTHIS;
    /* each scanline is 448 pixels wide.
    96 pixels of nothing
    48 pixels of border
    256 pixels of draw (read pattern bg, attrib, bg+1, attrib, none, none, none, none)
    48 pixels of border
    */
    // Border
    if (this->screen_x < 0) return;
    u32 bo = (this->screen_y * 352) + this->screen_x;
    if (((this->screen_x >= 0) && (this->screen_x < 48)) ||
        (this->screen_x >= 304)) {
        bus->clock.contended = 0;
        this->cur_output[bo] = this->io.border_color;
        return;
    }
    // OK we are in drawing area
    i32 dx = this->screen_x - 48;
    i32 dy = this->screen_y - 56;
    switch(dx & 7) {
        case 0: // Contention off
            this->bg_shift = this->next_bg_shift;
            bus->clock.contended = 0;
            u32 brt = (this->next_attr & 0x40) ? 8 : 0;
            this->attr.flash = ((this->next_attr & 0x80) >> 7) & bus->clock.flash.bit;
            this->attr.colors[0] = ((this->next_attr >> 3) & 7) + brt;
            this->attr.colors[1] = (this->next_attr & 7) + brt;
            break;
        case 2:
            bus->clock.contended = 1;
            break;
        case 6: {// fetch next bg
            u32 addr = 0x4000 | ((dy & 0xC0) << 5) | ((dy & 7) << 8) | ((dy & 0x38) << 2) | (dx >> 3);
            u32 val = ula_readmem(bus, addr);
            // Only 2 shifts left before we need this data
            this->next_bg_shift = val;
            break; }
        case 7: // next next attr
            this->next_attr = ula_readmem(bus, 0x5800 + ((dy >> 3) * 0x20) + (dx >> 3));
            break;
    }

    u32 out_bit = ((this->bg_shift & 0x80) >> 7) ^ this->attr.flash;
    this->bg_shift <<= 1;
    this->cur_output[bo] = this->attr.colors[out_bit];
}

void ZXSpectrum_ULA_init(ZXSpectrum* bus, enum ZXSpectrum_variants variant)
{
    struct ZXSpectrum_ULA *this = &bus->ula;
    memset(this, 0, sizeof(ZXSpectrum_ULA));
    this->variant = variant;

    switch(bus->variant) {
        case ZXS_spectrum128:
            bus->clock.screen_bottom = 311;
            bus->clock.screen_right = 228 * 2;
            bus->clock.scanlines_before_frame = 63;
            break;
        case ZXS_spectrum48:
            bus->clock.screen_bottom = 312;
            bus->clock.screen_right = 224 * 2;
            bus->clock.scanlines_before_frame = 64;
            break;
        default:
            assert(1==2);
    }

    this->scanline_func = &scanline_vblank;
    this->first_vblank = 1;

    this->screen_x = this->screen_y = 0;
}

void ZXSpectrum_ULA_delete(ZXSpectrum_ULA* this)
{

}

void ZXSpectrum_ULA_reset(ZXSpectrum* bus)
{
    BTHIS;
    bus->clock.ula_frame_cycle = bus->clock.ula_x = bus->clock.ula_y = 0;
    bus->clock.contended = 0;
    bus->clock.flash.bit = 0;
    bus->clock.flash.count = 16;
    bus->clock.frames_since_restart = 0;
    bus->bank.disable = 0;

    this->bg_shift = 0;
    this->scanline_func = &scanline_vblank;
    this->first_vblank = true;
    this->screen_x = this->screen_y = 0;
    this->next_attr = 0;
}

static u32 get_keypress(ZXSpectrum* bus, enum JKEYS key)
{
    struct physical_io_device *d = cvec_get(bus->ula.keyboard_devices, bus->ula.keyboard_device_index);
    struct JSM_KEYBOARD *kbd = &d->keyboard;
    i32 v = -1;
    for (u32 i = 0; i < kbd->num_keys; i++) {
        if (kbd->key_defs[i] == key) {
            v = (i32)kbd->key_states[i];
        }
    }
    if (v == -1) {
        printf("\nUNKNOWN KEY %d", key);
    }
    return v == 0xFFFFFFFF ? 0 : v;
}

static u32 kb_scan_row(ZXSpectrum* bus, const u32 *row) {
    u32 out = 0xE0;
    for (u32 i = 0; i < 5; i++) {
        u32 key = row[i];
        u32 kp = get_keypress(bus, key) ^ 1;
        assert((kp >=0) && (kp <= 1));
        out |= kp << i;
    }
    return out;
}

u32 ZXSpectrum_ULA_reg_read(ZXSpectrum* bus, u32 addr) {
    BTHIS;
    if ((addr & 1) == 1) {
        printf("\nUHOH IN TO %04x", addr);
        return 0xFF;
    }

    u32 hi = (addr >> 8) ^ 0xFF;
    u32 out = 0xFF;
    for (u32 i = 0; i < 8; i++) {
        if (hi & (1 << i)) out &= kb_scan_row(bus, ZXSpectrum_keyboard_halfrows[i]);
    }
    
    return out;
}

void ZXSpectrum_ULA_reg_write(ZXSpectrum* bus, u32 addr, u32 val)
{
    BTHIS;
    if (addr & 1) {
        printf("\nUHOH OUT TO %04x", addr);
        return;
    }
    if ((this->variant == ZXS_spectrum128) && ((addr & 0x7FFD) == addr)) {
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
    
    this->io.border_color = val & 7;
}

static void new_scanline(ZXSpectrum* bus)
{
    BTHIS;
    bus->clock.ula_x = 0;
    this->display->scan_x = 0;
    this->display->scan_y++;
    this->screen_x = -96;
    bus->clock.ula_y++;
    this->screen_y++;
    if (bus->clock.ula_y == bus->clock.screen_bottom) {
        bus->clock.ula_y = 0;
        this->display->scan_y = 0;
        bus->clock.ula_frame_cycle = 0;
        this->screen_y = -8;
        bus->clock.frames_since_restart++;
        bus->clock.flash.count--;
        if (bus->clock.flash.count <= 0) {
            bus->clock.flash.count = 16;
            bus->clock.flash.bit ^= 1;
        }

        // Swap buffer we're drawing to...
        this->cur_output = this->display->output[this->display->last_written];
        this->display->last_written ^= 1;
    }

    /*lines 0-7 are vblank
    lines 8-63 are upper border
    lines 64-255 are visible
    lines 256-311 are lower border*/
    switch(bus->clock.ula_y) {
        case 0: // lines 0-7 are vblank
            this->scanline_func = &scanline_vblank;
            break;
        case 16: // lines 8-63 are upper border. 8-62 on 128k
            //switch(this->variant) // Assume this was a typo!
            this->scanline_func = &scanline_border_top;
            break;
        case 63:
            if (this->variant == ZXS_spectrum128)
                this->scanline_func = &scanline_visible;
            break;
        case 64: // lines 64-255 are visible
            this->scanline_func = &scanline_visible;
            break;
        case 255:
            if (this->variant == ZXS_spectrum128)
                this->scanline_func = &scanline_border_bottom;
            break;
        case 256: // 256-311 or 310 are lower border
            this->scanline_func = &scanline_border_bottom;
            break;
    }
    
}

void ZXSpectrum_ULA_cycle(ZXSpectrum* bus)
{
    BTHIS;
    this->scanline_func(bus);
    bus->clock.ula_x++;
    this->screen_x++;
    this->display->scan_x++;
    bus->clock.ula_frame_cycle++;
    if (bus->clock.ula_x == bus->clock.screen_right) new_scanline(bus);
}
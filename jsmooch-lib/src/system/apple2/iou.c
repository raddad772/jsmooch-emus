//
// Created by . on 8/30/24.
//

#include <cstring>
#include <cstdlib>
#include <cassert>
#include <cstdio>

#include "iou.h"
#include "apple2.h"
#include "apple2_internal.h"
#include "iou.h"

void apple2_IOU_reset(apple2* this)
{
    if (!this->iou.display)
        this->iou.display = (JSM_DISPLAY*)cpg(this->iou.display_ptr);

    this->clock.crt_x = this->clock.crt_y = 0;
    this->clock.frames_since_restart = 0;
    this->iou.flash_counter = 0;
    this->iou.flash = 0;
    memset(&this->iou.io, 0, sizeof(APPLE2IOUIO));
    this->iou.io.VBL = 0;
    this->iou.io.HIRES = this->iou.io.PAGE2 = 0;

    // TODO: paddle timers
}

static void advance_frame(apple2* this)
{
    this->clock.crt_y = 0;
    this->clock.frames_since_restart++;
    this->clock.master_frame++;

    this->iou.cur_output = this->iou.display->output[this->iou.display->last_written];
    this->iou.display->last_written ^= 1;

    this->iou.flash_counter = (this->iou.flash_counter + 1) % 32;
    if (this->iou.flash_counter == 0) {
        this->iou.flash ^= 1;
    }
    // TODO: flash counter, keyboard auto-repeat,
}

static void advance_line(apple2* this)
{
    this->clock.crt_x = 0;
    this->clock.crt_y++;
    switch(this->clock.crt_y) {
        case 262:
            advance_frame(this);
            break;
    }
}

enum {
    gm_text,
    gm_lores,
    gm_hires,
    gm_mixed
};

static void pixels_lores(apple2* this)
{

}

static void IOU_pixels(apple2* this)
{
    // output 1-14 pixels depending on video mode
    // 1 pixel in LORES, 7 in HIRES or 40COL, and 14 in DOUBLE HIRES or 80COL
    // HIRES = 280x192, 1 bit per pixel, 7 1-pixel renders
    // LORES = 40x192, 4 bits per pixel, each is pixel and pixel below it, 7 1-pixel repeats
    // TEXT = 280x192, 1 bit per pixel, text rendering (7 pix each from rom)
    //

    if (this->iou.io.MIXED) { // Moxed mode
        printf("\nWARNING MIXED MODE");
    }
    else if (this->iou.io.HIRES) {
        printf("\nWARNING HIRES MODE");
    }
    else if (this->iou.io.TEXT) {
        printf("\nWARNING TEXT MODE");
    }
    else if (!this->iou.io.HIRES) {
        pixels_lores(this);
    }
    else {
        printf("\nWARNING UNKNOWN MODE");
    }
}

void apple2_IOU_cycle(apple2* this)
{
    // for visible lines, 40 CPU cycles = output, 25 = blank (with the last elongated)

    // 455? virtual NTSC pixels,
    // 280 of which are visible, the rest blanked?


    // 262 NTSC lines
    // 192 lines are active display
    // 70 are blank
    if (this->clock.long_cycle_counter == 0) {
        advance_line(this);
    }


    if (this->clock.crt_y < 192) { // 192 visible lines
        if (this->clock.long_cycle_counter < 40) {// up to 40*7 pixels wide
            IOU_pixels(this);

            if ((this->clock.crt_y == 191) && (this->clock.long_cycle_counter == 39)) {
                this->iou.io.VBL = 1;
            }
        }
    }
    if ((this->clock.crt_y == 261) && (this->clock.long_cycle_counter == 39)) {
        this->iou.io.VBL = 0;
    }
}

static void keystrobe(apple2* this)
{
    printf("\nKEYSTROBE!");
}


#define NUMKEYS 51
static u32 apple2_keyboard_keymap[51] = {
        JK_1, JK_2, JK_3, JK_4, JK_5,
        JK_0, JK_9, JK_8, JK_7, JK_6,
        JK_Q, JK_W, JK_E, JK_R, JK_T,
        JK_P, JK_O, JK_I, JK_U, JK_Y,
        JK_A, JK_S, JK_D, JK_F, JK_G,
        JK_L, JK_K, JK_J, JK_H,
        JK_Z, JK_X, JK_C, JK_V,
        JK_M, JK_N, JK_B,
        JK_SPACE, JK_ESC, JK_CTRL,
        JK_SHIFT, JK_RSHIFT, JK_LEFT, JK_RIGHT,
        JK_ENTER, JK_BACKSPACE, JK_SEMICOLON, JK_MINUS,
        JK_EQUALS, JK_COMMA, JK_SLASH_FW, JK_DOT
};

void apple2_setup_keyboard(apple2* this)
{
    struct physical_io_device *d = cvec_push_back(this->IOs);
    physical_io_device_init(d, HID_KEYBOARD, 0, 0, 1, 1);

    d->id = 0;
    d->kind = HID_KEYBOARD;
    d->connected = 1;
    d->enabled = 1;

    struct JSM_KEYBOARD* kbd = &d->keyboard;
    memset(kbd, 0, sizeof(JSM_KEYBOARD));
    kbd->num_keys = NUMKEYS;

    for (u32 i = 0; i < NUMKEYS; i++) {
        kbd->key_defs[i] = apple2_keyboard_keymap[i];
    }
}


static u32 is_matrix_key(enum JKEYS jkey)
{
#define CK(x) (jkey == x)
    if (CK(JK_SHIFT) || CK(JK_RSHIFT) || CK(JK_CMD) || CK(JK_OPTION) || CK(JK_CAPS)) return 0;
    return 1;
#undef CK
}

static u32 AKD(apple2* this)
{
    struct physical_io_device *pio = cpg(this->iou.keyboard_ptr);
    struct JSM_KEYBOARD *kbd = &pio->keyboard;
    for (u32 i = 0; i < kbd->num_keys; i++) {
        if (is_matrix_key(kbd->key_defs[i])) {
            if (kbd->key_states[i])
                return 1;
        }
    }
    return 0;
}

u8 apple2_read_keyboard(apple2* this)
{
    // TODO: this
    return 0;
}


void apple2_IOU_access_c0xx(apple2* this, u32 addr, u32 has_effect, u32 is_write, u32 *r, u32 *MSB) {
    u32 addr3 = addr & 0xFFF0;
    u32 did_AKD = 5;
    if ((is_write && (addr3 == 0xC010)) || (addr == 0xC010)) {
        keystrobe(this);
        *MSB = AKD(this);
        did_AKD = *MSB;
    }
    if ((!is_write) && (addr3 == 0xC010)) {
        *MSB = this->iou.io.KEYSTROBE << 7;
        if (did_AKD != 5)
            *MSB = AKD(this);
    }

    if (addr3 == 0xC020)
        this->iou.io.CSSTOUT ^= 1;
    if (addr3 == 0xC030)
        this->iou.io.SPKR ^= 1;
    if (addr < 0xC020)
        *r = apple2_read_keyboard(this);

    if (addr3 == 0xC060) {
        // Multiplexed bit...
        u32 addr4 = addr & 7;
        switch(addr4) {
            case 0:
                *MSB = this->iou.io.cassette_in << 7;
                break;
            case 1:
            case 2:
            case 3:
                *MSB = this->iou.io.pushbutton[addr4 - 1] << 7;
                //PB0 = open apple (option)
                //PB1 = closed apple (command)
                break;
            case 4:
            case 5:
            case 6:
            case 7:
                *MSB = this->iou.io.paddle[addr4 - 4] << 7;
                break;
        }
    }
}
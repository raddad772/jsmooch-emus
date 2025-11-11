//
// Created by . on 5/9/25.
//

#include <string.h>
#include "snes_joypad.h"
#include "helpers/physical_io.h"


void SNES_joypad_latch(SNES_joypad *this, u32 val)
{
    if (this->latched == val) return;
    this->latched = val;
    this->counter = 0;
    struct cvec* bl = &this->pio->controller.digital_buttons;
    struct HID_digital_button *b;
    u32 bn = 0;
#define B_GET(button) { b = cvec_get(bl, bn++); this->input_buffer. button = b->state; }
    if (this->latched == 0) {
        B_GET(up);
        B_GET(down);
        B_GET(left);
        B_GET(right);
        B_GET(a);
        B_GET(b);
        B_GET(x);
        B_GET(y);
        B_GET(l);
        B_GET(r);
        B_GET(start);
        B_GET(select);
    }
#undef B_GET
}

u32 SNES_joypad_data(SNES_joypad *this)
{
    if (this->latched == 1) return this->input_buffer.b;
    switch(this->counter++) {
        case 0: return this->input_buffer.b;
        case 1: return this->input_buffer.y;
        case 2: return this->input_buffer.select;
        case 3: return this->input_buffer.start;
        case 4: return this->input_buffer.up;
        case 5: return this->input_buffer.down;
        case 6: return this->input_buffer.left;
        case 7: return this->input_buffer.right;
        case 8: return this->input_buffer.a;
        case 9: return this->input_buffer.x;
        case 10: return this->input_buffer.l;
        case 11: return this->input_buffer.r;
        case 12:
        case 13:
        case 14:
        case 15:
            return 0;
    }
    this->counter = 16;
    return 1;
}

void SNES_joypad_setup_pio(physical_io_device *d, u32 num, const char*name, u32 connected)
{
    physical_io_device_init(d, HID_CONTROLLER, 0, 0, 1, 1);

    snprintf(d->controller.name, sizeof(d->controller.name), "%s", name);
    d->id = num;
    d->kind = HID_CONTROLLER;
    d->connected = connected;
    d->enabled = connected;

    struct JSM_CONTROLLER* cnt = &d->controller;

    // up down left right a b start select. in that order
    pio_new_button(cnt, "up", DBCID_co_up);
    pio_new_button(cnt, "down", DBCID_co_down);
    pio_new_button(cnt, "left", DBCID_co_left);
    pio_new_button(cnt, "right", DBCID_co_right);
    pio_new_button(cnt, "a", DBCID_co_fire1);
    pio_new_button(cnt, "b", DBCID_co_fire2);
    pio_new_button(cnt, "x", DBCID_co_fire3);
    pio_new_button(cnt, "y", DBCID_co_fire4);
    pio_new_button(cnt, "l", DBCID_co_shoulder_left);
    pio_new_button(cnt, "r", DBCID_co_shoulder_right);
    pio_new_button(cnt, "start", DBCID_co_start);
    pio_new_button(cnt, "select", DBCID_co_select);
}

void SNES_joypad_init(SNES_joypad* this)
{
    memset(this, 0, sizeof(*this));
}

void SNES_joypad_delete(SNES_joypad* this)
{
    this->pio = NULL;
}

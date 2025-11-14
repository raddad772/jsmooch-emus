//
// Created by . on 5/9/25.
//

#include <cstring>
#include "snes_joypad.h"
#include "helpers/physical_io.h"


void SNES_joypad::latch(u32 val)
{
    if (latched == val) return;
    latched = val;
    counter = 0;
    physical_io_device &pio = pio_ptr.get();
    //pio.controller.digital_buttons;
    HID_digital_button *b;
    u32 bn = 0;
#define B_GET(button) { b = &pio.controller.digital_buttons.at(bn++); input_buffer. button = b->state; }
    if (latched == 0) {
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

u32 SNES_joypad::data()
{
    if (latched == 1) return input_buffer.b;
    switch(counter++) {
        case 0: return input_buffer.b;
        case 1: return input_buffer.y;
        case 2: return input_buffer.select;
        case 3: return input_buffer.start;
        case 4: return input_buffer.up;
        case 5: return input_buffer.down;
        case 6: return input_buffer.left;
        case 7: return input_buffer.right;
        case 8: return input_buffer.a;
        case 9: return input_buffer.x;
        case 10: return input_buffer.l;
        case 11: return input_buffer.r;
        case 12:
        case 13:
        case 14:
        case 15:
            return 0;
    }
    counter = 16;
    return 1;
}

void SNES_joypad::setup_pio(physical_io_device &d, u32 num, const char*name, u32 connected)
{
    d.init(HID_CONTROLLER, 0, 0, 1, 1);

    snprintf(d.controller.name, sizeof(d.controller.name), "%s", name);
    d.id = num;
    d.kind = HID_CONTROLLER;
    d.connected = connected;
    d.enabled = connected;

    JSM_CONTROLLER* cnt = &d.controller;

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

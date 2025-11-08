//
// Created by Dave on 2/5/2024.
//

#include "nes_joypad.h"

NES_joypad::NES_joypad(u32 joynum) : joynum(joynum)
{
}

void NES_joypad::latch(u32 what)
{
    if (latched == what) return;
    latched = what;
    counter = 0;
    if (latched == 0) {
        physical_io_device& p = devices[device_index];
        if (p.connected) {
            struct std::vector<HID_digital_button> &bl = p.controller.digital_buttons;
            struct HID_digital_button* b;
            // up down left right a b start select
#define B_GET(button, num) { b = &bl.at(num); input_buffer. button = b->state; }
            B_GET(up, 0);
            B_GET(down, 1);
            B_GET(left, 2);
            B_GET(right, 3);
            B_GET(a, 4);
            B_GET(b, 5);
            B_GET(start, 6);
            B_GET(select, 7);
#undef B_GET
        }
    }
}

u32 NES_joypad::data()
{
    if (latched == 1) {
        return input_buffer.a;
    }

    switch(counter++) {
        case 0: return input_buffer.a;
        case 1: return input_buffer.b;
        case 2: return input_buffer.select;
        case 3: return input_buffer.start;
        case 4: return input_buffer.up;
        case 5: return input_buffer.down;
        case 6: return input_buffer.left;
        case 7: return input_buffer.right;
    }

    counter = 8;
    return 1;
}

void NES_joypad::setup_pio(struct physical_io_device *d, u32 num, const char*name, u32 connected)
{
    // = cvec_push_back(IOs);
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
    pio_new_button(cnt, "start", DBCID_co_start);
    pio_new_button(cnt, "select", DBCID_co_select);
}

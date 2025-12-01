//
// Created by . on 11/18/24.
//

#include <cstring>
#include <cstdio>

#include "genesis6.h"

namespace genesis {

void c6button::setup_pio(physical_io_device *d, u32 num, const char*name, u32 connected)
{
    d->init(HID_CONTROLLER, 0, 0, 1, 1);

    snprintf(d->controller.name, sizeof(d->controller.name), "%s", name);
    d->id = num;
    d->kind = HID_CONTROLLER;
    d->connected = connected;
    d->enabled = connected;

    JSM_CONTROLLER* cnt = &d->controller;

    // up down left right a b start select. in that order
    pio_new_button(cnt, "up", DBCID_co_up);
    pio_new_button(cnt, "down", DBCID_co_down);
    pio_new_button(cnt, "left", DBCID_co_left);
    pio_new_button(cnt, "right", DBCID_co_right);
    pio_new_button(cnt, "a", DBCID_co_fire1);
    pio_new_button(cnt, "b", DBCID_co_fire2);
    pio_new_button(cnt, "c", DBCID_co_fire3);
    pio_new_button(cnt, "x", DBCID_co_fire4);
    pio_new_button(cnt, "y", DBCID_co_fire5);
    pio_new_button(cnt, "z", DBCID_co_fire6);
    pio_new_button(cnt, "start", DBCID_co_start);
    pio_new_button(cnt, "mode", DBCID_co_select);
    pio = d;
}

void c6button::latch()
{
    auto& bl = pio->controller.digital_buttons;
    HID_digital_button *b;
#define B_GET(button, num) { b = &bl.at(num); input_buffer. button = b->state; }
    B_GET(up, 0);
    B_GET(down, 1);
    B_GET(left, 2);
    B_GET(right, 3);
    B_GET(a, 4);
    B_GET(b, 5);
    B_GET(c, 6);
    B_GET(x, 7);
    B_GET(y, 8);
    B_GET(z, 9);
    B_GET(start, 10);
    B_GET(mode, 11);
#undef B_GET
}

u8 c6button::read_data() {
    if ((*master_clock) >= timeout_at) counter = 0;
    u8 out = 0;
#define B_GET(name, shift) out |= input_buffer. name << shift

    if (select == 0) {
        switch(counter) {
            case 0:
            case 1:
            case 4:
                B_GET(up, 0);
                B_GET(down, 1);
                out |= 0b1100;
                break;
            case 2:
                out |= 0b1111;
                break;
            case 3:
                break;
        }
        B_GET(a, 4);
        B_GET(start, 5);
    } else {
        switch(counter) {
            case 0:
            case 1:
            case 2:
            case 4:
                B_GET(up, 0);
                B_GET(down, 1);
                B_GET(left, 2);
                B_GET(right, 3);
                break;
            case 3:
                B_GET(z, 0);
                B_GET(y, 1);
                B_GET(x, 2);
                B_GET(mode, 3);
        }
        B_GET(b, 4);
        B_GET(c, 5);
    }

    return out ^ 0x3F;
}

#define MCLOCK_1_6ms 86019

void c6button::write_data(u8 val)
{
    if ((*master_clock) >= timeout_at) counter = 0;

    if(!select && ((val >> 6) & 1)) {
        if(counter < 4) counter = (counter + 1) & 7;
        timeout_at = (*master_clock) + MCLOCK_1_6ms;
    }

    select = (val >> 6) & 1;
}
}
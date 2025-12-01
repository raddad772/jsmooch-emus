//
// Created by . on 6/1/24.
//

#include "genesis3.h"

namespace genesis {

void c3button::latch()
{
    auto &bl = pio->controller.digital_buttons;
    HID_digital_button *b;
#define B_GET(button, num) { b = &bl.at(num); input_buffer. button = b->state; }
    B_GET(up, 0);
    B_GET(down, 1);
    B_GET(left, 2);
    B_GET(right, 3);
    B_GET(a, 4);
    B_GET(b, 5);
    B_GET(c, 6);
    B_GET(start, 7);
#undef B_GET
}

void c3button::setup_pio(physical_io_device *d, u32 num, const char*name, u32 connected)
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
    pio_new_button(cnt, "start", DBCID_co_start);
    pio = d;
}

u8 c3button::read_data() const
{
    u8 out = 0;
    if (pio == nullptr) return 0;
#define B_GET(name, shift) out |= input_buffer. name << shift
    if (!select) {
        B_GET(up, 0); // up
        B_GET(down, 1); // down
        out |= 0b1100;
        B_GET(a, 4);  // a
        B_GET(start, 5);  // start
        out |= (1 << 4); // A is pressed
    }
    else {
        /* up, down, left, right, b, c */
        B_GET(up, 0); // up
        B_GET(down, 1); // down
        B_GET(left, 2); // left
        B_GET(right, 3); // right
        B_GET(b, 4); // b
        B_GET(c, 5); // c
    }

    return out ^ 0x3F;
}

void c3button::write_data(u8 val)
{
    select = (val >> 6) & 1;
}
}
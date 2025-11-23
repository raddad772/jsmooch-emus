//
// Created by Dave on 2/7/2024.
//

#include "helpers/physical_io.h"
#include "sms_gamepad.h"

u8 SMSGG_gamepad::read()
{
    latch();
    return (pins.up) | (pins.down << 1) | (pins.left << 2) | (pins.right << 3) | (pins.tl << 4) | (pins.tr << 5) | 0x40;
}

void SMSGG_gamepad::setup_pio(physical_io_device &d, u32 num, const char*name, u32 connected, u32 pause_button)
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
    pio_new_button(cnt, "1", DBCID_co_fire1);
    pio_new_button(cnt, "2", DBCID_co_fire2);
    if (pause_button)
        pio_new_button(cnt, "pause", DBCID_co_start);
}


void SMSGG_gamepad::latch()
{
    physical_io_device& p = device_ptr.get();
    if (p.connected) {
        HID_digital_button* b;
#define B_GET(button, num) { b = &p.controller.digital_buttons.at(num); pins. button = b->state ^ 1; }
        B_GET(up, 0);
        B_GET(down, 1);
        B_GET(left, 2);
        B_GET(right, 3);
        B_GET(tr, 4);
        B_GET(tl, 5);
        if (variant == jsm::systems::GG) {
            B_GET(start, 6);
        }
#undef B_GET
    }
    else {
        pins.up = 1;
        pins.down = 1;
        pins.left = 1;
        pins.right = 1;
        pins.tr = 1;
        pins.tl = 1;
    }
}

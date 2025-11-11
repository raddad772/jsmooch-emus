//
// Created by Dave on 2/7/2024.
//

#include "stdio.h"
#include "helpers/physical_io.h"
#include "sms_gamepad.h"
#include "string.h"

void SMSGG_gamepad_init(SMSGG_gamepad* this, enum jsm::systems variant, u32 num)
{
    *this = (SMSGG_gamepad) {
            .variant = variant,
            .num = num,
            .pins = (SMSGG_gamepad_pins) { 1, 1, 1, 1, 1, 1, 1}
    };
}

u32 SMSGG_gamepad_read(SMSGG_gamepad* this)
{
    SMSGG_gamepad_latch(this);
    return (this->pins.up) | (this->pins.down << 1) | (this->pins.left << 2) | (this->pins.right << 3) | (this->pins.tl << 4) | (this->pins.tr << 5) | 0x40;
}

void SMSGG_gamepad_setup_pio(physical_io_device *d, u32 num, const char*name, u32 connected, u32 pause_button)
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
    pio_new_button(cnt, "1", DBCID_co_fire1);
    pio_new_button(cnt, "2", DBCID_co_fire2);
    if (pause_button)
        pio_new_button(cnt, "pause", DBCID_co_start);
}


void SMSGG_gamepad_latch(SMSGG_gamepad* this)
{
    struct physical_io_device* p = cpg(this->device_ptr);
    if (p->connected) {
        struct cvec* bl = &p->controller.digital_buttons;
        struct HID_digital_button* b;
#define B_GET(button, num) { b = cvec_get(bl, num); this->pins. button = b->state ^ 1; }
        B_GET(up, 0);
        B_GET(down, 1);
        B_GET(left, 2);
        B_GET(right, 3);
        B_GET(tr, 4);
        B_GET(tl, 5);
        if (this->variant == jsm::systems::GG) {
            B_GET(start, 6);
        }
#undef B_GET
    }
    else {
        this->pins.up = 1;
        this->pins.down = 1;
        this->pins.left = 1;
        this->pins.right = 1;
        this->pins.tr = 1;
        this->pins.tl = 1;
    }
}

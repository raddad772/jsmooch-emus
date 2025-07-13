//
// Created by . on 7/13/25.
//

#include "tg16b2.h"

void TG16_2button_init(struct TG16_2button *this)
{
    this->pio = NULL;

}

void TG16_2button_delete(struct TG16_2button *this)
{
    this->pio = NULL;
}

void TG16_2button_setup_pio(struct physical_io_device *d, u32 num, const char*name, u32 connected)
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
    pio_new_button(cnt, "right", DBCID_co_right);
    pio_new_button(cnt, "down", DBCID_co_down);
    pio_new_button(cnt, "left", DBCID_co_left);
    pio_new_button(cnt, "i", DBCID_co_fire1);
    pio_new_button(cnt, "ii", DBCID_co_fire2);
    pio_new_button(cnt, "select", DBCID_co_fire3);
    pio_new_button(cnt, "run", DBCID_co_start);
}

u8 TG16_2button_read_data(struct TG16_2button *this)
{
    struct cvec* bl = &this->pio->controller.digital_buttons;
    struct HID_digital_button *b;

    if (this->clr) return 0;
    u32 data = 0;
#define B_GET(num, snum) { b = cvec_get(bl, num); data |= b->state << snum; }
    if (this->sel) {
        B_GET(0, 0); // up
        B_GET(1, 1); // right
        B_GET(2, 2); // down
        B_GET(3, 3); // left
    }
    else {
        B_GET(4, 0); // I
        B_GET(5, 1); // II
        B_GET(6, 2); // select
        B_GET(7, 3); // run
    }
#undef B_GET
    data ^= 0x0F;
    return data;
}

void TG16_2button_write_data(struct TG16_2button *this, u8 val)
{
    this->sel = val & 1;
    this->clr = (val >> 1) & 1;
}

//
// Created by . on 12/4/24.
//

#include "nds_bus.h"
#include "nds_controller.h"

u32 NDS_get_controller_state(struct NDS *this, u32 byte)
{

    struct JSM_CONTROLLER *cnt = &this->controller.pio->controller;
    struct JSM_TOUCHSCREEN *tsc = &this->spi.touchscr.pio->touchscreen;
    struct cvec* bl = &cnt->digital_buttons;
    struct HID_digital_button *b;
    u32 v = 0;
#define B_GET(bit, num) { b = cvec_get(bl, num); v |= (b->state << bit); }
    B_GET(0, 4); // A
    B_GET(1, 5); // B
    B_GET(2, 9); // select
    B_GET(3, 8); // start
    B_GET(4, 3); // right
    B_GET(5, 2); // left
    B_GET(6, 0); // up
    B_GET(7, 1); // down
    B_GET(8, 7); // R button
    B_GET(9, 6); // L button
    B_GET(16, 10); // X button
    B_GET(17, 11); // Y button
#undef B_GET
    b = cvec_get(bl, 10);
    // pen down bit 22
    v |= (tsc->touch.down << 22);
    v ^= 0x007FFFFF;
    v &= 0x007F03FF;

    if (byte == 0) return v & 0xFF;
    if (byte == 1) return (v >> 8) & 0xFF;
    if (byte == 2) {
        return (v >> 16) & 0x7F;
    }
    printf("\nWHAT!?!?!?!?");
    return v;
}


void NDS_controller_setup_pio(struct physical_io_device *d)
{
    physical_io_device_init(d, HID_CONTROLLER, 0, 0, 1, 1);

    snprintf(d->controller.name, sizeof(d->controller.name), "%s", "NDS Input");
    d->id = 0;
    d->kind = HID_CONTROLLER;
    d->connected = 1;
    d->enabled = 1;

    struct JSM_CONTROLLER* cnt = &d->controller;

    cvec_alloc_atleast(&cnt->digital_buttons, 12);
    // up down left right a b start select. in that order
    pio_new_button(cnt, "up", DBCID_co_up);
    pio_new_button(cnt, "down", DBCID_co_down);
    pio_new_button(cnt, "left", DBCID_co_left);
    pio_new_button(cnt, "right", DBCID_co_right);
    pio_new_button(cnt, "a", DBCID_co_fire2);
    pio_new_button(cnt, "b", DBCID_co_fire1);
    pio_new_button(cnt, "l", DBCID_co_shoulder_left);
    pio_new_button(cnt, "r", DBCID_co_shoulder_right);
    pio_new_button(cnt, "start", DBCID_co_start);
    pio_new_button(cnt, "select", DBCID_co_select);
    pio_new_button(cnt, "x", DBCID_co_fire4);
    pio_new_button(cnt, "y", DBCID_co_fire3);
}

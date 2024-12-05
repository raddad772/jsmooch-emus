//
// Created by . on 12/4/24.
//

#include "gba_controller.h"

void GBA_controller_setup_pio(struct physical_io_device *d)
{
    physical_io_device_init(d, HID_CONTROLLER, 0, 0, 1, 1);

    snprintf(d->controller.name, sizeof(d->controller.name), "%s", "GBA Input");
    d->id = 0;
    d->kind = HID_CONTROLLER;
    d->connected = 1;
    d->enabled = 1;

    struct JSM_CONTROLLER* cnt = &d->controller;

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
}

//
// Created by . on 6/1/24.
//

#include <stdlib.h>
#include <stdio.h>

#include "genesis3.h"

void genesis_controller_3button_init(struct genesis_controller_3button* this)
{
    this->pio = NULL;
}

void genesis_controller_3button_delete(struct genesis_controller_3button* this)
{

}


void genesis3_setup_pio(struct physical_io_device *d, u32 num, const char*name, u32 connected)
{
    physical_io_device_init(d, HID_CONTROLLER, 0, 0, 1, 1);

    sprintf(d->device.controller.name, "%s", name);
    d->id = num;
    d->kind = HID_CONTROLLER;
    d->connected = connected;

    struct JSM_CONTROLLER* cnt = &d->device.controller;

    // up down left right a b start select. in that order
    pio_new_button(cnt, "up", DBCID_co_up);
    pio_new_button(cnt, "down", DBCID_co_down);
    pio_new_button(cnt, "left", DBCID_co_left);
    pio_new_button(cnt, "right", DBCID_co_right);
    pio_new_button(cnt, "a", DBCID_co_fire1);
    pio_new_button(cnt, "b", DBCID_co_fire2);
    pio_new_button(cnt, "c", DBCID_co_fire3);
    pio_new_button(cnt, "start", DBCID_co_start);
}

//
// Created by Dave on 2/5/2024.
//

#include "nes_joypad.h"
#include "stdio.h"

void nespad_inputs_init(struct nespad_inputs* this)
{
    this->a = this->b = this->start = this->select = 0;
    this->up = this->down = this->left = this->right = 0;
}

void NES_joypad_init(struct NES_joypad* this, u32 joynum)
{
    this->counter = this->latched = 0;
    this->joynum = joynum;
    nespad_inputs_init(&this->input_buffer);
}

void NES_joypad_latch(struct NES_joypad *this, u32 what)
{
    if (this->latched == what) return;
    this->latched = what;
    this->counter = 0;
    if (this->latched == 0) {
        struct physical_io_device* p = (struct physical_io_device*)cvec_get(this->devices, this->device_index);
        if (p->connected) {
            struct cvec* bl = &p->device.controller.digital_buttons;
            struct HID_digital_button* b;
            // up down left right a b start select
#define B_GET(button, num) { b = cvec_get(bl, num); this->input_buffer. button = b->state; }
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

u32 NES_joypad_data(struct NES_joypad* this)
{
    if (this->latched == 1) {
        return this->input_buffer.a;
    }

    switch(this->counter++) {
        case 0: return this->input_buffer.a;
        case 1: return this->input_buffer.b;
        case 2: return this->input_buffer.select;
        case 3: return this->input_buffer.start;
        case 4: return this->input_buffer.up;
        case 5: return this->input_buffer.down;
        case 6: return this->input_buffer.left;
        case 7: return this->input_buffer.right;
    }

    this->counter = 8;
    return 1;
}

void NES_joypad_setup_pio(struct physical_io_device *d, u32 num, const char*name, u32 connected)
{
    // = cvec_push_back(this->IOs);
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
    pio_new_button(cnt, "start", DBCID_co_start);
    pio_new_button(cnt, "select", DBCID_co_select);
}

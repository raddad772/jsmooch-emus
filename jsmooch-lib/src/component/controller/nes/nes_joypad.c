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


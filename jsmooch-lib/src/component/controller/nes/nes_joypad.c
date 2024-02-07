//
// Created by Dave on 2/5/2024.
//

#include "nes_joypad.h"

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
    nespad_inputs_init(&this->input_waiting);
}

void NES_joypad_buffer_input(struct NES_joypad* this, struct nespad_inputs *from)
{
    this->input_waiting.up = from->up;
    this->input_waiting.down = from->down;
    this->input_waiting.left = from->left;
    this->input_waiting.right = from->right;
    this->input_waiting.a = from->a;
    this->input_waiting.b = from->b;
    this->input_waiting.start = from->start;
    this->input_waiting.select = from->select;
}

void NES_joypad_latch(struct NES_joypad *this, u32 what)
{
    if (this->latched == what) return;
    this->latched = what;
    this->counter = 0;
    if (this->latched == 0) {
        this->input_buffer.up = this->input_waiting.up;
        this->input_buffer.down = this->input_waiting.down;
        this->input_buffer.left = this->input_waiting.left;
        this->input_buffer.right = this->input_waiting.right;
        this->input_buffer.a = this->input_waiting.a;
        this->input_buffer.b = this->input_waiting.b;
        this->input_buffer.start = this->input_waiting.start;
        this->input_buffer.select = this->input_waiting.select;
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


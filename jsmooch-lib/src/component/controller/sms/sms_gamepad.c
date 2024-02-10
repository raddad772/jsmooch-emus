//
// Created by Dave on 2/7/2024.
//

#include "sms_gamepad.h"

void smspad_inputs_init(struct smspad_inputs* this)
{
    *this = (struct smspad_inputs) {
            .b1 = 0,
            .b2 = 0,
            .up = 0,
            .down = 0,
            .left = 0,
            .right = 0,
            .start = 0
    };
}

void SMSGG_gamepad_init(struct SMSGG_gamepad* this, enum jsm_systems variant, u32 num)
{
    *this = (struct SMSGG_gamepad) {
            .variant = variant,
            .num = num,
            .pins = (struct SMSGG_gamepad_pins) { 1, 1, 1, 1, 1, 1, 1}
    };
    smspad_inputs_init(&this->input_waiting);
}

void SMSGG_gamepad_buffer_input(struct SMSGG_gamepad* this, struct smspad_inputs* from)
{
    this->input_waiting = *from;
    if ((this->variant == SYS_SMS1) || (this->variant == SYS_SMS2) || (this->num != 1)) this->input_waiting.start = 0;
}

u32 SMSGG_gamepad_read(struct SMSGG_gamepad* this)
{
    SMSGG_gamepad_latch(this);
    return (this->pins.up) | (this->pins.down << 1) | (this->pins.left << 2) | (this->pins.right << 3) | (this->pins.tl << 4) | (this->pins.tr << 5) | 0x40;
}

void SMSGG_gamepad_latch(struct SMSGG_gamepad* this)
{
    this->pins.up = this->input_waiting.up ? 0 : 1;
    this->pins.down = this->input_waiting.down ? 0 : 1;
    this->pins.left = this->input_waiting.left ? 0 : 1;
    this->pins.right = this->input_waiting.right ? 0 : 1;
    this->pins.tr = this->input_waiting.b2 ? 0 : 1;
    this->pins.tl = this->input_waiting.b1 ? 0 : 1;
}

//
// Created by Dave on 2/7/2024.
//

#ifndef JSMOOCH_EMUS_SMS_GAMEPAD_H
#define JSMOOCH_EMUS_SMS_GAMEPAD_H

#include "helpers/int.h"
#include "helpers/sys_interface.h"

struct smspad_inputs {
    u32 b1, b2, start, up, down, left, right;
};

struct SMSGG_gamepad {
    struct SMSGG_gamepad_pins { u32 tr, th, tl, up, down, left, right; } pins;
    u32 num;
    struct smspad_inputs input_waiting;
    enum jsm_systems variant;

    struct cvec* devices;
    u32 device_index;
};

void smspad_inputs_init(struct smspad_inputs* this);
void SMSGG_gamepad_init(struct SMSGG_gamepad* this, enum jsm_systems variant, u32 num);
void SMSGG_gamepad_buffer_input(struct SMSGG_gamepad* this, struct smspad_inputs* src);
u32 SMSGG_gamepad_read(struct SMSGG_gamepad* this);
void SMSGG_gamepad_latch(struct SMSGG_gamepad* this);


#endif //JSMOOCH_EMUS_SMS_GAMEPAD_H

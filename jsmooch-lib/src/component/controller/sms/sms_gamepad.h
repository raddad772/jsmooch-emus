//
// Created by Dave on 2/7/2024.
//

#ifndef JSMOOCH_EMUS_SMS_GAMEPAD_H
#define JSMOOCH_EMUS_SMS_GAMEPAD_H

#include "helpers/int.h"
#include "helpers/sys_interface.h"

struct SMSGG_gamepad {
    struct SMSGG_gamepad_pins { u32 tr, th, tl, up, down, left, right, start; } pins;
    u32 num;
    enum jsm_systems variant;

    struct cvec_ptr device_ptr;
};

void SMSGG_gamepad_init(struct SMSGG_gamepad*, enum jsm_systems variant, u32 num);
u32 SMSGG_gamepad_read(struct SMSGG_gamepad*);
void SMSGG_gamepad_latch(struct SMSGG_gamepad*);
void SMSGG_gamepad_setup_pio(struct physical_io_device *d, u32 num, const char*name, u32 connected, u32 pause_button);

#endif //JSMOOCH_EMUS_SMS_GAMEPAD_H

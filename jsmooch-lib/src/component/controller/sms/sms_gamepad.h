//
// Created by Dave on 2/7/2024.
//

#pragma once

#include "helpers/int.h"
#include "helpers/sys_interface.h"
#include "helpers/cvec.h"

struct SMSGG_gamepad {
    struct SMSGG_gamepad_pins { u8 tr{1}, th{1}, tl{1}, up{1}, down{1}, left{1}, right{1}, start{1}; } pins{};
    u32 num{};
    jsm::systems variant{};

    cvec_ptr<physical_io_device> device_ptr{};

    u8 read();
    void latch();
    static void setup_pio(physical_io_device &d, u32 num, const char *name, u32 connected, u32 pause_button);
};


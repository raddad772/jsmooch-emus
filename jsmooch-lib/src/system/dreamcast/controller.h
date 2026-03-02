//
// Created by RadDad772 on 3/13/24.
//

#pragma once
#include "helpers/int.h"

namespace DC {

struct DC_inputs {
    u32 a{}, b{}, x{}, y{}, up{}, down{}, left{}, right{}, start{};
    u32 analog_x{}, analog_y{}, analog_l{}, analog_r{};
};

enum DCC {
    DCC_rx,
    DCC_tx
};
struct core;
struct controller {
    void write(u32 data);
    u32 read(u32* more);
    void setup_pio(physical_io_device *d, u32 num, const char*name, bool connected);

    DC_inputs input_waiting{};
    DCC state{DCC_rx};
    u32 cmd_index{};
    u32 cmd[16]{};
    u32 reply_cmd{};
    u32 de_cmd{};
    u32 reply_len{};
    u32 reply_buf[32]{};

    physical_io_device *pio{};

    u32 src_AP{};
    u32 dest_AP{};
    u32 data_size{};
};

}
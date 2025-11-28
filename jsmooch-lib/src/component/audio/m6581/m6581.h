#pragma once

#include "helpers/int.h"
namespace M6581 {

struct channel {
    bool ext_enable;
};
struct core {
    void write_IO(u16 addr, u8 val);;
    u8 read_IO(u16 addr, u8 old, bool has_effect);
    bool ext_enable;
    channel channels[3];


    float sample_channel_debug(int num) { return 0.0f; }
    void reset();
    void cycle();

    struct {
        float level;
    } output{};
};
}
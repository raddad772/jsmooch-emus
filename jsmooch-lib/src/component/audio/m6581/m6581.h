#pragma once

#include "helpers/int.h"

struct M6581 {
    void write_IO(u16 addr, u8 val);;
    u8 read_IO(u16 addr, u8 old, bool has_effect);
};
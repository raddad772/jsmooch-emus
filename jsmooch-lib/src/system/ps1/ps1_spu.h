//
// Created by . on 2/20/25.
//

#pragma once
#include "helpers/int.h"

namespace PS1 {
struct SPU {
    void write(u32 addr, u8 sz, u32 val);
    u32 read(u32 addr, u8 sz, bool has_effect);
    u16 RAM[0x400]{};
};
}
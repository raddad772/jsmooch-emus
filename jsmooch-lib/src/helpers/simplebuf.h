//
// Created by . on 8/29/24.
//

#pragma once

#include "int.h"
struct buf;
struct simplebuf8 {
    simplebuf8() = default;
    ~simplebuf8();
    void allocate(u64 sz);
    void clear();
    void copy_from_buf(buf &src);
    u8 *ptr{};
    u64 sz{};
    u32 mask{};
};

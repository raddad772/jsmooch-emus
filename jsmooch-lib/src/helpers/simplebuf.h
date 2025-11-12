//
// Created by . on 8/29/24.
//

#pragma once
#include <cstddef>

#include "int.h"
struct buf;
struct simplebuf8 {
    explicit simplebuf8(size_t n) { allocate(n); };
    simplebuf8() = default;
    ~simplebuf8();
    void allocate(size_t sz);
    void clear();
    void copy_from_buf(buf &src);
    u8 *ptr{};
    size_t sz{};
    u32 mask{};
};

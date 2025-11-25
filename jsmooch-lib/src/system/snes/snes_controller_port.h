//
// Created by . on 5/9/25.
//

#pragma once

#include "helpers/int.h"
namespace SNES::controller {
enum kinds {
    none,
    standard
};

struct core;

struct port {
    void latch(u8 val);
    u8 data();
    void connect(kinds in_kind, void *in_ptr);;

    void *ptr{};
    kinds kind = kinds::none;
};
}
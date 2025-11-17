//
// Created by . on 5/9/25.
//

#pragma once

#include "helpers/int.h"

enum SNES_controller_kinds {
    SNES_CK_none,
    SNES_CK_standard
};

struct SNES;

struct SNES_controller_port {
    void latch(u32 val);
    u32 data();
    void connect(SNES_controller_kinds in_kind, void *in_ptr);;

    void *ptr{};
    SNES_controller_kinds kind = SNES_CK_none;

};

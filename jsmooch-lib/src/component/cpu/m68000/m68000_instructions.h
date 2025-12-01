//
// Created by . on 5/15/24.
//

#pragma once

#include "helpers/int.h"
#include "helpers/debug.h"
#include "m68000_internal.h"

namespace M68k {
struct core;
struct ins_t;

typedef void (core::*ins_func)();
typedef void (*disassemble_t)(ins_t &ins, u32 &PC, jsm_debug_read_trace &rt, jsm_string &out);

struct ins_t {
    u16 opcode;
    u32 sz;
    u32 variant;
    ins_func exec;
    disassemble_t disasm;
    operand_modes operand_mode;
    EA ea[2];
};

extern ins_t decoded[65536];

}